// SPDX-License-Identifier: Apache-2.0
#include "memory_pressure.h"
#include <cassert>
#include <sstream>
#include <limits>
using namespace eleos;
int main() {
    std::istringstream valid("some avg10=0.20 avg60=0.10 avg300=0.00 total=1000\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=20\n");
    auto p=ReadMemoryPsi(valid); assert(p && p->some_us==1000 && p->full_us==20);
    for (const char* bad: {"some avg10=0.20 avg60=0.10 avg300=0.00 total=1000", "some avg10=101.00 avg60=0.00 avg300=0.00 total=100\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=0","some avg10=0.00 avg60=0.00 avg300=0.00 total=0\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=1"}) {
        std::istringstream in(bad); assert(!ReadMemoryPsi(in));
    }
    std::istringstream vm("pgmajfault 4\npgscan_direct 10\npgscan_kswapd 100\n");
    auto v=ReadVmActivity(vm); assert(v && v->major_fault==4 && v->direct_scan==10);
    std::istringstream badvm("pgmajfault 4\npgmajfault 6\npgscan_direct 10"); assert(!ReadVmActivity(badvm));
    using R=MemoryForecast::Risk;
    MemoryForecast f;
    assert(f.Update(MemoryCounters{1000,300},Psi{100,0},VmActivity{10,4},1000000).risk==R::Unknown);
    auto a=f.Update(MemoryCounters{1000,300},Psi{200,0},VmActivity{10,4},1250000);
    assert(a.risk==R::Normal && a.projected_permille==300 && a.stall_permille==0);
    a=f.Update(MemoryCounters{1000,140},Psi{250,0},VmActivity{11,5},1500000);
    assert(a.risk==R::Elevated && a.projected_permille==0);
    a=f.Update(MemoryCounters{1000,140},Psi{50250,30000},VmActivity{11,5},1750000);
    assert(a.risk==R::Critical && a.stall_permille==200);
    assert(f.Update(MemoryCounters{1000,40},{},{},2000000).risk==R::Critical);
    assert(f.Update(MemoryCounters{1000,900},{},{},2250000).risk==R::Unknown);
    assert(f.Update(MemoryCounters{1000,900},Psi{0,0},VmActivity{0,0},2500000).risk==R::Unknown);
    assert(f.Update(MemoryCounters{1000,900},Psi{1,0},VmActivity{0,0},2499999).risk==R::Unknown);
    const auto max=std::numeric_limits<std::uint64_t>::max();
    a=f.Update(MemoryCounters{max,max},Psi{1,0},VmActivity{0,0},3000000);
    assert(a.available_permille==1000 && a.risk==R::Unknown);
}
