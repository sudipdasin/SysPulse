// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/metrics.hpp"
#include <array>
#include <charconv>
#include <limits>
#include <cstdio>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace syspulse::native {
namespace {
using U = std::uint64_t;
using S = std::string_view;
constexpr U limit = std::numeric_limits<U>::max();
S word(S& s) noexcept {
    const auto first=s.find_first_not_of(" \t\r");
    if(first==S::npos){s={};return {};}
    s.remove_prefix(first); const auto end=s.find_first_of(" \t\r");
    if(end==S::npos){auto result=s;s={};return result;}
    auto result=s.substr(0,end);s.remove_prefix(end);return result;
}
std::optional<U> number(S s) noexcept {
    U n{};if(s.empty())return {};
    const auto r=std::from_chars(s.data(),s.data()+s.size(),n);
    if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size())return {};
    return n;
}
bool add(U& a,U b) noexcept {if(b>limit-a)return false;a+=b;return true;}
#if defined(__linux__)
template<std::size_t N> std::optional<S> readBounded(const char* path,std::array<char,N>& bytes) noexcept {
    auto* file=std::fopen(path,"rb");if(!file)return {};
    const auto n=std::fread(bytes.data(),1,bytes.size(),file);
    const bool valid=!std::ferror(file)&&n<bytes.size()&&std::feof(file);
    std::fclose(file);if(!valid)return {};return S(bytes.data(),n);
}
#endif
}
std::optional<CpuTicks> parseLinuxCpu(S input) noexcept {
    auto line=input.substr(0,input.find('\n'));if(word(line)!="cpu")return {};
    CpuTicks result;unsigned count=0;
    // guest and guest_nice already included in user and nice.
    for(;count<8;++count){const auto token=word(line);if(token.empty())break;
        const auto n=number(token);if(!n||!add(result.total,*n))return {};
        if((count==3||count==4)&&!add(result.idle,*n))return {};
    }
    if(count<4||result.idle>result.total)return {};
    return result;
}
std::optional<Memory> parseLinuxMemory(S input) noexcept {
    std::array<std::optional<U>,4> values{};
    constexpr std::array<S,4> keys={"MemTotal:","MemAvailable:","SwapTotal:","SwapFree:"};
    while(!input.empty()){
        const auto end=input.find('\n');auto line=input.substr(0,end);
        input=end==S::npos?S{}:input.substr(end+1);
        const auto key=word(line);
        for(std::size_t i=0;i<keys.size();++i)if(key==keys[i]){
            if(values[i])return {};
            auto n=number(word(line));
            if(!n||word(line)!="kB"||!word(line).empty())return {};
            values[i]=n;break;
        }
    }
    if(!values[0]||!*values[0]||!values[1]||*values[1]>*values[0])return {};
    Memory result{*values[0],*values[1],{},values[2]};
    if(values[2]&&values[3]&&*values[3]<=*values[2])result.swapUsedKiB=*values[2]-*values[3];
    else if(values[2]&&*values[2]==0&&!values[3])result.swapUsedKiB=0;
    return result;
}
std::optional<double> cpuPercent(CpuTicks before,CpuTicks after) noexcept {
    if(before.idle>before.total||after.idle>after.total||after.total<=before.total||after.idle<before.idle)return {};
    const auto elapsed=after.total-before.total, idle=after.idle-before.idle;
    if(idle>elapsed)return {};
    return 100.0*static_cast<double>(elapsed-idle)/static_cast<double>(elapsed);
}
Snapshot collect() noexcept {
    Snapshot result;
#if defined(__linux__)
    // Read the aggregate CPU line only, avoiding per-core allocation and process spawning.
    std::array<char,4096> line{};
    if(auto* f=std::fopen("/proc/stat","r")){
        if(std::fgets(line.data(),static_cast<int>(line.size()),f)){
            const S view(line.data());if(view.find('\n')!=S::npos)result.cpu=parseLinuxCpu(view);
        }
        std::fclose(f);
    }
    std::array<char,16384> memory{};
    if(auto data=readBounded("/proc/meminfo",memory))result.memory=parseLinuxMemory(*data);
#elif defined(_WIN32)
    FILETIME idle{},kernel{},user{};
    if(GetSystemTimes(&idle,&kernel,&user)){
        const auto ticks=[](FILETIME t){return (static_cast<U>(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
        U total=ticks(kernel);const U idleTicks=ticks(idle);
        if(add(total,ticks(user))&&idleTicks<=total)result.cpu=CpuTicks{total,idleTicks};
    }
    MEMORYSTATUSEX m{};m.dwLength=sizeof(m);
    if(GlobalMemoryStatusEx(&m)&&m.ullTotalPhys&&m.ullAvailPhys<=m.ullTotalPhys)
        result.memory=Memory{m.ullTotalPhys/1024,m.ullAvailPhys/1024,{},{}};
#endif
    return result;
}
}
