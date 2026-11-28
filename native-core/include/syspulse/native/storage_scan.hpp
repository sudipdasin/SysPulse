// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>
namespace syspulse::native {
enum class FileCategory { Cache, Review, Archives, Media, Other, Count };
struct StorageEntry {std::string relative; std::uint64_t logical{}, allocated{}; FileCategory category{};};
struct StorageScan {std::array<std::uint64_t,5> categoryBytes{};std::vector<StorageEntry> largest;std::uint64_t logical{}, allocated{};unsigned entries{}, files{}, skipped{}, aliases{}, mounts{};bool available{}, complete{}, limited{}, cancelled{}, overflow{};};
struct ScanLimits {unsigned entries{25000}, largest{20};std::chrono::milliseconds time{3000};};
// Read-only scan. Does not follow links, cross mount boundaries or modify files.
StorageScan scanStorage(const std::string& root, const ScanLimits& limits, const std::atomic_bool& cancel);
StorageScan scanAccountHome(const ScanLimits& limits,const std::atomic_bool& cancel);
std::string storageReport(const StorageScan& scan);
}
