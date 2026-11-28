// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/native/runtime.hpp"
#include "syspulse/health_triage.hpp"
namespace syspulse::native {
struct HealthEvidence {
    std::optional<Memory> memory;
    std::optional<triage::Counters> rootBytes;
};
HealthEvidence collectHealth();
std::vector<triage::Finding> assessHealth(const HealthEvidence& evidence);
struct ProcessNode { std::uint32_t pid; std::optional<std::uint32_t> parent; };
// Partial inventories produce roots for missing parents. Invalid or cyclic input is rejected.
std::optional<std::vector<ProcessNode>> processForest(const std::vector<Process>& processes);
struct ReviewPlan { std::vector<triage::Check> checks; bool incomplete{}; };
ReviewPlan planReview(const HealthEvidence& evidence);
}
