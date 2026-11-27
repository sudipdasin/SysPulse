// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/bounded_line.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
namespace syspulse::system_memory {
enum class State { Unknown, Disabled, Enabled };
inline State parseEnabled(const std::string& s) {
    if(s=="Y"||s=="1")return State::Enabled;
    if(s=="N"||s=="0")return State::Disabled;
    return State::Unknown;
}
inline State readEnabled(){std::ifstream in("/sys/module/zswap/parameters/enabled");std::string value,extra;if(!(in>>value)||in>>extra||in.bad())return State::Unknown;return parseEnabled(value);}
inline std::optional<bool> activeSwap(std::istream& in) {
    std::string line,a,b,c,d,e,extra;
    if(boundedLine<4096>(in,line)!=LineState::Line)return {};
    std::istringstream header(line);
    if(!(header>>a>>b>>c>>d>>e)||header>>extra||a!="Filename"||b!="Type"||c!="Size"||d!="Used"||e!="Priority")return {};
    bool active=false;unsigned rows=0;
    for(;;){
        const auto state=boundedLine<4096>(in,line);
        if(state==LineState::End)break;
        if(state!=LineState::Line||++rows>256)return {};
        std::istringstream row(line);
        if(!(row>>a>>b>>c>>d>>e)||row>>extra||a.empty()||a[0]!='/'||(b!="file"&&b!="partition"))return {};
        unsigned long long size=0,used=0;int priority=0;
        const auto x=std::from_chars(c.data(),c.data()+c.size(),size),y=std::from_chars(d.data(),d.data()+d.size(),used),z=std::from_chars(e.data(),e.data()+e.size(),priority);
        if(x.ec!=std::errc{}||x.ptr!=c.data()+c.size()||y.ec!=std::errc{}||y.ptr!=d.data()+d.size()||z.ec!=std::errc{}||z.ptr!=e.data()+e.size()||!size||used>size)return {};
        if(a.rfind("/dev/zram",0)==0)return {}; // refuse mixed compression configurations
        active=true;
    }
    if(in.bad()||(!in.eof()&&in.fail()))return {};
    return active;
}
inline std::optional<bool> zramPresent(){
    namespace fs=std::filesystem;std::error_code ec;fs::directory_iterator it("/sys/block",ec),end;
    if(ec)return {};
    unsigned count=0;
    for(;it!=end;it.increment(ec)){
        if(ec||++count>4096)return {};
        if(it->path().filename().string().rfind("zram",0)==0)return true;
    }
    if(ec)return {};
    return false;
}
inline bool canEnable(State state,std::optional<bool> swap,std::optional<bool> zram){return state==State::Disabled&&swap&&*swap&&zram&&!*zram;}
inline bool eligible(){std::ifstream in("/proc/swaps");return canEnable(readEnabled(),activeSwap(in),zramPresent());}
}
