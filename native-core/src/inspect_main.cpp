// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/inspection.hpp"
#include <iostream>
int main(){
    using namespace syspulse::native;
    const auto r=inspect();
    std::cout<<"SysPulse Native — read-only inspection\n";
    if(r.system.memory)std::cout<<"Physical memory available: "<<r.system.memory->availableKiB<<" / "<<r.system.memory->totalKiB<<" KiB\n";
    else std::cout<<"Physical memory: unavailable\n";
    const auto show=[](const char* title,bool known,std::size_t n){std::cout<<title<<": ";if(known)std::cout<<n;else std::cout<<"unavailable";std::cout<<'\n';};
    show("CPU cores",r.cpuAvailable,r.cores.size());show("Disk counter rows (not aggregated)",r.diskAvailable,r.disks.size());show("Network interfaces",r.networkAvailable,r.networks.size());show("Readable process rows",r.processAvailable,r.processes.size());
    if(r.processTruncated)std::cout<<"Process inventory capped\n";
    const char* state="unknown";switch(memoryPressure(r.system.memory)){case MemoryPressure::Normal:state="normal";break;case MemoryPressure::Elevated:state="elevated";break;case MemoryPressure::High:state="high";break;case MemoryPressure::Unknown:break;}
    std::cout<<"Memory availability heuristic: "<<state<<" (not a repair recommendation)\n";
    return 0;
}
