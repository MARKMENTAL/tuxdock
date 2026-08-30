#include "docker_manager.hpp"
#include "docker_engine_client.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

class MockEngineClient : public DockerEngineClient {
public:
    mutable std::string last_method;
    mutable std::string last_path;
    EngineResponse response;

    EngineResponse request(const std::string& method,
                           const std::string& path,
                           const std::string& /*body*/ = {},
                           int /*timeout_seconds*/ = 10) const override {
        last_method = method;
        last_path = path;
        return response;
    }
};

static std::unique_ptr<MockEngineClient> make_engine(const EngineResponse& response) {
    auto engine = std::make_unique<MockEngineClient>();
    engine->response = response;
    return engine;
}

void test_start_detached() {
    {
        auto engine = make_engine(EngineResponse{204, {}, {}});
        auto* raw = engine.get();
        DockerManager manager(std::move(engine));
        std::string message;
        assert(manager.startDetached("abc123", message));
        assert(message == "Container started in detached mode.");
        assert(raw->last_method == "POST");
        assert(raw->last_path == "/containers/abc123/start");
    }
    {
        auto engine = make_engine(EngineResponse{304, {}, {}});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(manager.startDetached("abc123", message));
        assert(message == "Container already started.");
    }
    {
        auto engine = make_engine(EngineResponse{500, {}, "engine error"});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(!manager.startDetached("abc123", message));
        assert(message == "engine error");
    }
}

void test_remove_container() {
    {
        auto engine = make_engine(EngineResponse{204, {}, {}});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(manager.removeContainer("abc123", message));
        assert(message == "Container removed.");
    }
    {
        auto engine = make_engine(EngineResponse{404, {}, "No such container"});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(manager.removeContainer("abc123", message));
        assert(message == "Container already removed.");
    }
    {
        auto engine = make_engine(EngineResponse{409, {}, "conflict"});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(!manager.removeContainer("abc123", message));
        assert(message == "conflict");
    }
}

void test_delete_image() {
    {
        auto engine = make_engine(EngineResponse{200, {}, {}});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(manager.deleteImage("sha256:abc", message));
        assert(message == "Image deleted.");
    }
    {
        auto engine = make_engine(EngineResponse{404, {}, "No such image"});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(manager.deleteImage("sha256:abc", message));
        assert(message == "Image already deleted.");
    }
    {
        auto engine = make_engine(EngineResponse{409, {}, "image in use"});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(!manager.deleteImage("sha256:abc", message));
        assert(message == "image in use");
    }
}

void test_stop_container() {
    {
        auto engine = make_engine(EngineResponse{204, {}, {}});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(manager.stopContainer("abc123", message, 10));
        assert(message == "Container stopped.");
    }
    {
        auto engine = make_engine(EngineResponse{304, {}, {}});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(manager.stopContainer("abc123", message, 10));
        assert(message == "Container stopped.");
    }
    {
        auto engine = make_engine(EngineResponse{404, {}, "No such container"});
        DockerManager manager(std::move(engine));
        std::string message;
        assert(manager.stopContainer("abc123", message, 10));
        assert(message == "Container stopped.");
    }
}

int main() {
    test_start_detached();
    test_remove_container();
    test_delete_image();
    test_stop_container();
    std::cout << "Docker manager idempotent operation tests passed\n";
}
