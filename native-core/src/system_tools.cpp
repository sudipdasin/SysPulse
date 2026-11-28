// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/system_tools.hpp"
#if defined(__linux__)
#include "syspulse/system_memory.hpp"
#include "syspulse/memory_relief.hpp"
#endif
namespace syspulse::native {
ZswapState parseZswap(std::string_view s) noexcept {if(s=="Y"||s=="1")return ZswapState::Enabled;if(s=="N"||s=="0")return ZswapState::Disabled;return ZswapState::Unknown;}
CompressionEvidence compressionEvidence(){
#if defined(__linux__)
    namespace m=syspulse::system_memory;
    auto s=m::readEnabled();std::ifstream swaps("/proc/swaps");return {s==m::State::Enabled?ZswapState::Enabled:s==m::State::Disabled?ZswapState::Disabled:ZswapState::Unknown,m::activeSwap(swaps),m::zramPresent()};
#else
    return {};
#endif
}
bool compressionChangeAllowed(const CompressionEvidence& e,bool enable) noexcept {return enable?e.state==ZswapState::Disabled&&e.activeSwap==true&&e.zram==false:e.state==ZswapState::Enabled;}
SelfMemoryResult trimOwnMemory(){
#if defined(__linux__)
    auto r=syspulse::relief::trimSelf();return {r.before,r.after,r.supported,r.released};
#else
    return {};
#endif
}
}
