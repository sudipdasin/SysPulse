// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <string>
#include <string_view>
#include <optional>
#include <cstddef>
namespace syspulse::native {
struct DiagnosticCategory {const char* name; const char* advice; bool critical;};
const std::array<DiagnosticCategory,14>& diagnosticCategories();
struct DiagnosticSummary {
    std::array<unsigned,14> counts{};
    unsigned records{}, malformed{}, duplicates{}, recovered{}, oversized{};
    bool available{}, partial{};
};
// Bounded journalctl JSON lines. Raw message text never leaves this parser.
DiagnosticSummary diagnoseJournal(std::string_view input, bool commandSucceeded, bool outputCapped);
std::string diagnosticReport(const DiagnosticSummary& summary);
}
