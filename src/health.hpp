#pragma once
#include <istream>
#include <sstream>
#include <string>
#include <optional>
#include <cstdint>

namespace sp {
struct Memory { std::optional<std::uint64_t> total, available, swapTotal, swapFree; };
inline Memory parseMemory(std::istream& input) {
    Memory m; std::string line;
    while(std::getline(input,line)) {
        std::istringstream row(line); std::string name, unit, extra; std::uint64_t value{};
        if(!(row>>name) || !(row>>std::ws) || row.peek()<'0' || row.peek()>'9') continue;
        if(!(row>>value>>unit) || unit!="kB" || row>>extra) continue;
        if(name=="MemTotal:") m.total=value;
        else if(name=="MemAvailable:") m.available=value;
        else if(name=="SwapTotal:") m.swapTotal=value;
        else if(name=="SwapFree:") m.swapFree=value;
    }
    if(m.total && (!*m.total || (m.available && *m.available>*m.total))) m.available.reset();
    if(m.swapTotal && m.swapFree && *m.swapFree>*m.swapTotal) m.swapFree.reset();
    return m;
}
inline std::string memorySummary(const Memory& m) {
    if(!m.total || !*m.total || !m.available) return "RAM availability: unavailable";
    return "RAM available: "+std::to_string(*m.available/1024)+" / "+std::to_string(*m.total/1024)+" MiB";
}
inline std::string swapSummary(const Memory& m) {
    if(!m.swapTotal || !m.swapFree) return "Swap usage: unavailable";
    return "Swap used: "+std::to_string((*m.swapTotal-*m.swapFree)/1024)+" / "+std::to_string(*m.swapTotal/1024)+" MiB";
}
}
