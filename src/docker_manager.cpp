#include "docker_manager.hpp"

#include "process_runner.hpp"
#include "container_parser.hpp"
#include "stop_waiter.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

std::string apiError(const EngineResponse& response, const std::string& fallback) {
    if (!response.error.empty()) return response.error;
    return fallback;
}

}  // namespace

std::string DockerManager::processError(const std::string& fallback,
                                        const std::string& stderr_text) {
    return stderr_text.empty() ? fallback : stderr_text;
}

bool DockerManager::runProcess(const std::vector<std::string>& args,
                               std::string& message,
                               bool inherit_stdio) {
    ProcessOptions options;
    options.inherit_stdio = inherit_stdio;
    options.capture_stdout = !inherit_stdio;
    options.capture_stderr = !inherit_stdio;
    const ProcessResult result = ProcessRunner::run(args, options);
    if (!result.ok()) {
        message = processError("Docker command failed.", result.stderr_text);
        return false;
    }
    return true;
}

bool DockerManager::checkConnection(std::string& error) const {
    return engine_.checkConnection(error);
}

DockerManager::ListResult<DockerManager::ContainerInfo> DockerManager::getContainerList() const {
    ListResult<ContainerInfo> result;
    const EngineResponse response = engine_.request("GET", "/containers/json?all=true");
    if (!response.ok()) {
        result.error = apiError(response, "Could not list containers.");
        return result;
    }

    return parseContainerList(response.body);
}

DockerManager::ListResult<DockerManager::ImageInfo> DockerManager::getImageList() const {
    ListResult<ImageInfo> result;
    const EngineResponse response = engine_.request("GET", "/images/json");
    if (!response.ok()) {
        result.error = apiError(response, "Could not list images.");
        return result;
    }

    try {
        for (const auto& item : json::parse(response.body)) {
            const std::string id = item.value("Id", "");
            const auto tags = item.value("RepoTags", std::vector<std::string>{});
            if (id.empty()) continue;
            if (tags.empty()) result.items.emplace_back(id, "<untagged>");
            else for (const auto& tag : tags) result.items.emplace_back(id, tag);
        }
    } catch (const json::exception& error) {
        result.items.clear();
        result.error = std::string("Could not parse image data: ") + error.what();
    }
    return result;
}

bool DockerManager::pullImage(const std::string& image, std::string& message) const {
    if (image.empty()) {
        message = "Please provide an image name.";
        return false;
    }
    const bool ok = runProcess({"docker", "pull", image}, message);
    if (ok) message = "Image pulled successfully.";
    return ok;
}

bool DockerManager::createContainer(const std::string& name,
                                    const std::string& image,
                                    const std::vector<std::string>& ports,
                                    std::string& message) const {
    if (name.empty() || image.empty()) {
        message = "Please provide a container name and choose an image first.";
        return false;
    }

    // Resolve host path to tuxreaperd binary: prefer the current directory,
    // then fall back to the directory holding the tux-dock executable
    // (e.g. build/tuxreaperd next to build/tux-dock).
    std::filesystem::path hostReaperPath = std::filesystem::current_path() / "tuxreaperd";
    if (!std::filesystem::exists(hostReaperPath)) {
        std::error_code ec;
        const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec) {
            hostReaperPath = exe.parent_path() / "tuxreaperd";
        }
    }
    if (!std::filesystem::exists(hostReaperPath)) {
        message = "Failed to create container: tuxreaperd binary not found in the current directory or next to the tux-dock executable.";
        return false;
    }

    std::vector<std::string> args{"docker", "create", "--name", name};

    // Port mappings
    for (const auto& port : ports) {
        args.push_back("-p");
        args.push_back(port);
    }

    // Mount host tuxreaperd into container target path (read-only for security)
    args.push_back("-v");
    args.push_back(hostReaperPath.string() + ":/usr/local/bin/tuxreaperd:ro");

    // Target image and entry command with tuxreaperd as PID 1
    args.insert(args.end(), {image, "/usr/local/bin/tuxreaperd", "sleep", "infinity"});

    const bool ok = runProcess(args, message);
    if (ok) {
        message = "Container created with tuxreaperd PID 1 supervisor and ready to start.";
    }
    return ok;
}

bool DockerManager::startDetached(const std::string& id, std::string& message) const {
    const EngineResponse response = engine_.request("POST", "/containers/" + id + "/start");
    if (!response.ok()) {
        message = apiError(response, "Could not start that container.");
        return false;
    }
    message = "Container started in detached mode.";
    return true;
}

bool DockerManager::deleteImage(const std::string& id, std::string& message) const {
    const EngineResponse response = engine_.request("DELETE", "/images/" + id);
    if (!response.ok()) {
        message = apiError(response, "Could not delete that image.");
        return false;
    }
    message = "Image deleted.";
    return true;
}

bool DockerManager::stopContainer(const std::string& id,
                                  std::string& message,
                                  int timeout_seconds) const {
    if (id.empty()) {
        message = "Please provide a container ID.";
        return false;
    }
    if (timeout_seconds < 0) {
        message = "Stop timeout cannot be negative.";
        return false;
    }

    // Docker holds the /stop response until the container stops or the grace
    // period expires. Use a per-request HTTP timeout longer than the grace
    // period so the client does not race Docker's own timeout.
    const EngineResponse response = engine_.request(
        "POST",
        "/containers/" + id + "/stop?t=" + std::to_string(timeout_seconds),
        "",
        timeout_seconds + 5);

    if (response.ok() || response.status_code == 304 || response.status_code == 404) {
        message = "Container stopped.";
        return true;
    }

    message = apiError(response, "Could not stop that container.");
    return false;
}

bool DockerManager::removeContainer(const std::string& id, std::string& message) const {
    const EngineResponse response = engine_.request("DELETE", "/containers/" + id);
    if (!response.ok()) {
        message = apiError(response, "Could not remove that container.");
        return false;
    }
    message = "Container removed.";
    return true;
}

bool DockerManager::execShell(const std::string& id, std::string& message) const {
    const bool ok = runProcess({"docker", "exec", "-it", id, "/bin/sh"}, message, true);
    if (ok) message = "Shell session finished.";
    return ok;
}

bool DockerManager::execDetachedCommand(const std::string& id,
                                         const std::string& command,
                                         std::string& message) const {
    if (command.empty()) {
        message = "Please provide a command to run.";
        return false;
    }
    const bool ok = runProcess({"docker", "exec", "-d", id, "/bin/sh", "-c", command}, message);
    if (ok) message = "Command dispatched in detached mode.";
    return ok;
}
