// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/health.hpp"
#include <filesystem>
#include <map>
#include <limits>
namespace syspulse::native {
HealthEvidence collectHealth() {
    HealthEvidence evidence; evidence.memory=collect().memory;
#if defined(__linux__)
    std::error_code error; const auto root=std::filesystem::space("/",error);
    if(!error && root.capacity!=std::uintmax_t(-1) && root.available!=std::uintmax_t(-1) && root.capacity && root.available<=root.capacity && root.capacity<=UINT64_MAX)
        evidence.rootBytes=triage::Counters{static_cast<std::uint64_t>(root.capacity),static_cast<std::uint64_t>(root.available)};
#endif
    return evidence;
}
std::vector<triage::Finding> assessHealth(const HealthEvidence& evidence) {
    std::optional<triage::Counters> ram; std::optional<std::uint64_t> swap;
    if(evidence.memory){ram=triage::Counters{evidence.memory->totalKiB,evidence.memory->availableKiB};swap=evidence.memory->swapTotalKiB;}
    return triage::assess(ram,evidence.rootBytes,swap);
}
std::optional<std::vector<ProcessNode>> processForest(const std::vector<Process>& processes) {
    if(processes.size()>4096)return {};
    std::map<std::uint32_t,const Process*> index;
    for(const auto& row:processes)if(!row.pid || row.pid==row.parent || !index.emplace(row.pid,&row).second)return {};
    std::map<std::uint32_t,unsigned> state;
    for(const auto& entry:index){
        std::vector<std::uint32_t> path;auto current=entry.first;
        while(index.count(current) && state[current]!=2){
            if(state[current]==1)return {};
            state[current]=1;path.push_back(current);current=index.at(current)->parent;
        }
        for(auto pid:path)state[pid]=2;
    }
    std::vector<ProcessNode> out;out.reserve(index.size());
    for(const auto& entry:index){const auto parent=entry.second->parent;out.push_back({entry.first,index.count(parent)?std::optional<std::uint32_t>(parent):std::nullopt});}
    return out;
}
ReviewPlan planReview(const HealthEvidence& evidence) {
    ReviewPlan result;
    for(const auto& f:assessHealth(evidence)){
        if(f.severity==triage::Severity::Unknown)result.incomplete=true;
        if(f.severity==triage::Severity::Unknown || f.severity==triage::Severity::Critical || f.severity==triage::Severity::Warning)result.checks.push_back(f.check);
    }
    return result;
}
}
