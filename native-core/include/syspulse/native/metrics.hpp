// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

namespace syspulse::native {
struct CpuTicks { std::uint64_t total{}, idle{}; };
struct Memory { std::uint64_t totalKiB{}, availableKiB{}; std::optional<std::uint64_t> swapUsedKiB, swapTotalKiB; };
struct Snapshot { std::optional<CpuTicks> cpu; std::optional<Memory> memory; };
std::optional<CpuTicks> parseLinuxCpu(std::string_view input) noexcept;
std::optional<Memory> parseLinuxMemory(std::string_view input) noexcept;
std::optional<double> cpuPercent(CpuTicks before, CpuTicks after) noexcept;
Snapshot collect() noexcept; // read-only, missing data stays unknown
}
