// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/native/metrics.hpp"
#include <string>
#include <vector>
namespace syspulse::native {
struct CpuCore { std::string id; CpuTicks ticks; };
struct Disk { std::string name; std::uint64_t readSectors{}, writeSectors{}, busyMs{}; };
struct Network { std::string name; std::uint64_t received{}, sent{}; };
struct Process { std::uint32_t pid{}, parent{}; std::string name; char state{}; std::uint64_t cpuTicks{}, startTicks{}; std::optional<std::uint64_t> rssPages; };
struct Inspection {
    Snapshot system;
    std::vector<CpuCore> cores;
    std::vector<Disk> disks;
    std::vector<Network> networks;
    std::vector<Process> processes;
    bool cpuAvailable{}, diskAvailable{}, networkAvailable{}, processAvailable{}, processTruncated{};
};
std::vector<CpuCore> parseCores(std::string_view text);
std::vector<Disk> parseDisks(std::string_view text);
std::vector<Network> parseNetwork(std::string_view text);
std::optional<Process> parseProcess(std::string_view text);
std::optional<double> counterRate(std::uint64_t before,std::uint64_t after,double seconds) noexcept;
Inspection inspect(std::size_t maxProcesses=256);
enum class MemoryPressure { Unknown, Normal, Elevated, High };
MemoryPressure memoryPressure(const std::optional<Memory>& memory) noexcept;
}
