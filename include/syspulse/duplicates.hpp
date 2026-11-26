// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/cleaner.hpp"
#include <map>
#include <cstring>
namespace syspulse::cleaner {
struct DuplicateGroup { std::vector<Entry> files; };
struct DuplicateResult {std::vector<DuplicateGroup> groups;std::uint64_t logicalExtra=0,bytesRead=0;unsigned compared=0,unverified=0;bool limited=false,cancelled=false;};
inline bool unchanged(int fd,const Entry& e){
    struct stat s{};return !::fstat(fd,&s)&&S_ISREG(s.st_mode)&&s.st_nlink==1&&s.st_dev==e.dev&&s.st_ino==e.ino&&s.st_uid==e.uid&&s.st_uid==::getuid()&&s.st_size>=0&&static_cast<std::uint64_t>(s.st_size)==e.bytes&&sameTime(s.st_mtim,e.mtime)&&sameTime(s.st_ctim,e.ctime);
}
inline int openFile(const Entry& e){
    Fd parent(openDir(e.path.parent_path()));if(parent.n<0)return -1;
    Fd fd(::openat(parent.n,e.path.filename().c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));
    if(fd.n<0||!unchanged(fd.n,e))return -1;
    int result=fd.n;fd.n=-1;return result;
}
inline DuplicateResult duplicates(const std::vector<Entry>& entries,const std::atomic_bool& cancel,std::uint64_t budget=256ULL*1024*1024,unsigned seconds=15){
    DuplicateResult out;const auto start=std::chrono::steady_clock::now();
    const auto stop=[&]{if(cancel.load()){out.cancelled=true;return true;}if(std::chrono::steady_clock::now()-start>=std::chrono::seconds(seconds)||out.bytesRead>=budget||out.compared>=2000){out.limited=true;return true;}return false;};
    std::map<std::uint64_t,std::vector<const Entry*>> sizes;
    for(const auto& e:entries)if(e.bytes>=1048576)sizes[e.bytes].push_back(&e);
    const auto compare=[&](const Entry& a,const Entry& b)->int{
        if(a.dev==b.dev&&a.ino==b.ino)return 0;
        Fd x(openFile(a)),y(openFile(b));if(x.n<0||y.n<0)return -1;
        ++out.compared;std::array<char,65536> first{},second{};
        std::uint64_t left=a.bytes;
        while(left){
            if(stop())return -1;
            const auto n=static_cast<std::size_t>(std::min<std::uint64_t>(left,first.size()));
            if(budget-out.bytesRead<2*n){out.limited=true;return -1;}
            auto readExact=[&](int fd,char* buf){std::size_t pos=0;while(pos<n){if(cancel.load()){out.cancelled=true;return false;}const auto r=::read(fd,buf+pos,n-pos);if(r<0&&errno==EINTR)continue;if(r<=0)return false;pos+=static_cast<std::size_t>(r);out.bytesRead+=static_cast<std::uint64_t>(r);}return true;};
            if(!readExact(x.n,first.data())||!readExact(y.n,second.data()))return -1;
            if(std::memcmp(first.data(),second.data(),n)!=0)return 0;
            left-=n;
        }
        return unchanged(x.n,a)&&unchanged(y.n,b)?1:-1;
    };
    for(const auto& bucket:sizes){
        if(stop()||out.limited)break;
        if(bucket.second.size()<2)continue;
        std::vector<DuplicateGroup> groups;
        for(const auto* e:bucket.second){
            if(stop()||out.limited)break;
            bool matched=false;for(auto& group:groups){
                const int eq=compare(group.files.front(),*e);
                if(eq<0){++out.unverified;if(out.cancelled||out.limited)break;}
                if(eq==1){group.files.push_back(*e);matched=true;break;}
            }
            if(!matched&&!out.cancelled&&!out.limited)groups.push_back({{*e}});
        }
        for(auto& group:groups)if(group.files.size()>1){
            for(std::size_t i=1;i<group.files.size();++i){const auto n=group.files[i].bytes;if(n>std::numeric_limits<std::uint64_t>::max()-out.logicalExtra){out.limited=true;break;}out.logicalExtra+=n;}
            out.groups.push_back(std::move(group));
        }
    }
    return out;
}
}
