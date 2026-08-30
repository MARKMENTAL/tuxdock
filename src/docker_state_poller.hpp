#pragma once

#include "docker_manager.hpp"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

class DockerStatePoller {
public:
    using ResultCallback = std::function<void(DockerManager::ListResult<DockerManager::ContainerInfo>,
                                              DockerManager::ListResult<DockerManager::ImageInfo>)>;
    using PostFunction = std::function<void(std::function<void()>)>;

    DockerStatePoller() = default;
    ~DockerStatePoller();

    DockerStatePoller(const DockerStatePoller&) = delete;
    DockerStatePoller& operator=(const DockerStatePoller&) = delete;

    void start(const DockerManager& docker,
               PostFunction post,
               ResultCallback callback);
    void stop();
    void trigger_now();

private:
    void loop();

    const DockerManager* docker_ = nullptr;
    PostFunction post_;
    ResultCallback callback_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    bool immediate_ = false;

    static constexpr std::chrono::seconds kInterval{5};
};
