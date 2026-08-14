#include "docker_manager.hpp"

#include "process_runner.hpp"
#include "container_parser.hpp"

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

bool DockerManager::runContainerInteractive(const std::string& image,
                                            const std::vector<std::string>& ports,
                                            std::string& message) const {
    if (image.empty()) {
        message = "Please choose an image first.";
        return false;
    }
    std::vector<std::string> args{"docker", "run", "-it"};
    for (const auto& port : ports) {
        args.push_back("-p");
        args.push_back(port);
    }
    args.insert(args.end(), {image, "/bin/sh"});
    const bool ok = runProcess(args, message, true);
    if (ok) message = "Interactive container session finished.";
    return ok;
}

bool DockerManager::startInteractive(const std::string& id, std::string& message) const {
    const bool ok = runProcess({"docker", "start", "-ai", id}, message, true);
    if (ok) message = "Interactive container session finished.";
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
    const EngineResponse response = engine_.request("POST", "/containers/" + id + "/stop");
    if (!response.ok()) {
        message = apiError(response, "Could not stop that container.");
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

bool DockerManager::spinUpMySQL(const std::string& port,
                                const std::string& password,
                                const std::string& version,
                                std::string& message) const {
    const bool ok = runProcess({"docker", "run", "-p", port, "--name", "mysql-container",
                                "-e", "MYSQL_ROOT_PASSWORD=" + password, "-d", "mysql:" + version}, message);
    if (ok) message = "MySQL container launched.";
    return ok;
}

bool DockerManager::createDockerfile(const std::string& baseImage,
                                     const std::string& bashScriptPath,
                                     std::string outputFile,
                                     const std::string& imageName,
                                     std::string& message) const {
    if (baseImage.empty() || bashScriptPath.empty()) {
        message = "Base image and script path are required.";
        return false;
    }
    if (!std::filesystem::exists(bashScriptPath)) {
        message = "Could not find that script file.";
        return false;
    }
    if (outputFile.empty()) outputFile = "Dockerfile";
    std::ifstream scriptFile(bashScriptPath);
    std::ofstream dockerfile(outputFile);
    if (!scriptFile.is_open() || !dockerfile.is_open()) {
        message = "Could not open one of the files.";
        return false;
    }
    dockerfile << "FROM " << baseImage << "\nWORKDIR /app\n\n# Auto-generated by Tux-Dock\n";
    std::string line;
    while (std::getline(scriptFile, line)) {
        if (!line.empty() && line.rfind("#", 0) != 0) dockerfile << "RUN " << line << "\n";
    }
    dockerfile << "\nCMD [\"/bin/bash\"]\n";
    if (imageName.empty()) {
        message = "Dockerfile created. Build skipped because no image name was provided.";
        return true;
    }
    const bool ok = runProcess({"docker", "build", "-t", imageName, "-f", outputFile, "."}, message);
    if (ok) message = "Dockerfile created and image build completed.";
    return ok;
}
