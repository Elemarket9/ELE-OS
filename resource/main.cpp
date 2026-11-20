// SPDX-License-Identifier: Apache-2.0
#include "telemetry.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
auto Cpu() {
    std::ifstream in("/proc/stat");
    return eleos::ReadCpu(in);
}
auto Memory() {
    std::ifstream in("/proc/meminfo");
    return eleos::ReadMemory(in);
}
const char* Pressure(eleos::ResourcePolicy::Pressure p) {
    using P = eleos::ResourcePolicy::Pressure;
    switch (p) {
        case P::Normal: return "normal";
        case P::Elevated: return "elevated";
        case P::Critical: return "critical";
        case P::Unknown: return "unknown";
    }
    return "unknown";
}
}
int main(int argc, char**) {
    if (argc != 1) {
        std::cerr << "usage: ele-resource\n";
        return 2;
    }
    eleos::ResourcePolicy policy;
    auto before = Cpu();
    // Bounded collection: four intervals, no background daemon or device writes.
    for (unsigned i = 0; i < 4; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        auto after = Cpu();
        auto sample = eleos::MakeSample(before, after, Memory());
        auto advice = policy.Update(sample);
        std::cout << "sample=" << i + 1 << " cpu_busy_permille=";
        if (sample.cpu_busy) std::cout << *sample.cpu_busy; else std::cout << "unknown";
        std::cout << " cpu_forecast_permille=";
        if (advice.cpu_forecast) std::cout << *advice.cpu_forecast; else std::cout << "unknown";
        std::cout << " memory_pressure=" << Pressure(advice.memory)
                  << " thermal=unknown background_allowed=no\n";
        before = after;
    }
    return std::cout.good() ? 0 : 1;
}
