#include "docker_state_poller.hpp"
#include "docker_manager.hpp"
#include "docker_engine_client.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>

class FakeEngineClient : public DockerEngineClient {
public:
    EngineResponse container_response;
    EngineResponse image_response;

    EngineResponse request(const std::string& method,
                           const std::string& path,
                           const std::string& /*body*/ = {},
                           int /*timeout_seconds*/ = 10) const override {
        (void)method;
        if (path == "/containers/json?all=true") return container_response;
        if (path == "/images/json") return image_response;
        return EngineResponse{};
    }
};

struct CallbackState {
    std::mutex mutex;
    std::condition_variable cv;
    int count = 0;
    std::size_t container_count = 0;
    std::size_t image_count = 0;
    bool last_containers_ok = false;
    bool last_images_ok = false;

    bool wait_for_count(int n, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, timeout, [this, n] { return count >= n; });
    }
};

static std::unique_ptr<DockerManager> make_manager(const std::string& containers_json,
                                                   const std::string& images_json) {
    auto engine = std::make_unique<FakeEngineClient>();
    engine->container_response = EngineResponse{200, containers_json, {}};
    engine->image_response = EngineResponse{200, images_json, {}};
    return std::make_unique<DockerManager>(std::move(engine));
}

void test_trigger_now() {
    auto manager = make_manager(
        R"([{"Id":"c1","Names":["/web"],"State":"running","Ports":[]}])",
        "[]");

    CallbackState state;
    DockerStatePoller poller;
    poller.start(
        *manager,
        [](std::function<void()> fn) { fn(); },
        [&state](auto containers, auto images) {
            std::lock_guard<std::mutex> lock(state.mutex);
            ++state.count;
            state.container_count = containers.items.size();
            state.image_count = images.items.size();
            state.last_containers_ok = containers.ok();
            state.last_images_ok = images.ok();
            state.cv.notify_all();
        });

    poller.trigger_now();
    assert(state.wait_for_count(1, std::chrono::seconds(2)));
    assert(state.container_count == 1);
    assert(state.image_count == 0);
    assert(state.last_containers_ok);
    assert(state.last_images_ok);

    poller.stop();
}

void test_periodic_polling() {
    auto manager = make_manager(
        R"([{"Id":"c1","Names":["/web"],"State":"running","Ports":[]}])",
        "[]");

    CallbackState state;
    DockerStatePoller poller;
    poller.start(
        *manager,
        [](std::function<void()> fn) { fn(); },
        [&state](auto containers, auto images) {
            std::lock_guard<std::mutex> lock(state.mutex);
            ++state.count;
            state.container_count = containers.items.size();
            state.image_count = images.items.size();
            state.cv.notify_all();
        });

    assert(state.wait_for_count(2, std::chrono::seconds(12)));
    assert(state.container_count == 1);
    assert(state.image_count == 0);

    poller.stop();
}

void test_stop_does_not_hang() {
    auto manager = make_manager("[]", "[]");
    DockerStatePoller poller;
    poller.start(*manager, [](std::function<void()>) {}, [](auto, auto) {});
    auto start = std::chrono::steady_clock::now();
    poller.stop();
    auto elapsed = std::chrono::steady_clock::now() - start;
    assert(elapsed < std::chrono::seconds(1));
}

int main() {
    test_trigger_now();
    test_periodic_polling();
    test_stop_does_not_hang();
    std::cout << "Docker state poller tests passed\n";
}
