// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/runtime.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <locale>
#include <set>
namespace syspulse::native {
namespace {
using U=std::uint64_t;
std::optional<U> unsignedValue(std::string_view s){U n{};if(s.empty())return {};const auto r=std::from_chars(s.data(),s.data()+s.size(),n);if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size())return {};return n;}
std::optional<double> percent(std::string_view s){if(s.empty()||s.size()>16||s.find_first_not_of("0123456789.")!=s.npos)return {};std::istringstream stream{std::string(s)};stream.imbue(std::locale::classic());double v{};char extra{};if(!(stream>>v)||(stream>>extra)||!std::isfinite(v)||v<0||v>100)return {};return v;}
std::optional<PressureLine> pressureLine(std::istringstream& row){std::string a,b,c,d,extra;if(!(row>>a>>b>>c>>d)||(row>>extra))return {};
    if(a.rfind("avg10=",0)||b.rfind("avg60=",0)||c.rfind("avg300=",0)||d.rfind("total=",0))return {};
    const auto x=percent(std::string_view(a).substr(6)),y=percent(std::string_view(b).substr(6)),z=percent(std::string_view(c).substr(7));const auto t=unsignedValue(std::string_view(d).substr(6));if(!x||!y||!z||!t)return {};return PressureLine{*x,*y,*z,*t};}
#if defined(__linux__)
std::optional<std::string> bounded(const char* path,std::size_t limit){std::ifstream f(path,std::ios::binary);if(!f)return {};std::string s;std::array<char,4096> buf{};while(f){f.read(buf.data(),buf.size());const auto n=static_cast<std::size_t>(f.gcount());if(n>limit-s.size())return {};s.append(buf.data(),n);}if(f.bad()||!f.eof())return {};return s;}
std::optional<Pressure> readPressure(const char* p){const auto raw=bounded(p,4096);return raw?parsePressure(*raw):std::nullopt;}
#endif
}
std::optional<Pressure> parsePressure(std::string_view input){if(input.empty()||input.size()>4096)return {};std::istringstream stream{std::string(input)};std::string line;std::optional<PressureLine> some,full;while(std::getline(stream,line)){std::istringstream row(line);std::string type;if(!(row>>type))return {};auto value=pressureLine(row);if(!value)return {};if(type=="some"&&!some)some=value;else if(type=="full"&&!full)full=value;else return {};}
    if(!some)return {};
    return Pressure{*some,full};}
PressureEvidence collectPressure(){
#if defined(__linux__)
    return {readPressure("/proc/pressure/cpu"),readPressure("/proc/pressure/memory"),readPressure("/proc/pressure/io")};
#else
    return {};
#endif
}
std::optional<double> processTicksPerSecond(const Process& before,const Process& after,double seconds) noexcept {if(!before.pid||before.pid!=after.pid||!before.startTicks||before.startTicks!=after.startTicks)return {};return counterRate(before.cpuTicks,after.cpuTicks,seconds);}
IdentityReview reviewProcess(ProcessIdentity target,const std::vector<Process>& current,std::uint32_t ownPid) noexcept {if(target.pid<=1||!ownPid||target.pid==ownPid||!target.startTicks)return IdentityReview::Protected;const Process* match=nullptr;for(const auto& row:current)if(row.pid==target.pid){if(match)return IdentityReview::Ambiguous;match=&row;}if(!match)return IdentityReview::Missing;return match->startTicks==target.startTicks?IdentityReview::Match:IdentityReview::Changed;}
std::optional<Rates> sampleRates(const Sample& before,const Sample& after){const double seconds=std::chrono::duration<double>(after.time-before.time).count();if(!(seconds>0)||!std::isfinite(seconds)||seconds>30)return {};Rates result;result.seconds=seconds;if(before.system.cpu&&after.system.cpu)result.cpu=cpuPercent(*before.system.cpu,*after.system.cpu);if(const auto& m=after.system.memory;m&&m->totalKiB&&m->availableKiB<=m->totalKiB)result.memoryUsed=100.0*static_cast<double>(m->totalKiB-m->availableKiB)/static_cast<double>(m->totalKiB);
    std::map<std::string,CpuTicks> old;std::set<std::string> seen;for(const auto& c:before.cores)if(!old.emplace(c.id,c.ticks).second)return {};for(const auto& c:after.cores){if(!seen.insert(c.id).second)return {};const auto it=old.find(c.id);if(it!=old.end())if(auto rate=cpuPercent(it->second,c.ticks))result.cores.emplace(c.id,*rate);}return result;}
Sample captureSample(){Sample s;s.time=Sample::Clock::now();s.system=collect();
#if defined(__linux__)
    if(auto raw=bounded("/proc/stat",1024*1024)){s.system.cpu=parseLinuxCpu(*raw);s.cores=parseCores(*raw);}
    else s.system.cpu.reset();
#endif
    return s;}
History::History(std::size_t capacity):capacity_(std::clamp<std::size_t>(capacity,1,600)){}
void History::push(Rates value){value.cores.clear();if(rows_.size()>=capacity_)rows_.pop_front();rows_.push_back(std::move(value));}
}
