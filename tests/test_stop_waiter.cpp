#include "stop_waiter.hpp"

#include <cassert>
#include <chrono>
#include <iostream>

int main() {
    int calls = 0;
    const auto stopped = waitForStopped([&] {
        return ++calls < 3 ? StopProbe::Running : StopProbe::Stopped;
    },
                                        std::chrono::milliseconds(100),
                                        std::chrono::milliseconds(1));
    assert(stopped == StopProbe::Stopped);

    const auto timed_out = waitForStopped([] { return StopProbe::Unknown; },
                                          std::chrono::milliseconds(5),
                                          std::chrono::milliseconds(1));
    assert(timed_out == StopProbe::Unknown);
    int transient_calls = 0;
    const auto transient = waitForStopped([&] {
        ++transient_calls;
        if (transient_calls < 3) return StopProbe::Unknown;
        return StopProbe::Stopped;
    }, std::chrono::milliseconds(100), std::chrono::milliseconds(1));
    assert(transient == StopProbe::Stopped);
    // A failed refresh must not be interpreted as a stopped state.
    const auto preserved = waitForStopped([] { return StopProbe::Unknown; },
                                          std::chrono::milliseconds(3),
                                          std::chrono::milliseconds(1));
    assert(preserved == StopProbe::Unknown);
    std::cout << "Stop waiter tests passed\n";
}
