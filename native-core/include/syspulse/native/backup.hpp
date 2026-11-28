// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
namespace syspulse::native {
struct CopyPlan {std::string source,destination;std::uint64_t bytes{};};
struct CopyOutcome {bool published{},durable{};std::uint64_t bytes{};std::string sha256,message;};
// A single user-owned regular file, copied into a new name without replacing any file.
class BackupReview {
    struct Impl;std::unique_ptr<Impl> p;
public:
    BackupReview();~BackupReview();BackupReview(const BackupReview&)=delete;BackupReview& operator=(const BackupReview&)=delete;
    std::optional<CopyPlan> prepare(const std::string& source,const std::string& destination);
    CopyOutcome apply();void cancel();
};
}
