// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/device_rates.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <set>
#if defined(__linux__)
#include <dirent.h>
#include <memory>
#endif
namespace syspulse::native {
namespace {
using Key=std::pair<DeviceKind,std::string>;
constexpr std::size_t cap=256;
bool validName(const std::string& name){return !name.empty()&&name.size()<=128&&name!="."&&name!=".."&&name.find_first_of("/ \t\r\n")==name.npos;}
#if defined(__linux__)
std::optional<std::string> readSmall(const std::string& path){std::ifstream f(path,std::ios::binary);std::array<char,1024> b{};if(!f)return {};f.read(b.data(),b.size());if(f.bad()||!f.eof())return {};return std::string(b.data(),static_cast<std::size_t>(f.gcount()));}
std::optional<std::uint64_t> number(const std::string& value){std::istringstream stream(value);std::string token,extra;if(!(stream>>token)||(stream>>extra))return {};std::uint64_t n{};auto r=std::from_chars(token.data(),token.data()+token.size(),n);return r.ec==std::errc{}&&r.ptr==token.data()+token.size()?std::optional<std::uint64_t>(n):std::nullopt;}
std::optional<std::uint64_t> readNumber(const std::string& path){auto raw=readSmall(path);return raw?number(*raw):std::nullopt;}
std::optional<DeviceCounter> disk(const std::string& name,const std::string& root){const auto dir=root+name+"/";auto first=readNumber(dir+"diskseq");if(!first||!*first)return {};auto stats=readSmall(dir+"stat");auto last=readNumber(dir+"diskseq");if(!stats||first!=last)return {};std::istringstream stream(*stats);std::array<std::uint64_t,11> fields{};std::string t;for(auto& f:fields){if(!(stream>>t))return {};auto v=number(t);if(!v)return {};f=*v;}return DeviceCounter{DeviceKind::Disk,name,std::to_string(*first),fields[2],fields[6]};}
std::optional<std::string> netId(const std::string& dir){auto a=readNumber(dir+"ifindex"),b=readNumber(dir+"iflink");if(!a||!b||!*a||!*b)return {};return std::to_string(*a)+":"+std::to_string(*b);}
std::optional<DeviceCounter> network(const std::string& name,const std::string& root){const auto dir=root+name+"/";auto first=netId(dir);if(!first)return {};auto rx=readNumber(dir+"statistics/rx_bytes"),tx=readNumber(dir+"statistics/tx_bytes");auto last=netId(dir);if(!rx||!tx||first!=last)return {};return DeviceCounter{DeviceKind::Network,name,*first,*rx,*tx};}
void collect(DeviceSample& sample,const char* root,std::optional<DeviceCounter>(*reader)(const std::string&,const std::string&)){auto close=[](DIR* d){closedir(d);};std::unique_ptr<DIR,decltype(close)> dir(opendir(root),close);if(!dir)return;std::size_t examined=0;while(auto* e=readdir(dir.get())){std::string name(e->d_name);if(!validName(name))continue;if(++examined>cap||sample.counters.size()>=cap){sample.truncated=true;break;}if(auto row=reader(name,root))sample.counters.push_back(std::move(*row));}}
#endif
}
DeviceSample captureDevices(){DeviceSample s;s.time=Sample::Clock::now();
#if defined(__linux__)
    collect(s,"/sys/class/block/",disk);collect(s,"/sys/class/net/",network);
#endif
    return s;}
std::optional<std::vector<DeviceRate>> deviceRates(const DeviceSample& before,const DeviceSample& after){const double seconds=std::chrono::duration<double>(after.time-before.time).count();if(!(seconds>0)||!std::isfinite(seconds)||seconds>30||before.counters.size()>cap||after.counters.size()>cap)return {};std::map<Key,const DeviceCounter*> old;std::set<Key> seen;for(const auto& row:before.counters)if(!validName(row.name)||!old.emplace(Key{row.kind,row.name},&row).second)return {};std::vector<DeviceRate> result;for(const auto& row:after.counters){Key k{row.kind,row.name};if(!validName(row.name)||!seen.insert(k).second)return {};const auto it=old.find(k);if(it==old.end()||row.identity.empty()||it->second->identity!=row.identity)continue;auto rx=counterRate(it->second->read,row.read,seconds),tx=counterRate(it->second->write,row.write,seconds);if(!rx||!tx)continue;const double scale=row.kind==DeviceKind::Disk?512:1;result.push_back({row.kind,row.name,*rx*scale,*tx*scale});}return result;}
}
