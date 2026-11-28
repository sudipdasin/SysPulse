// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/apt_plan.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <sstream>
#if defined(__linux__)
#include <openssl/sha.h>
#endif
namespace syspulse::native {
bool validPlanToken(std::string_view token) noexcept{return token.size()==64&&std::all_of(token.begin(),token.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
namespace {
#if defined(__linux__)
std::optional<std::uint64_t> number(const std::string& s){std::uint64_t n{};auto p=std::from_chars(s.data(),s.data()+s.size(),n);if(s.empty()||p.ec!=std::errc{}||p.ptr!=s.data()+s.size())return {};return n;}
#endif
}
std::optional<std::string> aptPlanToken(std::string_view operation,std::string_view output){
#if defined(__linux__)
    if((operation!="upgrade"&&operation!="repair")||output.empty()||output.size()>65536)return {};
    std::istringstream in{std::string(output)};std::string line,canonical(operation);canonical+='\n';std::optional<std::uint64_t> expected;std::uint64_t installs=0;unsigned lines=0;
    while(std::getline(in,line)){
        if(++lines>512||line.size()>1024||line.find('\0')!=line.npos)return {};
        if(line.rfind("Remv ",0)==0||line.rfind("Purg ",0)==0)return {};
        if(line.rfind("Inst ",0)==0||line.rfind("Conf ",0)==0){if(line.size()>512||!std::all_of(line.begin(),line.end(),[](unsigned char c){return c>=32&&c<127;}))return {};if(line.rfind("Inst ",0)==0)++installs;canonical+=line+'\n';}
        if(line.find(" upgraded, ")!=line.npos){
            if(expected)return {};
            std::istringstream row(line);std::string a,b,c,d,e,f,g,h,i,j,k,l,extra;
            if(!(row>>a>>b>>c>>d>>e>>f>>g>>h>>i>>j>>k>>l)||(row>>extra)||b!="upgraded,"||d!="newly"||e!="installed,"||g!="to"||h!="remove"||i!="and"||k!="not"||l!="upgraded.")return {};
            auto upgraded=number(a),added=number(c),removed=number(f),held=number(j);if(!upgraded||!added||!removed||*removed||!held||*added>UINT64_MAX-*upgraded)return {};
            expected=*upgraded+*added;canonical+=line+'\n';
        }
    }
    if(in.bad()||!expected||*expected!=installs)return {};
    std::array<unsigned char,SHA256_DIGEST_LENGTH> digest{};if(!SHA256(reinterpret_cast<const unsigned char*>(canonical.data()),canonical.size(),digest.data()))return {};
    constexpr char hex[]="0123456789abcdef";std::string token;token.reserve(64);for(auto c:digest){token+=hex[c>>4];token+=hex[c&15];}return token;
#else
    (void)operation;(void)output;return {};
#endif
}
}
