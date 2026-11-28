// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
#include <string>
#include <string_view>
namespace syspulse::native {
// Fingerprint the exact simulated package actions and totals, not progress text.
// Missing/ambiguous summaries, removals and truncated plans fail closed.
std::optional<std::string> aptPlanToken(std::string_view operation,std::string_view output);
bool validPlanToken(std::string_view token) noexcept;
}
