#include "stop_waiter.hpp"

#include <thread>

StopProbe waitForStopped(const std::function<StopProbe()>& probe,
                    std::chrono::milliseconds timeout,
                    std::chrono::milliseconds interval) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (probe() == StopProbe::Stopped) return StopProbe::Stopped;
        std::this_thread::sleep_for(interval);
    }
    return probe();
}
