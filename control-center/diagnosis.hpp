#pragma once
#include <string>
#include <vector>
#include <sys/statvfs.h>
#include <fstream>
#include <sstream>
namespace sp {
struct Finding { std::string level, title, advice; };
inline std::vector<Finding> assess(long disk, long mem, long swap) {
    std::vector<Finding> f;
    if(disk<0 || disk>100) f.push_back({"UNKNOWN","Root storage unavailable","Open disk management to inspect the filesystem."});
    else if(disk>=98) f.push_back({"CRITICAL","Root filesystem almost full","Review large files and back up before deleting anything."});
    else if(disk>=90) f.push_back({"WARNING","Root filesystem is nearly full","Review disk usage and cleanup candidates."});
    else f.push_back({"OK","Root filesystem below 90% used","No high-usage warning from this check."});
    if(mem<0 || mem>100) f.push_back({"UNKNOWN","Available RAM unavailable","Open system information for more detail."});
    else if(mem<5) f.push_back({"CRITICAL","Very low available RAM","Save work; inspect memory-heavy processes before closing them."});
    else if(mem<15) f.push_back({"WARNING","Low available RAM","Review task manager and swap; do not blindly drop caches."});
    else f.push_back({"OK","Available RAM above low-memory threshold","Snapshot only; not a performance guarantee."});
    if(swap==0) f.push_back({"INFO","No swap configured","Review workload and zRAM or swap needs; no automatic tuning."});
    else if(swap<0) f.push_back({"UNKNOWN","Swap configuration unavailable","Review memory and swap information."});
    else f.push_back({"OK","Swap configured","Review utilization separately if performance is poor."});
    return f;
}
inline std::vector<Finding> snapshot(){
    struct statvfs fs{}; long disk=-1,mem=-1,swap=-1;
    if(statvfs("/",&fs)==0 && fs.f_blocks && fs.f_bfree<=fs.f_blocks && fs.f_bavail<=fs.f_bfree){
        const auto used=fs.f_blocks-fs.f_bfree; const auto denom=static_cast<long double>(used)+fs.f_bavail;
        if(denom>0) disk=static_cast<long>(100.0L*used/denom+0.999L);
    }
    std::ifstream in("/proc/meminfo"); std::string line; unsigned long long total=0,avail=0; bool have=false;
    while(std::getline(in,line)){std::istringstream r(line); std::string key,unit; unsigned long long n=0;
        if(!(r>>key) || !(r>>std::ws) || r.peek()<'0'||r.peek()>'9'||!(r>>n>>unit)||unit!="kB")continue;
        if(key=="MemTotal:")total=n; else if(key=="MemAvailable:"){avail=n;have=true;} else if(key=="SwapTotal:")swap=n?1:0;
    }
    if(total && have && avail<=total)mem=static_cast<long>(100.0L*avail/total);
    return assess(disk,mem,swap);
}
}
