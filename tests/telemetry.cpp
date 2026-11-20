// SPDX-License-Identifier: Apache-2.0
#include "telemetry.h"
#include <cassert>
#include <limits>
#include <sstream>
using namespace eleos;
int main() {
    std::istringstream a("cpu 10 2 8 70 10 1 2 0 9 1\ncpu0 1 2 3");
    std::istringstream b("cpu 30 2 18 130 20 1 2 0 9 1");
    auto before = ReadCpu(a), after = ReadCpu(b);
    assert(before && before->total == 103 && before->idle == 80);
    assert(after && after->total == 203 && after->idle == 150);
    assert(BusyPermille(*before, *after) == 300);
    assert(!BusyPermille(*after, *before));
    assert(!BusyPermille(*before, *before));
    assert(!BusyPermille({100, 30}, {110, 50}));
    assert(!BusyPermille({1, 1}, {std::numeric_limits<std::uint64_t>::max(), 1}));
    for (const auto* malformed : {"cpu0 1 2 3 4 5 6 7 8", "cpu 1 -2 3 4 5 6 7 8",
                                  "cpu 18446744073709551615 1 0 0 0 0 0 0", "cpu 1 2"}) {
        std::istringstream bad(malformed);
        assert(!ReadCpu(bad));
    }
    std::istringstream mem("MemTotal: 1000 kB\nMemFree: 1 kB\nMemAvailable: 150 kB\n");
    auto memory = ReadMemory(mem);
    assert(memory && memory->total_kib == 1000 && memory->available_kib == 150);
    for (const auto* malformed : {"MemTotal: 10 kB\n", "MemTotal: 1 kB\nMemAvailable: 2 kB",
                                  "MemTotal: 10 kB\nMemAvailable: -1 kB",
                                  "MemTotal: 10 kB\nMemTotal: 10 kB\nMemAvailable: 1 kB",
                                  "MemTotal: 10 MB\nMemAvailable: 0 kB"}) {
        std::istringstream bad(malformed);
        assert(!ReadMemory(bad));
    }
    ResourcePolicy policy;
    auto sample = MakeSample(before, after, memory);
    assert(sample.cpu_busy == 300 && !sample.thermal_allows_background);
    assert(!policy.Update(sample).background_allowed);
    auto unavailable = MakeSample({}, after, {});
    assert(!unavailable.cpu_busy && !policy.Update(unavailable).background_allowed);
}
