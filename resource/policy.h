// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <optional>

namespace eleos {
// Single-writer advisory controller. All percentages are permille.
// No sysfs writes, thread migration, process killing or hardware control.
class ResourcePolicy {
public:
    enum class Pressure { Normal, Elevated, Critical, Unknown };
    struct Sample {
        std::optional<unsigned> cpu_busy;
        std::uint64_t total_kib;
        std::uint64_t available_kib;
        bool thermal_allows_background;
    };
    struct Advice {
        std::optional<unsigned> cpu_forecast;
        Pressure memory;
        bool background_allowed;
    };
    Advice Update(const Sample& s) {
        if (!s.cpu_busy || *s.cpu_busy > 1000) {
            forecast_.reset();
        } else {
            // EWMA alpha=1/4, bounded integer arithmetic and rounding.
            forecast_ = forecast_ ? (3 * *forecast_ + *s.cpu_busy + 2) / 4
                                  : *s.cpu_busy;
        }
        if (!s.total_kib || s.available_kib > s.total_kib) {
            pressure_ = Pressure::Unknown;
        } else {
            // Divide before multiply to avoid overflow even on malformed input.
            const auto threshold = [&](unsigned percent) {
                return (s.total_kib / 100) * percent +
                       ((s.total_kib % 100) * percent + 99) / 100;
            };
            if (s.available_kib < threshold(5)) pressure_ = Pressure::Critical;
            else if (pressure_ == Pressure::Critical && s.available_kib < threshold(8)) {
                // Hysteresis: remain critical until at least 8% is available.
            } else if (s.available_kib < threshold(12)) pressure_ = Pressure::Elevated;
            else if (pressure_ == Pressure::Elevated && s.available_kib < threshold(16)) {
                // Hysteresis: elevated clears at 16%.
            } else pressure_ = Pressure::Normal;
        }
        // Safety and validity override optimization. Never authorize urgent work here.
        const bool allow = s.thermal_allows_background &&
            pressure_ == Pressure::Normal && forecast_ && *forecast_ < 650 &&
            s.cpu_busy && *s.cpu_busy < 800;
        return {forecast_, pressure_, allow};
    }
private:
    std::optional<unsigned> forecast_;
    Pressure pressure_ = Pressure::Unknown;
};
}  // namespace eleos
