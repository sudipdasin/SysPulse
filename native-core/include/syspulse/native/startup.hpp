// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace syspulse::native {
struct StartupEntry {std::string name; bool disabled{};};
struct StartupInventory {std::vector<StartupEntry> entries; bool complete{}; std::string status;};
class StartupReview {
    struct Impl; std::unique_ptr<Impl> p;
public:
    StartupReview(); ~StartupReview(); StartupReview(const StartupReview&)=delete; StartupReview& operator=(const StartupReview&)=delete;
    StartupInventory scan(); bool prepare(const StartupEntry& entry); bool apply(); void cancel();
};
struct PackageAudit {unsigned installed{}, incomplete{}, residual{}, other{}; bool complete{};};
PackageAudit parsePackageStatus(std::string_view text);
PackageAudit auditPackages();
}
