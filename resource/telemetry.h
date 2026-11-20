// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <istream>
#include <optional>
#include "policy.h"

namespace eleos {
struct CpuCounters { std::uint64_t total; std::uint64_t idle; };
struct MemoryCounters { std::uint64_t total_kib; std::uint64_t available_kib; };
// Linux aggregate counters, not process or per-core data. Invalid or regressed
// counters fail closed; guest fields are already included in user/nice.
std::optional<CpuCounters> ReadCpu(std::istream& in);
std::optional<MemoryCounters> ReadMemory(std::istream& in);
std::optional<unsigned> BusyPermille(CpuCounters before, CpuCounters after);
ResourcePolicy::Sample MakeSample(std::optional<CpuCounters> before,
                                 std::optional<CpuCounters> after,
                                 std::optional<MemoryCounters> memory);
} // namespace eleos
