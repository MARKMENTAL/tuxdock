#pragma once

#include <chrono>
#include <functional>

enum class StopProbe { Stopped, Running, Unknown };

StopProbe waitForStopped(const std::function<StopProbe()>& probe,
                    std::chrono::milliseconds timeout = std::chrono::milliseconds(2000),
                    std::chrono::milliseconds interval = std::chrono::milliseconds(100));
