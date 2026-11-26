// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/health_probe.hpp"
#include "syspulse/pressure.hpp"
#include <algorithm>
#include <cmath>
namespace syspulse::care {
enum class Route { Memory, Storage, Swap, Cpu, Io };
struct Item { const char* title; triage::Severity severity; Route route; const char* advice; };
inline int priority(triage::Severity s) {
    using S=triage::Severity;
    switch(s){case S::Critical:return 0;case S::Warning:return 1;case S::Unknown:return 2;case S::Info:return 3;case S::Ok:return 4;}
    return 2;
}
inline std::vector<Item> plan(const triage::Snapshot& counters,const std::array<pressure::Snapshot,3>& pressure) {
    using S=triage::Severity;using C=triage::Check;
    std::vector<Item> items;
    for(const auto& f:triage::assess(counters.ram,counters.root,counters.swap)) {
        if(f.check==C::AvailableMemory)items.push_back({"Available RAM",f.severity,Route::Memory,f.advice});
        else if(f.check==C::RootStorage)items.push_back({"Root filesystem space",f.severity,Route::Storage,f.advice});
        else items.push_back({"Swap configuration",f.severity,Route::Swap,f.advice});
    }
    const std::array<const char*,3> names={"CPU stalls","Memory stalls","I/O stalls"};
    const std::array<Route,3> routes={Route::Cpu,Route::Memory,Route::Io};
    const std::array<double,3> warn={10,1,1},critical={40,10,10};
    for(unsigned i=0;i<3;++i) {
        const auto& sample=pressure[i].some;
        S severity=S::Unknown;const char* advice="Pressure measurement unavailable; inspect resource history.";
        if(sample&&std::isfinite(sample->avg10)&&sample->avg10>=0&&sample->avg10<=100){
            if(sample->avg10>=critical[i]){severity=S::Critical;advice="High recent stall time. Save work and inspect resource consumers; do not blindly tune the system.";}
            else if(sample->avg10>=warn[i]){severity=S::Warning;advice="Elevated recent stall time. Inspect resource history and workloads.";}
            else {severity=S::Ok;advice="Recent stall time below the advisory threshold; not a full health diagnosis.";}
        }
        items.push_back({names[i],severity,routes[i],advice});
    }
    std::stable_sort(items.begin(),items.end(),[](const Item& a,const Item& b){return priority(a.severity)<priority(b.severity);});
    return items;
}
}
