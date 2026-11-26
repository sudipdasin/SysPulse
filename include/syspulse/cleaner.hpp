// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <limits>
#include <set>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <vector>
namespace syspulse::cleaner {
namespace fs=std::filesystem;
struct Fd { int n=-1; explicit Fd(int v=-1):n(v){} ~Fd(){if(n>=0)::close(n);} Fd(const Fd&)=delete; Fd& operator=(const Fd&)=delete; };
inline int openDir(const fs::path& p){
    if(!p.is_absolute()){errno=EINVAL;return -1;}
    Fd fd(::open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));if(fd.n<0)return -1;
    for(const auto& part:p.relative_path()){
        if(part.empty()||part==".")continue;
        if(part==".."){errno=EINVAL;return -1;}
        int next=::openat(fd.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(next<0)return -1;
        ::close(fd.n);fd.n=next;
    }
    int result=fd.n;fd.n=-1;return result;
}
inline fs::path homeCache(){
    const auto* pw=::getpwuid(::getuid());
    if(!pw||!pw->pw_dir||pw->pw_dir[0]!='/')return {};
    return fs::path(pw->pw_dir)/".cache";
}
inline bool inside(const fs::path& path,const fs::path& base){
    if(base.empty())return false;
    auto p=path.begin();for(auto b=base.begin();b!=base.end();++b,++p)if(p==path.end()||*p!=*b)return false;
    return p!=path.end();
}
enum Category { Cache, Review, Archives, Media, Other, Count };
inline Category category(const fs::path& path,const fs::path& cache){
    if(inside(path,cache))return Cache;
    auto ext=path.extension().string();
    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return c>='A'&&c<='Z'?c+32:c;});
    if(ext==".tmp"||ext==".bak"||ext==".old"||ext==".log"||ext==".part"||ext==".crdownload")return Review;
    if(ext==".zip"||ext==".gz"||ext==".xz"||ext==".7z"||ext==".tar"||ext==".iso"||ext==".deb"||ext==".rpm")return Archives;
    if(ext==".mp4"||ext==".mkv"||ext==".avi"||ext==".mp3"||ext==".wav"||ext==".jpg"||ext==".png"||ext==".webm")return Media;
    return Other;
}
struct Entry { fs::path path; std::uint64_t bytes=0; dev_t dev=0; ino_t ino=0; uid_t uid=0; timespec mtime{},ctime{}; Category kind=Other; bool removable=false; std::uint64_t allocated=0; };
struct Volume {std::uint64_t used=0,available=0,reserved=0;bool known=false;};
struct Limits {unsigned entries=150000, seconds=45, top=250;std::uint64_t minBytes=0;unsigned ageDays=7;bool collectDuplicates=false,allowRemoval=true;};
struct Result {std::array<std::uint64_t,Count> categories{};std::vector<Entry> largest,candidates,review,duplicateInput;std::uint64_t allocated=0,logical=0;unsigned hardlinkAliases=0;bool accountingOverflow=false,duplicateInputLimited=false;Volume volume;unsigned visited=0,files=0,skipped=0;bool complete=false,cancelled=false,limited=false;std::string error;fs::path resolvedRoot;bool redirected=false,missingCache=false,readOnly=false;};
inline bool sameTime(timespec a,timespec b){return a.tv_sec==b.tv_sec&&a.tv_nsec==b.tv_nsec;}
inline bool oldEnough(const struct stat& st,unsigned days,std::time_t now){
    return now>0&&days>=7&&days<=3650&&std::difftime(now,st.st_mtime)>=days*86400.0&&std::difftime(now,st.st_ctime)>=days*86400.0;
}
inline bool safeCache(const struct stat& st,unsigned days,std::time_t now){
    return ::geteuid()!=0&&S_ISREG(st.st_mode)&&st.st_uid==::getuid()&&st.st_nlink==1&&oldEnough(st,days,now);
}
inline void retain(std::vector<Entry>& v,Entry e,unsigned max){
    if(!max)return;
    const auto cmp=[](const Entry& a,const Entry& b){return a.bytes>b.bytes;};
    if(v.size()<max){v.push_back(std::move(e));std::push_heap(v.begin(),v.end(),cmp);}
    else if(e.bytes>v.front().bytes){std::pop_heap(v.begin(),v.end(),cmp);v.back()=std::move(e);std::push_heap(v.begin(),v.end(),cmp);}
}
inline Volume volume(int fd){
    struct statvfs v{};if(::fstatvfs(fd,&v)||!v.f_frsize||v.f_bfree>v.f_blocks||v.f_bavail>v.f_bfree||v.f_blocks>std::numeric_limits<std::uint64_t>::max()/v.f_frsize)return {};
    return {(v.f_blocks-v.f_bfree)*v.f_frsize,v.f_bavail*v.f_frsize,(v.f_bfree-v.f_bavail)*v.f_frsize,true};
}
inline Result scan(const fs::path& root,const fs::path& cache,const Limits& limits,const std::atomic_bool& cancel){
    Result out;out.resolvedRoot=root;Fd fd(openDir(root));if(fd.n<0){const int reason=errno;out.error="Cannot open scan directory: "+std::string(std::strerror(reason))+". Choose an accessible folder.";return out;}
    struct stat rootStat{};if(::fstat(fd.n,&rootStat)){out.error="Cannot inspect scan directory.";return out;}
    out.volume=volume(fd.n);
    const auto start=std::chrono::steady_clock::now();const auto now=std::time(nullptr);
    const auto stop=[&]{if(cancel.load()){out.cancelled=true;return true;}if(out.visited>=limits.entries||std::chrono::steady_clock::now()-start>=std::chrono::seconds(limits.seconds)){out.limited=true;return true;}return false;};
    std::set<std::pair<dev_t,ino_t>> seen;
    const auto add=[&](std::uint64_t& sum,std::uint64_t n){if(n>std::numeric_limits<std::uint64_t>::max()-sum)out.accountingOverflow=true;else sum+=n;};
    const auto visit=[&](auto&& self,int dir,const fs::path& path,unsigned depth)->void{
        if(stop())return;
        if(depth>64){++out.skipped;return;}
        const int copy=::fcntl(dir,F_DUPFD_CLOEXEC,3);if(copy<0){++out.skipped;return;}
        DIR* stream=::fdopendir(copy);if(!stream){::close(copy);++out.skipped;return;}
        struct Close{DIR* d;~Close(){::closedir(d);}} guard{stream};
        while(!stop()){
            errno=0;const auto* ent=::readdir(stream);
            if(!ent){if(errno)++out.skipped;return;}
            std::string name(ent->d_name);if(name=="."||name=="..")continue;
            ++out.visited;struct stat st{};
            if(::fstatat(dir,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)){++out.skipped;continue;}
            if(st.st_dev!=rootStat.st_dev){++out.skipped;continue;}
            const auto full=path/name;
            if(S_ISDIR(st.st_mode)){
                Fd child(::openat(dir,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat actual{};
                if(child.n<0||::fstat(child.n,&actual)||actual.st_dev!=st.st_dev||actual.st_ino!=st.st_ino){++out.skipped;continue;}
                self(self,child.n,full,depth+1);continue;
            }
            if(!S_ISREG(st.st_mode)||st.st_size<0)continue;
            ++out.files;const auto bytes=static_cast<std::uint64_t>(st.st_size);
            const auto kind=category(full,cache);
            const bool unique=st.st_nlink==1||seen.emplace(st.st_dev,st.st_ino).second;
            const auto blocks=st.st_blocks>=0&&static_cast<std::uint64_t>(st.st_blocks)<=std::numeric_limits<std::uint64_t>::max()/512?static_cast<std::uint64_t>(st.st_blocks)*512:0;
            if(unique){add(out.allocated,blocks);add(out.logical,bytes);}else ++out.hardlinkAliases;
            if(bytes>std::numeric_limits<std::uint64_t>::max()-out.categories[kind]){out.limited=true;return;}
            out.categories[kind]+=bytes;
            if(bytes<limits.minBytes)continue;
            Entry e{full,bytes,st.st_dev,st.st_ino,st.st_uid,st.st_mtim,st.st_ctim,kind,limits.allowRemoval&&kind==Cache&&safeCache(st,limits.ageDays,now)};
            e.allocated=blocks;
            if(limits.collectDuplicates&&bytes>=1048576&&st.st_nlink==1&&st.st_uid==::getuid()){
                if(out.duplicateInput.size()<10000)out.duplicateInput.push_back(e);else out.duplicateInputLimited=true;
            }
            retain(out.largest,e,limits.top);
            if(e.kind==Review)retain(out.review,e,limits.top);
            if(e.removable)retain(out.candidates,std::move(e),limits.top);
        }
    };
    visit(visit,fd.n,root,0);
    const auto descending=[](const Entry& a,const Entry& b){return a.bytes>b.bytes;};
    std::sort(out.largest.begin(),out.largest.end(),descending);std::sort(out.candidates.begin(),out.candidates.end(),descending);std::sort(out.review.begin(),out.review.end(),descending);
    out.complete=!out.cancelled&&!out.limited&&!out.skipped;return out;
}
// Resolve the explicitly requested scan root once. Traversal and deletion still use
// no-follow descriptor walks. A redirected root or nonstandard cache is inspection-only.
inline Result scanRequested(const fs::path& requested,const fs::path& cache,const fs::path& cleanupCache,Limits limits,const std::atomic_bool& cancel,bool quick){
    Result out;
    if(cancel.load()){out.cancelled=true;return out;}
    if(!requested.is_absolute()){out.error="Choose an absolute folder path.";return out;}
    std::error_code ec;const auto resolved=fs::canonical(requested,ec);
    if(ec){
        const auto failure=ec;ec.clear();const auto state=fs::symlink_status(requested,ec);
        if(quick&&failure==std::errc::no_such_file_or_directory&&state.type()==fs::file_type::not_found){
            ec.clear();const auto parent=fs::canonical(requested.parent_path(),ec);
            if(!ec){Fd parentFd(openDir(parent));if(parentFd.n>=0){out.volume=volume(parentFd.n);out.complete=true;out.missingCache=true;out.readOnly=true;out.resolvedRoot=requested;return out;}}
        }
        out.error="Cannot open scan directory: "+failure.message()+". Choose an accessible folder or use Deep scan.";return out;
    }
    ec.clear();const auto resolvedCache=fs::canonical(cache,ec);
    const auto classification=ec?fs::path{}:resolvedCache;
    const bool redirected=resolved!=requested.lexically_normal();
    const bool standardCache=!classification.empty()&&classification==cleanupCache&&cache==cleanupCache;
    limits.allowRemoval=limits.allowRemoval&&!redirected&&standardCache;
    out=scan(resolved,classification,limits,cancel);out.redirected=redirected;out.readOnly=!limits.allowRemoval;
    return out;
}
// Only selected old user-cache regular files; no directory deletion, privilege escalation,
// links, personal-file removal or cross-device traversal. Revalidate metadata immediately
// before unlink; concurrent same-user modifications remain a residual race.
inline bool removeCache(const fs::path& cache,const Entry& entry,unsigned ageDays,std::time_t now=std::time(nullptr)){
    if(!entry.removable||!inside(entry.path,cache)||entry.path.filename().empty())return false;
    Fd anchor(openDir(cache));struct stat base{},st{};
    if(anchor.n<0||::fstat(anchor.n,&base)||base.st_uid!=::getuid())return false;
    Fd parent(::fcntl(anchor.n,F_DUPFD_CLOEXEC,3));if(parent.n<0)return false;
    auto rel=entry.path.lexically_relative(cache);
    for(const auto& part:rel.parent_path()){
        if(part.empty()||part=="."||part=="..")return false;
        Fd next(::openat(parent.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat dir{};
        if(next.n<0||::fstat(next.n,&dir)||dir.st_dev!=base.st_dev||dir.st_uid!=::getuid())return false;
        ::close(parent.n);parent.n=next.n;next.n=-1;
    }
    if(::fstatat(parent.n,entry.path.filename().c_str(),&st,AT_SYMLINK_NOFOLLOW))return false;
    if(st.st_dev!=base.st_dev||st.st_dev!=entry.dev||st.st_ino!=entry.ino||st.st_uid!=entry.uid||st.st_size<0||static_cast<std::uint64_t>(st.st_size)!=entry.bytes||!sameTime(st.st_mtim,entry.mtime)||!sameTime(st.st_ctim,entry.ctime)||!safeCache(st,ageDays,now))return false;
    return ::unlinkat(parent.n,entry.path.filename().c_str(),0)==0;
}
}
