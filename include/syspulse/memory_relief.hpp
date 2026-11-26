// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <charconv>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <unistd.h>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
namespace syspulse::relief {
inline std::optional<std::uint64_t> residentKiB(std::istream& in, std::uint64_t pageBytes) {
    std::string total, resident;
    if (!(in >> total >> resident) || !pageBytes || total.size()>20 || resident.size()>20) return {};
    auto number=[](const std::string& s)->std::optional<std::uint64_t>{
        std::uint64_t n=0;
        const auto p=std::from_chars(s.data(),s.data()+s.size(),n);
        if(p.ec!=std::errc{} || p.ptr!=s.data()+s.size())return {};
        return n;
    };
    const auto t=number(total),r=number(resident);
    if(!t || !r || *r>*t || *r>std::numeric_limits<std::uint64_t>::max()/pageBytes)return {};
    return *r*pageBytes/1024;
}
inline std::optional<std::uint64_t> selfResidentKiB() {
    const auto page=::sysconf(_SC_PAGESIZE);
    if(page<=0)return {};
    std::ifstream stream("/proc/self/statm");
    return residentKiB(stream,static_cast<std::uint64_t>(page));
}
struct Outcome { std::optional<std::uint64_t> before, after; bool supported=false, released=false; };
// Only reclaim unused allocator pages owned by this process. Never drop OS caches,
// signal other applications or change system tuning. RSS is an approximate snapshot.
inline Outcome trimSelf() {
    Outcome out;out.before=selfResidentKiB();
#if defined(__GLIBC__)
    out.supported=true;out.released=::malloc_trim(0)!=0;
#endif
    out.after=selfResidentKiB();return out;
}
}
