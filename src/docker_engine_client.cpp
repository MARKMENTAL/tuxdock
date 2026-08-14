#include "docker_engine_client.hpp"

#include "http_response_parser.hpp"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <sstream>
#include <utility>

DockerEngineClient::DockerEngineClient(std::string socket_path)
    : socket_path_(std::move(socket_path)) {}

bool DockerEngineClient::checkConnection(std::string& error) const {
    const EngineResponse response = request("GET", "/_ping");
    if (!response.error.empty()) {
        error = response.error;
        return false;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        error = "Docker Engine returned HTTP status " + std::to_string(response.status_code) + ".";
        return false;
    }
    if (response.body != "OK" && response.body != "OK\n" && response.body != "OK\r\n") {
        error = "Docker Engine returned an unexpected /_ping response.";
        return false;
    }
    return true;
}

EngineResponse DockerEngineClient::request(const std::string& method,
                                           const std::string& path,
                                           const std::string& body) const {
    EngineResponse response;
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        response.error = std::strerror(errno);
        return response;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (socket_path_.size() >= sizeof(address.sun_path)) {
        response.error = "Docker socket path is too long.";
        close(fd);
        return response;
    }
    std::strncpy(address.sun_path, socket_path_.c_str(), sizeof(address.sun_path) - 1);
    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        response.error = std::strerror(errno);
        close(fd);
        return response;
    }

    timeval timeout{};
    timeout.tv_sec = 10;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    std::ostringstream request;
    request << method << " " << path << " HTTP/1.1\r\n"
            << "Host: docker\r\n"
            << "Connection: close\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << body.size() << "\r\n\r\n"
            << body;
    const std::string wire = request.str();
    std::size_t sent = 0;
    while (sent < wire.size()) {
        const ssize_t count = write(fd, wire.data() + sent, wire.size() - sent);
        if (count <= 0) {
            response.error = std::strerror(errno);
            close(fd);
            return response;
        }
        sent += static_cast<std::size_t>(count);
    }

    std::string raw;
    char buffer[8192];
    ssize_t count = 0;
    while ((count = read(fd, buffer, sizeof(buffer))) > 0) {
        raw.append(buffer, static_cast<std::size_t>(count));
    }
    close(fd);
    if (count < 0) {
        response.error = std::strerror(errno);
        return response;
    }

    const auto parsed = parseHttpResponse(raw);
    response.status_code = parsed.status_code;
    response.body = parsed.body;
    response.error = parsed.error;
    return response;
}
