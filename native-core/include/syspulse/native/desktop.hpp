// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/native/health.hpp"
#include <array>
#include <string>
#include <vector>
namespace syspulse::native {
struct DesktopTool {const char* label;const char* path;};
const std::array<DesktopTool,12>& desktopTools() noexcept;
bool desktopAvailable(std::size_t index) noexcept;
class DesktopLauncher {
    std::vector<int> children_;
public:
    DesktopLauncher();
    void reap() noexcept;
    bool launch(std::size_t index);
};
std::string healthReport(const HealthEvidence&, const PressureEvidence&, const Inspection&);
struct ReportOutcome {bool published{},durable{};std::string path,message;};
// Explicit export only. No hostnames, process names, addresses or raw log lines.
ReportOutcome exportHealthReport(const std::string& text);
}
