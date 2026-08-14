#pragma once

#include <string>

struct EngineResponse {
    int status_code = 0;
    std::string body;
    std::string error;

    bool ok() const { return status_code >= 200 && status_code < 300 && error.empty(); }
};

class DockerEngineClient {
public:
    explicit DockerEngineClient(std::string socket_path = "/var/run/docker.sock");

    bool checkConnection(std::string& error) const;

    EngineResponse request(const std::string& method,
                           const std::string& path,
                           const std::string& body = {}) const;

private:
    std::string socket_path_;
};
