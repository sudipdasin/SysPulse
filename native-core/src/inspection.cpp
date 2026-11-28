// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/inspection.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <array>
#if defined(__linux__)
#include <dirent.h>
#endif
namespace syspulse::native {
namespace {
using S=std::string_view; using U=std::uint64_t;
constexpr std::size_t inputLimit=1024*1024, rowLimit=4096;
S token(S& s){const auto p=s.find_first_not_of(" \t\r");if(p==S::npos){s={};return {};}
    s.remove_prefix(p);const auto e=s.find_first_of(" \t\r");auto t=s.substr(0,e);s=e==S::npos?S{}:s.substr(e);return t;}
S line(S& s){const auto e=s.find('\n');auto t=s.substr(0,e);s=e==S::npos?S{}:s.substr(e+1);return t;}
std::optional<U> num(S s){U n{};if(s.empty())return {};auto r=std::from_chars(s.data(),s.data()+s.size(),n);if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size())return {};return n;}
bool nameOk(S n){return !n.empty()&&n.size()<=128&&n.find_first_of("/\t\r\n ")==S::npos;}
#if defined(__linux__)
std::optional<std::string> readFile(const std::string& path,std::size_t limit){
    auto* f=std::fopen(path.c_str(),"rb");if(!f)return {};
    std::string out;std::array<char,4096> buf{};bool ok=true;
    for(;;){const auto n=std::fread(buf.data(),1,buf.size(),f);if(n>limit-out.size()){ok=false;break;}out.append(buf.data(),n);if(n<buf.size()){ok=!std::ferror(f);break;}}
    std::fclose(f);if(!ok)return {};return out;
}
#endif
}
std::vector<CpuCore> parseCores(S text){std::vector<CpuCore> out;std::set<std::string> seen;if(text.size()>inputLimit)return out;
    while(!text.empty()&&out.size()<rowLimit){auto l=line(text);auto id=token(l);if(id.size()<4||id.substr(0,3)!="cpu"||!num(id.substr(3)))continue;
        std::string key(id);if(!seen.insert(key).second)return {};
        auto ticks=parseLinuxCpu("cpu "+std::string(l));if(ticks)out.push_back({key,*ticks});}return out;}
std::vector<Disk> parseDisks(S text){std::vector<Disk> out;std::set<std::string> seen;if(text.size()>inputLimit)return out;
    while(!text.empty()&&out.size()<rowLimit){auto l=line(text);auto major=num(token(l)),minor=num(token(l));auto name=token(l);if(!major||!minor||!nameOk(name))continue;
        std::array<U,11> f{};bool ok=true;for(auto& v:f){auto n=num(token(l));if(!n){ok=false;break;}v=*n;}if(!ok)continue;
        std::string key(name);if(!seen.insert(key).second)return {};out.push_back({key,f[2],f[6],f[9]});}return out;}
std::vector<Network> parseNetwork(S text){std::vector<Network> out;std::set<std::string> seen;if(text.size()>inputLimit)return out;
    while(!text.empty()&&out.size()<rowLimit){auto l=line(text);auto colon=l.find(':');if(colon==S::npos)continue;auto left=l.substr(0,colon);auto name=token(left);if(!nameOk(name)||!token(left).empty())continue;l.remove_prefix(colon+1);
        std::array<U,16> f{};bool ok=true;for(auto& v:f){auto n=num(token(l));if(!n){ok=false;break;}v=*n;}if(!ok)continue;
        std::string key(name);if(!seen.insert(key).second)return {};out.push_back({key,f[0],f[8]});}return out;}
std::optional<Process> parseProcess(S text){if(text.empty()||text.size()>65536)return {};const auto left=text.find(" ("),right=text.rfind(')');if(left==S::npos||right==S::npos||right<=left+2)return {};
    auto pid=num(text.substr(0,left));if(!pid||!*pid||*pid>UINT32_MAX)return {};auto name=text.substr(left+2,right-left-2);if(name.size()>4096)return {};auto rest=text.substr(right+1);auto state=token(rest);if(state.size()!=1)return {};
    std::array<S,21> f{};for(auto& t:f){t=token(rest);if(t.empty())return {};}
    auto parent=num(f[0]),u=num(f[10]),s=num(f[11]),start=num(f[18]),rss=num(f[20]);if(!parent||*parent>UINT32_MAX||!u||!s||!start||*s>UINT64_MAX-*u)return {};
    return Process{static_cast<std::uint32_t>(*pid),static_cast<std::uint32_t>(*parent),std::string(name),state.front(),*u+*s,*start,rss};}
std::optional<double> counterRate(U before,U after,double seconds) noexcept {if(after<before||!std::isfinite(seconds)||seconds<=0)return {};const auto v=static_cast<double>(after-before)/seconds;return std::isfinite(v)?std::optional<double>(v):std::nullopt;}
Inspection inspect(std::size_t maxProcesses){Inspection r;r.system=collect();
#if defined(__linux__)
    if(auto s=readFile("/proc/stat",inputLimit)){r.cores=parseCores(*s);r.cpuAvailable=!r.cores.empty();}
    if(auto s=readFile("/proc/diskstats",inputLimit)){r.disks=parseDisks(*s);r.diskAvailable=!r.disks.empty();}
    if(auto s=readFile("/proc/net/dev",inputLimit)){r.networks=parseNetwork(*s);r.networkAvailable=!r.networks.empty();}
    if(auto* dir=opendir("/proc")){r.processAvailable=true;maxProcesses=std::min(maxProcesses,rowLimit);std::size_t examined=0;
        while(auto* e=readdir(dir)){auto p=num(e->d_name);if(!p||!*p||*p>UINT32_MAX)continue;if(examined++>=rowLimit||r.processes.size()>=maxProcesses){r.processTruncated=true;break;}
            if(auto s=readFile(std::string("/proc/")+e->d_name+"/stat",65536)){auto row=parseProcess(*s);if(row&&row->pid==*p)r.processes.push_back(std::move(*row));}}
        closedir(dir);std::sort(r.processes.begin(),r.processes.end(),[](const auto& a,const auto& b){return a.pid<b.pid;});}
#else
    (void)maxProcesses;
#endif
    return r;}
MemoryPressure memoryPressure(const std::optional<Memory>& m) noexcept {if(!m||!m->totalKiB||m->availableKiB>m->totalKiB)return MemoryPressure::Unknown;const double ratio=static_cast<double>(m->availableKiB)/static_cast<double>(m->totalKiB);return ratio<0.10?MemoryPressure::High:ratio<0.20?MemoryPressure::Elevated:MemoryPressure::Normal;}
}
