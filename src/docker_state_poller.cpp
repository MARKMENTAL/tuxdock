#include "docker_state_poller.hpp"

DockerStatePoller::~DockerStatePoller() {
    stop();
}

void DockerStatePoller::start(const DockerManager& docker,
                              PostFunction post,
                              ResultCallback callback) {
    stop();
    docker_ = &docker;
    post_ = std::move(post);
    callback_ = std::move(callback);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = false;
        immediate_ = false;
    }
    thread_ = std::thread(&DockerStatePoller::loop, this);
}

void DockerStatePoller::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void DockerStatePoller::trigger_now() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        immediate_ = true;
    }
    cv_.notify_all();
}

void DockerStatePoller::loop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, kInterval, [this] { return stop_ || immediate_; });
            if (stop_) break;
            immediate_ = false;
        }

        if (!docker_ || !post_ || !callback_) continue;

        auto containers = docker_->getContainerList();
        auto images = docker_->getImageList();

        post_([callback = callback_,
               containers = std::move(containers),
               images = std::move(images)]() mutable {
            callback(std::move(containers), std::move(images));
        });
    }
}
