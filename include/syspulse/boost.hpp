// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/cleaner.hpp"
#include "syspulse/maintenance_preview.hpp"
namespace syspulse::boost {
namespace c=syspulse::cleaner;
inline constexpr unsigned AgeDays=30, MaxFiles=100;
inline bool thumbnail(const c::fs::path& path,const c::fs::path& cache){
    if(cache.empty()||!c::inside(path,cache))return false;
    auto relative=path.lexically_relative(cache);
    auto it=relative.begin();if(it==relative.end()||*it++!="thumbnails")return false;
    if(it==relative.end())return false;
    const auto size=*it++;
    if(size!="normal"&&size!="large"&&size!="x-large"&&size!="xx-large")return false;
    if(it==relative.end())return false;
    const auto name=(it++)->string();
    if(it!=relative.end()||name.size()!=36||name.substr(32)!=".png")return false;
    return std::all_of(name.begin(),name.begin()+32,[](char ch){return (ch>='0'&&ch<='9')||(ch>='a'&&ch<='f');});
}
struct Plan {
    c::fs::path cache;
    std::vector<c::Entry> files;
    std::uint64_t bytes=0;
    std::string status;
    preview::Cache apt;
};
inline Plan prepare(const c::fs::path& cache,const std::atomic_bool& cancel){
    Plan p;p.cache=cache;
    p.apt=preview::archives("/var/cache/apt/archives");
    if(::geteuid()==0||cache.empty()||!cache.is_absolute()){p.status="Ordinary user cache unavailable.";return p;}
    std::error_code ec;
    const auto canonical=c::fs::canonical(cache,ec);
    if(ec||canonical!=cache.lexically_normal()){p.status="Cache missing or redirected; automatic user-cache deletion skipped.";return p;}
    c::Fd base(c::openDir(cache));struct stat st{};
    if(base.n<0||::fstat(base.n,&st)||st.st_uid!=::getuid()){p.status="Cache ownership could not be verified.";return p;}
    const auto root=cache/"thumbnails";
    const auto actual=c::fs::canonical(root,ec);
    if(ec||actual!=root){p.status="No ordinary thumbnail cache found.";return p;}
    c::Fd thumb(c::openDir(root));
    const auto mount=c::mountId(base.n);
    if(thumb.n<0||!mount||c::mountId(thumb.n)!=mount){p.status="Thumbnail mount identity cannot be verified.";return p;}
    c::Limits limits;limits.entries=25000;limits.seconds=15;limits.top=MaxFiles;limits.ageDays=AgeDays;
    const auto result=c::scanRequested(root,cache,cache,limits,cancel,false);
    if(!result.complete||result.readOnly||result.redirected||result.mountExcluded||result.accountingOverflow){p.status="Thumbnail scan incomplete or protected; no automatic deletion.";return p;}
    for(const auto& f:result.candidates){
        if(!f.removable||!thumbnail(f.path,cache)||f.bytes>std::numeric_limits<std::uint64_t>::max()-p.bytes)continue;
        p.files.push_back(f);p.bytes+=f.bytes;
        if(p.files.size()>=MaxFiles)break;
    }
    p.status=p.files.empty()?"No eligible thumbnails older than 30 days.":"Reviewed thumbnail batch ready.";
    return p;
}
struct Outcome {unsigned removed=0,skipped=0;std::uint64_t bytes=0;};
inline Outcome apply(const Plan& plan,std::time_t now=std::time(nullptr)){
    Outcome out;
    if(::geteuid()==0||plan.cache.empty()||plan.files.size()>MaxFiles)return out;
    for(const auto& file:plan.files){
        if(!thumbnail(file.path,plan.cache)||file.bytes>std::numeric_limits<std::uint64_t>::max()-out.bytes||!c::removeCache(plan.cache,file,AgeDays,now)){++out.skipped;continue;}
        ++out.removed;out.bytes+=file.bytes;
    }
    return out;
}
}
