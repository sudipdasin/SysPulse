// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/health_triage.hpp"
#include <iostream>
#include <limits>
using namespace syspulse::triage;
int main() {
    auto check=[](std::optional<Counters> ram,std::optional<Counters> root,std::optional<std::uint64_t> swap,Severity r,Severity d,Severity s){
        const auto f=assess(ram,root,swap);
        return f.size()==3 && f[0].check==Check::AvailableMemory && f[1].check==Check::RootStorage && f[2].check==Check::SwapConfiguration && f[0].severity==r && f[1].severity==d && f[2].severity==s;
    };
    const auto max=std::numeric_limits<std::uint64_t>::max();
    if(!check({}, {}, {}, Severity::Unknown,Severity::Unknown,Severity::Unknown))return 1;
    if(!check(Counters{0,0},Counters{10,11},0,Severity::Unknown,Severity::Unknown,Severity::Info))return 2;
    if(!check(Counters{100,4},Counters{100,2},0,Severity::Critical,Severity::Critical,Severity::Info))return 3;
    if(!check(Counters{100,5},Counters{100,3},1,Severity::Warning,Severity::Warning,Severity::Info))return 4;
    if(!check(Counters{100,14},Counters{100,10},1,Severity::Warning,Severity::Warning,Severity::Info))return 5;
    if(!check(Counters{100,15},Counters{100,11},1,Severity::Ok,Severity::Ok,Severity::Info))return 6;
    if(!check(Counters{max,max},Counters{max,0},max,Severity::Ok,Severity::Critical,Severity::Info))return 7;
    if(!check(Counters{100,101},Counters{0,0},{},Severity::Unknown,Severity::Unknown,Severity::Unknown))return 8;
    std::cout << "8 health triage fixture cases passed\n";
}
