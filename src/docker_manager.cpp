#include "docker_manager.hpp"

#include "process_runner.hpp"
#include "container_parser.hpp"
#include "stop_waiter.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

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
    std::vector<std::string> args{"docker", "create", "--name", name};
    for (const auto& port : ports) {
        args.push_back("-p");
        args.push_back(port);
    }
    args.insert(args.end(), {image, "sleep", "infinity"});
    const bool ok = runProcess(args, message);
    if (ok) message = "Container created and ready to start when you are ready.";
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

bool DockerManager::stopContainer(const std::string& id, std::string& message) const {
    const auto stop_request = [this, &id] { return engine_.request("POST", "/containers/" + id + "/stop"); };
    const auto acceptable = [](const EngineResponse& response) {
        return response.status_code == 204 || response.status_code == 304 || response.status_code == 404 || response.ok();
    };
    const auto probe = [this, &id] {
        const EngineResponse state = engine_.request("GET", "/containers/" + id + "/json");
        if (!state.ok()) return StopProbe::Unknown;
        try {
            const bool running = nlohmann::json::parse(state.body)
                                     .value("State", nlohmann::json::object())
                                     .value("Running", true);
            return running ? StopProbe::Running : StopProbe::Stopped;
        } catch (const nlohmann::json::exception&) {
            return StopProbe::Unknown;
        }
    };

    const EngineResponse first = stop_request();
    const bool first_timed_out = first.error == "Docker Engine response timed out.";
    if (!acceptable(first) && !first_timed_out) {
        message = apiError(first, "Could not stop that container.");
        return false;
    }

    StopProbe state = first_timed_out ? StopProbe::Unknown : waitForStopped(probe);
    if (state != StopProbe::Stopped) {
        const EngineResponse retry = stop_request();
        const bool retry_timed_out = retry.error == "Docker Engine response timed out.";
        if (!acceptable(retry) && !retry_timed_out) {
            message = apiError(retry, "Could not confirm that container stopped.");
            return false;
        }
        state = waitForStopped(probe);
    }

    if (state != StopProbe::Stopped) {
        message = "Stop requested, but container state could not be confirmed.";
        return false;
    }
    message = "Container stopped.";
    return true;
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
