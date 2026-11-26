// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
namespace syspulse::memoryplan {
enum class Level { Unknown, Normal, Elevated, High };
struct Plan { Level level; bool ramKnown, pressureKnown; const char* advice; };
inline Plan assess(std::optional<std::uint64_t> total, std::optional<std::uint64_t> available, std::optional<double> stalls) {
    const bool ram=total&&available&&*total>0&&*available<=*total;
    const bool psi=stalls&&std::isfinite(*stalls)&&*stalls>=0&&*stalls<=100;
    const double fraction=ram?static_cast<double>(*available)/static_cast<double>(*total):1.0;
    if((ram&&fraction<0.05)||(psi&&*stalls>=10.0))
        return {Level::High,ram,psi,"High pressure: review largest memory consumers and save work before closing selected applications. Avoid dropping caches or disabling swap."};
    if((ram&&fraction<0.15)||(psi&&*stalls>=1.0))
        return {Level::Elevated,ram,psi,"Elevated pressure: review memory-heavy applications, startup entries and existing compressed swap before changing settings."};
    if(!ram||!psi)return {Level::Unknown,ram,psi,"Measurements incomplete. Review memory and process history; do not assume the system needs tuning."};
    return {Level::Normal,ram,psi,"No elevated pressure detected at this instant. Retain useful kernel caches and avoid unnecessary tuning."};
}
}
