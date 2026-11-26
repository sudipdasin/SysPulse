// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
#include <sstream>
#include <string>
namespace syspulse::aptstatus {
struct Versions { std::string installed, candidate; };
inline bool validToken(const std::string& s) {
    if(s=="(none)") return true;
    if(s.empty()||s.size()>128||s[0]<'0'||s[0]>'9')return false;
    for(unsigned char c:s)if(!((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='.'||c=='+'||c=='~'||c=='-'||c==':'))return false;
    return true;
}
inline std::optional<Versions> parse(const std::string& data) {
    if(data.size()>16384)return {};
    Versions out; bool seenInstalled=false,seenCandidate=false,seenPackage=false;
    std::istringstream in(data);std::string line;
    while(std::getline(in,line)) {
        if(line.size()>4096)return {};
        if(line=="syspulse:"){if(seenPackage)return {};seenPackage=true;continue;}
        const bool installed=line.rfind("  Installed: ",0)==0;
        const bool candidate=line.rfind("  Candidate: ",0)==0;
        if(!installed&&!candidate)continue;
        if(!seenPackage)return {};
        auto& seen=installed?seenInstalled:seenCandidate;
        auto& value=installed?out.installed:out.candidate;
        if(seen)return {};
        seen=true;value=line.substr(13);
        if(!validToken(value))return {};
    }
    if(!seenPackage||!seenInstalled||!seenCandidate)return {};
    return out;
}
}
