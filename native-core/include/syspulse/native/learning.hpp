// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/native/diagnostics.hpp"
#include <cstdint>
#include <vector>
#include <optional>
namespace syspulse::native {
enum class LearningAction : unsigned { Thumbnails, FailedThumbnails, OwnMemory };
enum class LearningResult : unsigned { Applied, NoEffect, Failure };
enum class LearningContext : unsigned { Normal, Memory, Disk, Unknown };
struct LearnedOutcome { LearningContext context; LearningAction action; LearningResult result; std::uint64_t logicalBytes{}; std::optional<std::uint64_t> observedBefore{},observedAfter{}; };
struct LearningHistory { std::vector<DiagnosticSummary> analyses; std::vector<LearnedOutcome> outcomes; };
struct ActionHistory { unsigned attempts{},successes{},failures{},noEffect{}; bool demoted()const noexcept; };
std::string privateSignature(std::string_view message);
bool validSignature(std::string_view signature);
ActionHistory actionHistory(const LearningHistory&,LearningContext,LearningAction) noexcept;
std::string learningReport(const LearningHistory&);
std::optional<LearningHistory> readLearningHistory();
bool rememberAnalysis(const DiagnosticSummary&);
bool rememberOutcome(const LearnedOutcome&);
}
