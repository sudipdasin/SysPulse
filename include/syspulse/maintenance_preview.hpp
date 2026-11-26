// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <charconv>
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
    namespace fs=std::filesystem;Cache out;std::error_code ec;
    if(!fs::is_directory(fs::symlink_status(dir,ec))||ec||!limit)return out;
    fs::directory_iterator it(dir,ec),end;if(ec)return out;
    unsigned seen=0;
    for(;it!=end;it.increment(ec)) {
        if(ec||++seen>limit)return out;
        auto status=it->symlink_status(ec);if(ec)return out;
        if(!fs::is_regular_file(status)||it->path().extension()!=".deb")continue;
        auto bytes=it->file_size(ec);if(ec||bytes>std::numeric_limits<std::uint64_t>::max()-out.bytes)return out;
        out.bytes+=bytes;++out.files;
    }
    out.complete=!ec;return out;
}
inline std::optional<Swap> currentSwap(){std::ifstream in("/proc/meminfo");return swap(in);}
}
