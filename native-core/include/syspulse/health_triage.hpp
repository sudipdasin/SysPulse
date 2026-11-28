// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
#include <cstdint>
#include <vector>
namespace syspulse::triage {
enum class Severity { Unknown, Ok, Info, Warning, Critical };
enum class Check { AvailableMemory, RootStorage, SwapConfiguration };
struct Counters { std::uint64_t total, available; };
struct Finding { Check check; Severity severity; const char* advice; };
inline bool valid(const std::optional<Counters>& c) {
    return c && c->total > 0 && c->available <= c->total;
}
inline long double availablePercent(const Counters& c) {
    return 100.0L * c.available / c.total;
}
// Only accept measured counters. Missing readings never imply a healthy system.
// Thresholds are conservative policy defaults, not a comprehensive diagnosis.
inline std::vector<Finding> assess(std::optional<Counters> ram,
                                   std::optional<Counters> root,
                                   std::optional<std::uint64_t> swapTotal) {
    std::vector<Finding> out;
    if (!valid(ram)) out.push_back({Check::AvailableMemory, Severity::Unknown, "Inspect memory counters."});
    else if (availablePercent(*ram) < 5) out.push_back({Check::AvailableMemory, Severity::Critical, "Save work and inspect memory-heavy processes."});
    else if (availablePercent(*ram) < 15) out.push_back({Check::AvailableMemory, Severity::Warning, "Review memory and swap usage before tuning."});
    else out.push_back({Check::AvailableMemory, Severity::Ok, "Available memory is above this threshold."});
    if (!valid(root)) out.push_back({Check::RootStorage, Severity::Unknown, "Inspect filesystem counters."});
    else if (availablePercent(*root) <= 2) out.push_back({Check::RootStorage, Severity::Critical, "Review disk usage and back up before cleanup."});
    else if (availablePercent(*root) <= 10) out.push_back({Check::RootStorage, Severity::Warning, "Review cleanup candidates; do not delete automatically."});
    else out.push_back({Check::RootStorage, Severity::Ok, "Available root storage is above this threshold."});
    if (!swapTotal) out.push_back({Check::SwapConfiguration, Severity::Unknown, "Inspect swap configuration."});
    else if (*swapTotal == 0) out.push_back({Check::SwapConfiguration, Severity::Info, "No swap configured; review workload needs."});
    else out.push_back({Check::SwapConfiguration, Severity::Info, "Swap exists; this does not measure its pressure."});
    return out;
}
}
