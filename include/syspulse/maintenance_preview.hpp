// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <charconv>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
namespace syspulse::preview {
struct Swap { std::uint64_t total, free; };
inline std::optional<Swap> swap(std::istream& input) {
    std::optional<std::uint64_t> total,free; bool seenTotal=false,seenFree=false;
    std::string line; unsigned count=0;
    while(std::getline(input,line)) {
        if(++count>1024||line.size()>4096)return {};
        std::istringstream row(line);std::string key,value,unit,extra;
        if(!(row>>key)|| (key!="SwapTotal:"&&key!="SwapFree:"))continue;
        bool& seen=key=="SwapTotal:"?seenTotal:seenFree;
        if(seen)return {};
        seen=true;
        if(!(row>>value>>unit)||unit!="kB"||row>>extra)return {};
        std::uint64_t n=0;auto p=std::from_chars(value.data(),value.data()+value.size(),n);
        if(p.ec!=std::errc{}||p.ptr!=value.data()+value.size())return {};
        (key=="SwapTotal:"?total:free)=n;
    }
    if(input.bad()||(!input.eof()&&input.fail())||!total||!free||*free>*total)return {};
    return Swap{*total,*free};
}
struct Cache { std::uint64_t bytes=0; unsigned files=0; bool complete=false; };
// Count direct regular .deb entries only. Never follow symlinks, descend, or delete.
// Logical file lengths are not guaranteed recoverable disk blocks (hardlinks etc.).
inline Cache archives(const std::filesystem::path& dir,unsigned limit=4096) {
    Cache out;if(!limit)return out;
    const int fd=::open(dir.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return out;
    DIR* stream=::fdopendir(fd);
    if(!stream){::close(fd);return out;}
    struct Close { DIR* stream; ~Close(){::closedir(stream);} } guard{stream};
    unsigned seen=0;
    for(;;){
        errno=0;const auto* entry=::readdir(stream);
        if(!entry){out.complete=errno==0;return out;}
        const std::string name=entry->d_name;
        if(name=="."||name=="..")continue;
        if(++seen>limit)return out;
        if(name.size()<=4||name.compare(name.size()-4,4,".deb")!=0)continue;
        struct stat st{};
        if(::fstatat(fd,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)!=0)return out;
        if(!S_ISREG(st.st_mode))continue;
        if(st.st_size<0)return out;
        const auto bytes=static_cast<std::uint64_t>(st.st_size);
        if(bytes>std::numeric_limits<std::uint64_t>::max()-out.bytes)return out;
        out.bytes+=bytes;++out.files;
    }
}
inline std::optional<Swap> currentSwap(){std::ifstream in("/proc/meminfo");return swap(in);}
}
