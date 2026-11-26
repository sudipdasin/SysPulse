// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/health_triage.hpp"
#include <array>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/statvfs.h>
namespace syspulse::triage {
struct Snapshot { std::optional<Counters> ram, root; std::optional<std::uint64_t> swap; };
inline Snapshot parseMeminfo(std::istream& in) {
    Snapshot result;
    const std::array<std::string,3> names={"MemTotal:","MemAvailable:","SwapTotal:"};
    std::array<std::optional<std::uint64_t>,3> values{};
    std::array<bool,3> seen{};
    std::string line; std::size_t lines=0;
    while(std::getline(in,line)) {
        if(++lines>1024 || line.size()>4096) return {};
        std::istringstream row(line); std::string key, number, unit, extra;
        if(!(row>>key)) continue;
        for(std::size_t i=0;i<names.size();++i) {
            if(key!=names[i]) continue;
            if(seen[i]) {values[i].reset(); break;}
            seen[i]=true;
            if(!(row>>number>>unit) || unit!="kB" || row>>extra) break;
            std::uint64_t value=0;
            const auto parsed=std::from_chars(number.data(),number.data()+number.size(),value);
            if(parsed.ec==std::errc{} && parsed.ptr==number.data()+number.size()) values[i]=value;
            break;
        }
    }
    if(in.bad() || (!in.eof() && in.fail())) return {};
    if(values[0] && values[1] && *values[0]>0 && *values[1]<=*values[0]) result.ram=Counters{*values[0],*values[1]};
    result.swap=values[2];
    return result;
}
inline std::optional<Counters> rootCounters(std::uint64_t blocks,std::uint64_t free,std::uint64_t available) {
    if(!blocks || free>blocks || available>free) return {};
    const auto usable=blocks-free+available;
    if(!usable) return {};
    return Counters{usable,available}; // exclude filesystem blocks reserved from normal users
}
inline Snapshot measure() {
    std::ifstream input("/proc/meminfo");
    auto result=parseMeminfo(input);
    struct statvfs fs{};
    if(statvfs("/",&fs)==0) result.root=rootCounters(fs.f_blocks,fs.f_bfree,fs.f_bavail);
    return result;
}
}
