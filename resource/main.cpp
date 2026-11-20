// SPDX-License-Identifier: Apache-2.0
#include "telemetry.h"
#include "memory_pressure.h"
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
auto Psi() {
    std::ifstream in("/proc/pressure/memory");
    return eleos::ReadMemoryPsi(in);
}
auto Vm() {
    std::ifstream in("/proc/vmstat");
    return eleos::ReadVmActivity(in);
}
const char* Risk(eleos::MemoryForecast::Risk risk) {
    using R=eleos::MemoryForecast::Risk;
    switch (risk) {
        case R::Normal: return "normal";
        case R::Elevated: return "elevated";
        case R::Critical: return "critical";
        case R::Unknown: return "unknown";
    }
    return "unknown";
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
    eleos::MemoryForecast forecast;
    // Bounded collection: four intervals, no background daemon or device writes.
    for (unsigned i = 0; i < 4; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        auto after = Cpu();
        auto memory = Memory();
        auto sample = eleos::MakeSample(before, after, memory);
        auto psi = Psi();
        auto vm = Vm();
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        auto risk = forecast.Update(memory, psi, vm, static_cast<std::uint64_t>(now));
        auto advice = policy.Update(sample);
        std::cout << "sample=" << i + 1 << " cpu_busy_permille=";
        if (sample.cpu_busy) std::cout << *sample.cpu_busy; else std::cout << "unknown";
        std::cout << " cpu_forecast_permille=";
        if (advice.cpu_forecast) std::cout << *advice.cpu_forecast; else std::cout << "unknown";
        std::cout << " memory_pressure=" << Pressure(advice.memory)
                  << " memory_risk=" << Risk(risk.risk) << " projected_available_permille=";
        if (risk.projected_permille) std::cout << *risk.projected_permille; else std::cout << "unknown";
        std::cout << " memory_stall_permille=";
        if (risk.stall_permille) std::cout << *risk.stall_permille; else std::cout << "unknown";
        std::cout << " thermal=unknown background_allowed=no\n";
        before = after;
    }
    return std::cout.good() ? 0 : 1;
}
