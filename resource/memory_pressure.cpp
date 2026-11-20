// SPDX-License-Identifier: Apache-2.0
#include "memory_pressure.h"
#include <algorithm>
#include <charconv>
#include <sstream>
#include <string>
#include <limits>

namespace eleos {
namespace {
using U = std::uint64_t;
bool Parse(const std::string& s, U& n) {
    if (s.empty()) return false;
    auto result = std::from_chars(s.data(), s.data()+s.size(), n);
    return result.ec == std::errc{} && result.ptr == s.data()+s.size();
}
// Linux prints average percentages as decimal numbers; validate but use
// cumulative microseconds for the measured, timestamped interval.
bool Average(const std::string& s, const std::string& prefix) {
    if (s.compare(0, prefix.size(), prefix) != 0) return false;
    const auto value = s.substr(prefix.size());
    const auto dot = value.find('.');
    U whole=0, fraction=0;
    return dot != std::string::npos && value.size()-dot==3 &&
           Parse(value.substr(0,dot), whole) && Parse(value.substr(dot+1), fraction) &&
           whole<=100 && fraction<=99 && (whole!=100 || fraction==0);
}
unsigned Ratio(U part, U all) {
    // All callers validate nonzero denominators and part <= all.
    return static_cast<unsigned>((static_cast<unsigned __int128>(part)*1000)/all);
}
}
std::optional<Psi> ReadMemoryPsi(std::istream& in) {
    std::optional<U> some, full;
    std::string line;
    while (std::getline(in,line)) {
        std::istringstream row(line);
        std::string label, a,b,c,d,extra;
        if (!(row>>label)) continue;
        auto* target=label=="some"? &some : label=="full"? &full : nullptr;
        if (!target) continue;
        U n=0;
        if (*target || !(row>>a>>b>>c>>d) || (row>>extra) ||
            !Average(a,"avg10=") || !Average(b,"avg60=") || !Average(c,"avg300=") ||
            d.compare(0,6,"total=")!=0 || !Parse(d.substr(6),n)) return {};
        *target=n;
    }
    if (in.bad() || !some || !full || *full>*some) return {};
    return Psi{*some,*full};
}
std::optional<VmActivity> ReadVmActivity(std::istream& in) {
    std::optional<U> direct, major;
    std::string line;
    while (std::getline(in,line)) {
        std::istringstream row(line);
        std::string key,val,extra;
        if (!(row>>key)) continue;
        auto* dest=key=="pgscan_direct"? &direct : key=="pgmajfault"? &major : nullptr;
        if (!dest) continue;
        U n=0;
        if (*dest || !(row>>val) || (row>>extra) || !Parse(val,n)) return {};
        *dest=n;
    }
    if(in.bad() || !direct || !major) return {};
    return VmActivity{*direct,*major};
}
MemoryForecast::Result MemoryForecast::Update(std::optional<MemoryCounters> mem,
        std::optional<Psi> psi, std::optional<VmActivity> vm, U now) {
    Result result{Risk::Unknown, {}, {}, {}};
    if (!mem || !mem->total_kib || mem->available_kib>mem->total_kib) {
        previous_.reset(); decline_=0; return result;
    }
    unsigned available=Ratio(mem->available_kib,mem->total_kib);
    result.available_permille=available;
    if (available<50) result.risk=Risk::Critical;
    if (!psi || !vm || psi->full_us>psi->some_us) {
        previous_.reset(); decline_=0; return result;
    }
    Baseline next{available,mem->total_kib,now,*psi,*vm};
    auto old=previous_;
    previous_=next;
    if (!old || now<=old->at || now-old->at>10000000 || now-old->at<100000 ||
        old->total_kib!=mem->total_kib || psi->some_us<old->psi.some_us ||
        psi->full_us<old->psi.full_us || vm->direct_scan<old->vm.direct_scan ||
        vm->major_fault<old->vm.major_fault) { decline_=0; return result; }
    U interval=now-old->at, some=psi->some_us-old->psi.some_us, full=psi->full_us-old->psi.full_us;
    if (some>interval || full>some) { decline_=0;return result; }
    unsigned stall=Ratio(some,interval), blocked=Ratio(full,interval);
    // Normalize available-memory trend to a 250ms interval, then project
    // one second ahead. EWMA alpha=1/4: constant storage and work.
    U drop=old->available>available?old->available-available:0;
    unsigned rate=static_cast<unsigned>(std::min<U>(1000,drop*250000/interval));
    decline_=(3*decline_+rate+2)/4;
    unsigned projected=available>4*decline_?available-4*decline_:0;
    result.projected_permille=projected;
    result.stall_permille=stall;
    bool direct=vm->direct_scan>old->vm.direct_scan, faults=vm->major_fault>old->vm.major_fault;
    if (available<50 || blocked>=100) result.risk=Risk::Critical;
    else if (projected<120 || stall>=100 || (direct && faults)) result.risk=Risk::Elevated;
    else result.risk=Risk::Normal;
    return result;
}
}
