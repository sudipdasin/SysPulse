// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/native/inspection.hpp"
#include <chrono>
#include <memory>
namespace syspulse::native {
struct CleanSummary { std::size_t files{}; std::uint64_t bytes{}; std::string status; };
struct CleanResult { std::size_t removed{}, skipped{}; std::uint64_t bytes{}; std::string status; bool completed{}; std::optional<std::uint64_t> availableBefore,availableAfter; };
class CacheReview {
    struct Impl; std::unique_ptr<Impl> p;
public:
    CacheReview(); ~CacheReview(); CacheReview(const CacheReview&)=delete; CacheReview& operator=(const CacheReview&)=delete;
    CleanSummary scan(bool failedMarkers=false); CleanResult apply(); void cancel();
};
class ProcessReview {
    int fd=-1; std::uint32_t pid{}; std::uint64_t start{}; std::chrono::steady_clock::time_point expiry{};
public:
    ProcessReview()=default; ~ProcessReview(); ProcessReview(const ProcessReview&)=delete; ProcessReview& operator=(const ProcessReview&)=delete;
    bool prepare(const Process& row); bool terminate(); void cancel();
};
}
