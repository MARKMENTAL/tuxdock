#include "stop_waiter.hpp"

#include <cassert>
#include <iostream>

int main() {
    int stop_requests = 0;
    int probes = 0;
    const auto first = [&] {
        ++stop_requests;
        return stop_requests == 1 ? StopProbe::Unknown : StopProbe::Stopped;
    };
    const auto state = waitForStopped([&] {
        ++probes;
        return probes < 2 ? StopProbe::Unknown : StopProbe::Stopped;
    }, std::chrono::milliseconds(10), std::chrono::milliseconds(1));
    assert(state == StopProbe::Stopped);
    assert(first() == StopProbe::Unknown);
    assert(first() == StopProbe::Stopped);
    assert(stop_requests == 2);
    std::cout << "Stop sequence tests passed\n";
}
