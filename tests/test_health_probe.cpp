// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/health_probe.hpp"
#include <iostream>
#include <limits>
using namespace syspulse::triage;
int main() {
    auto parse=[](const std::string& s){std::istringstream in(s);return parseMeminfo(in);};
    auto a=parse("MemTotal: 100 kB\nMemAvailable: 15 kB\nSwapTotal: 0 kB\n");
    if(!a.ram || a.ram->available!=15 || !a.swap || *a.swap!=0)return 1;
    if(parse("MemTotal: 100 kB\nMemAvailable: -1 kB\n").ram)return 2;
    if(parse("MemTotal: 100 kB\nMemAvailable: 101 kB\n").ram)return 3;
    if(parse("MemTotal: 100 kB\nMemAvailable: 15 kB\nMemAvailable: 20 kB\n").ram)return 4;
    if(parse("MemTotal: 100 kB\nMemAvailable: 15 MB\n").ram)return 5;
    if(parse("SwapTotal: 18446744073709551616 kB\n").swap)return 6;
    if(parse("SwapTotal: 20 kB trailing\n").swap)return 7;
    if(parse("SwapTotal: 10 kB\nSwapTotal: 20 kB\n").swap)return 8;
    auto root=rootCounters(100,20,10);
    if(!root || root->total!=90 || root->available!=10)return 9;
    if(rootCounters(0,0,0)||rootCounters(100,101,1)||rootCounters(100,10,11)||rootCounters(100,100,0))return 10;
    const auto max=std::numeric_limits<std::uint64_t>::max();
    root=rootCounters(max,max,max);if(!root || root->total!=max)return 11;
    if(parse(std::string(4097,'x')+"\nSwapTotal: 0 kB\n").swap)return 12;
    const auto live=measure();const auto findings=assess(live.ram,live.root,live.swap);
    if(findings.size()!=3)return 13;
    std::cout<<"13 health probe fixture and live shape checks passed\n";
}
