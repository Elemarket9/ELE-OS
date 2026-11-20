// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <istream>
#include <optional>
#include "telemetry.h"

namespace eleos {
// Linux PSI "some"/"full" cumulative microseconds and VM counters.
// A missing or malformed source is never converted into a zero-stall reading.
struct Psi { std::uint64_t some_us, full_us; };
struct VmActivity { std::uint64_t direct_scan, major_fault; };
std::optional<Psi> ReadMemoryPsi(std::istream&);
std::optional<VmActivity> ReadVmActivity(std::istream&);
class MemoryForecast {
public:
    enum class Risk { Unknown, Normal, Elevated, Critical };
    struct Result {
        Risk risk;
        std::optional<unsigned> available_permille, projected_permille, stall_permille;
    };
    Result Update(std::optional<MemoryCounters>, std::optional<Psi>,
                  std::optional<VmActivity>, std::uint64_t monotonic_us);
private:
    struct Baseline {
        unsigned available;
        std::uint64_t total_kib, at;
        Psi psi;
        VmActivity vm;
    };
    std::optional<Baseline> previous_;
    unsigned decline_ = 0;
};
}
