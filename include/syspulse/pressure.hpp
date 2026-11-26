// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
namespace syspulse::pressure {
struct Sample { double avg10=0, avg60=0, avg300=0; std::uint64_t total=0; };
struct Snapshot { std::optional<Sample> some, full; };
inline std::optional<double> percentage(const std::string& value) {
    if(value.empty() || value.size()>16) return {};
    unsigned whole=0, fraction=0, scale=1; bool dot=false, digit=false, fractional=false;
    for(char c:value) {
        if(c=='.' && !dot && digit) {dot=true;continue;}
        if(c<'0'||c>'9')return {};
        digit=true;
        if(!dot){whole=whole*10+unsigned(c-'0');if(whole>100)return {};}
        else {fractional=true;if(scale>=1000000)return {};scale*=10;fraction=fraction*10+unsigned(c-'0');}
    }
    if(!digit || (dot&&!fractional) || (whole==100&&fraction))return {};
    return whole+double(fraction)/scale;
}
inline Snapshot parse(std::istream& input) {
    Snapshot result; std::array<bool,2> seen{}; std::string line; unsigned lines=0;
    while(std::getline(input,line)) {
        if(++lines>16 || line.size()>512)return {};
        std::istringstream row(line);std::string kind,a,b,c,d,extra;
        if(!(row>>kind))continue;
        if(kind!="some"&&kind!="full")continue;
        const unsigned i=kind=="some"?0:1;auto& slot=i==0?result.some:result.full;
        if(seen[i]){slot.reset();continue;}seen[i]=true;
        if(!(row>>a>>b>>c>>d)||row>>extra)continue;
        if(a.rfind("avg10=",0)||b.rfind("avg60=",0)||c.rfind("avg300=",0)||d.rfind("total=",0))continue;
        auto x=percentage(a.substr(6)),y=percentage(b.substr(6)),z=percentage(c.substr(7));
        const auto raw=d.substr(6);std::uint64_t total=0;
        const auto p=std::from_chars(raw.data(),raw.data()+raw.size(),total);
        if(!x||!y||!z||p.ec!=std::errc{}||p.ptr!=raw.data()+raw.size())continue;
        slot=Sample{*x,*y,*z,total};
    }
    if(input.bad()||(!input.eof()&&input.fail()))return {};
    return result;
}
inline std::array<Snapshot,3> measure() {
    std::array<Snapshot,3> result{};unsigned i=0;
    for(const auto* path:{"/proc/pressure/cpu","/proc/pressure/memory","/proc/pressure/io"}) {
        std::ifstream input(path);result[i++]=parse(input);
    }
    return result;
}
}
