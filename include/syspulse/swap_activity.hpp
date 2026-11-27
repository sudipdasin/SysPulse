// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/bounded_line.hpp"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
namespace syspulse::swap_activity {
struct Counters { std::uint64_t in, out; };
struct Rates { double inKiB, outKiB; };
inline std::optional<Counters> parse(std::istream& input) {
    std::optional<std::uint64_t> in,out;
    std::string line; unsigned count=0;
    for(;;) {
        const auto state=boundedLine<4096>(input,line);
        if(state==LineState::End)break;
        if(state!=LineState::Line||++count>4096)return {};
        std::istringstream row(line);std::string key,value,extra;
        if(!(row>>key)||(key!="pswpin"&&key!="pswpout"))continue;
        auto& target=key=="pswpin"?in:out;
        if(target||!(row>>value)||row>>extra)return {};
        std::uint64_t n=0;const auto p=std::from_chars(value.data(),value.data()+value.size(),n);
        if(p.ec!=std::errc{}||p.ptr!=value.data()+value.size())return {};
        target=n;
    }
    if(input.bad()||(!input.eof()&&input.fail())||!in||!out)return {};
    return Counters{*in,*out};
}
inline std::optional<Counters> read() { std::ifstream input("/proc/vmstat");return parse(input); }
inline std::optional<Rates> rates(Counters before,Counters after,double seconds,long pageBytes) {
    if(!std::isfinite(seconds)||seconds<0.1||seconds>3600||pageBytes<1024||pageBytes>1048576||pageBytes%1024||after.in<before.in||after.out<before.out)return {};
    const double factor=static_cast<double>(pageBytes)/1024.0/seconds;
    return Rates{static_cast<double>(after.in-before.in)*factor,static_cast<double>(after.out-before.out)*factor};
}
inline long pageSize(){return ::sysconf(_SC_PAGESIZE);}
}
