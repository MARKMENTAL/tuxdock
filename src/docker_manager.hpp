#pragma once

#include "docker_engine_client.hpp"

#include <string>
#include <utility>
#include <vector>

class DockerManager {
public:
    struct ContainerInfo {
        std::string id;
        std::string name;
        std::string status;
        std::string ports;
        bool running = false;
    };

    using ImageInfo = std::pair<std::string, std::string>;

    std::vector<ContainerInfo> getContainerList() const;
    std::vector<ImageInfo> getImageList() const;

    bool pullImage(const std::string& image, std::string& message) const;
    bool runContainerInteractive(const std::string& image,
                                 const std::vector<std::string>& ports,
                                 std::string& message) const;
    bool startInteractive(const std::string& containerId, std::string& message) const;
    bool startDetached(const std::string& containerId, std::string& message) const;
    bool deleteImage(const std::string& imageId, std::string& message) const;
    bool stopContainer(const std::string& containerId, std::string& message) const;
    bool removeContainer(const std::string& containerId, std::string& message) const;
    bool execShell(const std::string& containerId, std::string& message) const;
    bool execDetachedCommand(const std::string& containerId,
                             const std::string& command,
                             std::string& message) const;
    bool spinUpMySQL(const std::string& port,
                     const std::string& password,
                     const std::string& version,
                     std::string& message) const;
    bool createDockerfile(const std::string& baseImage,
                          const std::string& bashScriptPath,
                          std::string outputFile,
                          const std::string& imageName,
                          std::string& message) const;

private:
    DockerEngineClient engine_;

    static std::string processError(const std::string& fallback,
                                    const std::string& stderr_text);
    static bool runProcess(const std::vector<std::string>& args,
                           std::string& message,
                           bool inherit_stdio = false);
};
