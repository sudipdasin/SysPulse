// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/native/inspection.hpp"
#include <array>
#include <chrono>
#include <deque>
#include <map>
namespace syspulse::native {
struct PressureLine { double avg10{}, avg60{}, avg300{}; std::uint64_t totalUs{}; };
struct Pressure { PressureLine some; std::optional<PressureLine> full; };
std::optional<Pressure> parsePressure(std::string_view input);
struct PressureEvidence { std::optional<Pressure> cpu, memory, io; };
PressureEvidence collectPressure();
struct ProcessIdentity { std::uint32_t pid{}; std::uint64_t startTicks{}; };
std::optional<double> processTicksPerSecond(const Process& before,const Process& after,double seconds) noexcept;
enum class IdentityReview { Match, Missing, Ambiguous, Changed, Protected };
IdentityReview reviewProcess(ProcessIdentity target,const std::vector<Process>& current,std::uint32_t ownPid) noexcept;
struct Sample { using Clock=std::chrono::steady_clock; Clock::time_point time; Snapshot system; std::vector<CpuCore> cores; };
struct Rates { double seconds{}; std::optional<double> cpu, memoryUsed; std::map<std::string,double> cores; };
std::optional<Rates> sampleRates(const Sample& before,const Sample& after);
Sample captureSample();
class History {
    std::size_t capacity_; std::deque<Rates> rows_;
public:
    explicit History(std::size_t capacity=120);
    void push(Rates value); // aggregate history only; per-core rates remain in the current sample
    const std::deque<Rates>& rows() const noexcept {return rows_;}
};
}
