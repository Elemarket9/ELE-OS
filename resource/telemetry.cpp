// SPDX-License-Identifier: Apache-2.0
#include "telemetry.h"
#include <charconv>
#include <limits>
#include <sstream>
#include <string>

namespace eleos {
namespace {
using U = std::uint64_t;
constexpr U kMax = std::numeric_limits<U>::max();
bool Number(const std::string& token, U& value) {
    if (token.empty()) return false;
    const auto end = token.data() + token.size();
    auto result = std::from_chars(token.data(), end, value);
    return result.ec == std::errc{} && result.ptr == end;
}
}
std::optional<CpuCounters> ReadCpu(std::istream& in) {
    std::string line, label, token;
    if (!std::getline(in, line)) return {};
    std::istringstream fields(line);
    if (!(fields >> label) || label != "cpu") return {};
    U sum = 0, idle = 0;
    for (unsigned i = 0; i < 8; ++i) {
        U n = 0;
        if (!(fields >> token) || !Number(token, n) || n > kMax - sum) return {};
        sum += n;
        if (i == 3 || i == 4) idle += n;
    }
    return CpuCounters{sum, idle};
}
std::optional<MemoryCounters> ReadMemory(std::istream& in) {
    std::string line, label, token, unit, extra;
    std::optional<U> total, available;
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        if (!(fields >> label)) continue;
        auto* target = label == "MemTotal:" ? &total :
                       label == "MemAvailable:" ? &available : nullptr;
        if (!target) continue;
        U n = 0;
        if (*target || !(fields >> token >> unit) || unit != "kB" ||
            !Number(token, n) || (fields >> extra)) return {};
        *target = n;
    }
    if (in.bad() || !total || !available || !*total || *available > *total) return {};
    return MemoryCounters{*total, *available};
}
std::optional<unsigned> BusyPermille(CpuCounters a, CpuCounters b) {
    if (a.idle > a.total || b.idle > b.total || b.total <= a.total || b.idle < a.idle) return {};
    const U total = b.total - a.total, idle = b.idle - a.idle;
    if (idle > total) return {};
    const U busy = total - idle;
    if (busy > kMax / 1000) return {};
    return static_cast<unsigned>((busy * 1000) / total);
}
ResourcePolicy::Sample MakeSample(std::optional<CpuCounters> a,
                                 std::optional<CpuCounters> b,
                                 std::optional<MemoryCounters> memory) {
    // No trusted Thermal HAL decision at this layer. Never authorize background work.
    return {a && b ? BusyPermille(*a, *b) : std::nullopt,
            memory ? memory->total_kib : 0, memory ? memory->available_kib : 0, false};
}
} // namespace eleos
