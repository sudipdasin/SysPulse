// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/metrics.hpp"
#include <iostream>
#include <cstdlib>
using namespace syspulse::native;
int count=0;
void check(bool ok,const char* label){++count;if(!ok){std::cerr<<label<<'\n';std::exit(1);}}
int main(){
    auto cpu=parseLinuxCpu("cpu 100 5 20 200 10 3 2 1 99 88\ncpu0 0 0 0 0\n");
    check(cpu&&cpu->total==341&&cpu->idle==210,"guest excluded; iowait idle");
    check(parseLinuxCpu("cpu 1 2 3 4").has_value(),"four fields");
    check(!parseLinuxCpu("cpu0 1 2 3 4"),"aggregate only");
    check(!parseLinuxCpu("cpu 1 2 3"),"short");
    check(!parseLinuxCpu("cpu 1 x 3 4"),"malformed");
    check(!parseLinuxCpu("cpu -1 2 3 4"),"negative");
    check(!parseLinuxCpu("cpu 18446744073709551615 1 0 0"),"overflow");
    check(cpuPercent({100,60},{200,110})==50.0,"CPU delta");
    check(cpuPercent({100,60},{200,160})==0.0,"fully idle");
    check(cpuPercent({100,60},{200,60})==100.0,"fully busy");
    check(!cpuPercent({100,60},{100,60}),"zero elapsed");
    check(!cpuPercent({100,60},{99,60}),"counter reset");
    check(!cpuPercent({100,60},{200,59}),"idle reset");
    check(!cpuPercent({100,60},{200,170}),"invalid idle delta");
    check(!cpuPercent({100,101},{200,120}),"invalid baseline");
    auto m=parseLinuxMemory("MemTotal: 4000 kB\nMemAvailable: 1000 kB\nSwapTotal: 2000 kB\nSwapFree: 1500 kB\n");
    check(m&&m->totalKiB==4000&&m->availableKiB==1000&&m->swapUsedKiB==500,"memory and swap");
    check(!parseLinuxMemory("MemTotal: 4000 kB\n"),"missing available");
    check(!parseLinuxMemory("MemTotal: 0 kB\nMemAvailable: 0 kB\n"),"zero total");
    check(!parseLinuxMemory("MemTotal: 4 kB\nMemAvailable: 5 kB\n"),"available above total");
    check(!parseLinuxMemory("MemTotal: 4 MB\nMemAvailable: 2 kB\n"),"wrong unit");
    check(!parseLinuxMemory("MemTotal: 4 kB\nMemTotal: 5 kB\nMemAvailable: 2 kB\n"),"duplicate field");
    m=parseLinuxMemory("MemTotal: 4 kB\nMemAvailable: 0 kB\nSwapTotal: 0 kB\n");
    check(m&&m->swapUsedKiB==0,"zero swap");
    m=parseLinuxMemory("MemTotal: 4 kB\nMemAvailable: 2 kB\nSwapTotal: 2 kB\nSwapFree: 3 kB\n");
    check(m&&!m->swapUsedKiB,"inconsistent swap unknown");
    m=parseLinuxMemory("MemTotal: 4 kB\nMemAvailable: 2 kB\n");
    check(m&&!m->swapTotalKiB&&!m->swapUsedKiB,"missing swap unknown");
#if defined(__linux__) || defined(_WIN32)
    auto live=collect();check(live.cpu.has_value(),"live CPU");check(live.memory.has_value(),"live memory");
    check(live.memory->availableKiB<=live.memory->totalKiB,"live invariant");
#endif
    std::cout<<count<<" checks passed\n";
}
