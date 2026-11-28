// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
#include <string_view>
#include <cstdint>
namespace syspulse::native {
enum class ZswapState { Unknown, Disabled, Enabled };
ZswapState parseZswap(std::string_view) noexcept;
struct CompressionEvidence { ZswapState state{ZswapState::Unknown}; std::optional<bool> activeSwap, zram; };
CompressionEvidence compressionEvidence();
bool compressionChangeAllowed(const CompressionEvidence&,bool enable) noexcept;
struct SelfMemoryResult { std::optional<std::uint64_t> before,after; bool supported{},released{}; };
SelfMemoryResult trimOwnMemory();
}
