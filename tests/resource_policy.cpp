// SPDX-License-Identifier: Apache-2.0
#include "policy.h"
#include <cassert>
#include <limits>

int main() {
    using P = eleos::ResourcePolicy;
    P p;
    auto a = p.Update({200, 1000, 200, true});
    assert(a.cpu_forecast == 200 && a.memory == P::Pressure::Normal && a.background_allowed);
    a = p.Update({1000, 1000, 200, true});
    assert(a.cpu_forecast == 400 && !a.background_allowed);
    a = p.Update({200, 1000, 49, true});
    assert(a.memory == P::Pressure::Critical && !a.background_allowed);
    assert(p.Update({200, 1000, 79, true}).memory == P::Pressure::Critical);
    assert(p.Update({200, 1000, 80, true}).memory == P::Pressure::Elevated);
    assert(p.Update({200, 1000, 150, true}).memory == P::Pressure::Elevated);
    assert(p.Update({200, 1000, 160, true}).background_allowed);
    assert(!p.Update({200, 1000, 200, false}).background_allowed);
    a = p.Update({1001, 1000, 200, true});
    assert(!a.cpu_forecast && !a.background_allowed);
    a = p.Update({std::nullopt, 0, 0, true});
    assert(!a.cpu_forecast && a.memory == P::Pressure::Unknown && !a.background_allowed);
    assert(p.Update({100, 1000, 1001, true}).memory == P::Pressure::Unknown);
    const auto max = std::numeric_limits<std::uint64_t>::max();
    assert(p.Update({100, max, max, true}).background_allowed);
    for (unsigned load = 0; load <= 1000; ++load) {
        a = p.Update({load, 1000, 500, true});
        assert(a.cpu_forecast && *a.cpu_forecast <= 1000);
        if (load >= 800) assert(!a.background_allowed);
    }
}
