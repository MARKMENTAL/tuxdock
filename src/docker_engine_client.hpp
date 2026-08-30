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
    virtual ~DockerEngineClient() = default;

    bool checkConnection(std::string& error) const;

    virtual EngineResponse request(const std::string& method,
                                   const std::string& path,
                                   const std::string& body = {},
                                   int timeout_seconds = 10) const;

private:
    std::string socket_path_;
};
