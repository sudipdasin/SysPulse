// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/inspection.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace syspulse::native;
int checks=0;void check(bool ok,const char* label){++checks;if(!ok){std::cerr<<label<<'\n';std::exit(1);}}
int main(){
    auto c=parseCores("cpu 1 2 3 4\ncpu0 10 1 2 20 3 0 0 0 1 2\ncpu1 2 3 4 5\n");
    check(c.size()==2,"two cores");check(c[0].ticks.total==36&&c[0].ticks.idle==23,"core ticks");
    check(parseCores("cpu0 1 2 3 4\ncpu0 1 2 3 4").empty(),"duplicate core");check(parseCores("cpuX 1 2 3 4").empty(),"invalid core");
    auto d=parseDisks("8 0 sda 1 2 3 4 5 6 7 8 9 10 11\n8 1 sda1 0 0 0 0 0 0 0 0 0 0 0\n");
    check(d.size()==2,"disk and partition distinct");check(d[0].readSectors==3&&d[0].writeSectors==7&&d[0].busyMs==10,"disk indices");
    check(parseDisks("8 0 sda 1 2").empty(),"short disk");check(parseDisks("8 0 /bad 1 2 3 4 5 6 7 8 9 10 11").empty(),"disk name");
    auto n=parseNetwork("eth0: 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16\n");check(n.size()==1&&n[0].received==1&&n[0].sent==9,"network fields");
    check(parseNetwork("eth0: 1 2").empty(),"short network");check(parseNetwork("bad name: 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16").empty(),"network name");
    const std::string p="42 (name ) with spaces) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21";
    auto proc=parseProcess(p);check(proc&&proc->pid==42&&proc->parent==1&&proc->name=="name ) with spaces","process name");
    check(proc->cpuTicks==23&&proc->startTicks==19&&proc->rssPages==21,"process indices");check(!parseProcess("42 (bad) S 1"),"short process");check(!parseProcess("0 (bad) S 1"),"zero pid");
    check(counterRate(10,30,2)==10,"rate");check(!counterRate(30,10,2),"reset");check(!counterRate(10,30,0),"zero interval");check(!counterRate(10,30,std::numeric_limits<double>::infinity()),"invalid interval");
    check(memoryPressure({})==MemoryPressure::Unknown,"unknown memory");check(memoryPressure(Memory{100,9,{},{}})==MemoryPressure::High,"high");check(memoryPressure(Memory{100,10,{},{}})==MemoryPressure::Elevated,"elevated");check(memoryPressure(Memory{100,20,{},{}})==MemoryPressure::Normal,"normal");
    check(parseCores(std::string(1024*1024+1,'x')).empty(),"bounded input");
#if defined(__linux__)
    auto live=inspect(16);check(live.cpuAvailable&&!live.cores.empty(),"live cores");check(live.networkAvailable,"live network");check(live.processAvailable&&!live.processes.empty()&&live.processes.size()<=16,"live bounded processes");
    auto zero=inspect(0);check(zero.processes.empty()&&zero.processTruncated,"zero process cap");
#endif
    std::cout<<checks<<" inspection checks passed\n";
}
