// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/cleaner.hpp"
#include <fstream>
#include <iostream>
int main(){
    using namespace syspulse::cleaner;
    char temp[]="/tmp/syspulse-cleaner-XXXXXX";if(!::mkdtemp(temp))return 1;
    struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{temp};
    const auto root=cleanup.p,cache=root/".cache";
    fs::create_directory(cache);fs::create_directory(root/"docs");
    std::ofstream(cache/"small")<<"123";std::ofstream(root/"docs"/"big.iso")<<std::string(100,'x');
    std::ofstream(root/"notes.log")<<"12345";fs::create_symlink(root/"docs"/"big.iso",cache/"link");fs::create_directory_symlink(root/"docs",root/"linkdir");
    std::atomic_bool cancel{false};Limits limits;limits.top=2;
    auto r=scan(root,cache,limits,cancel);
    if(!r.complete||r.files!=3||r.largest.size()!=2||r.largest[0].bytes!=100||r.largest[1].bytes!=5)return 2;
    if(r.categories[Cache]!=3||r.categories[Archives]!=100||r.categories[Review]!=5||!r.volume.known)return 3;
    if(!r.candidates.empty())return 4;
    if(!inside(cache/"small",cache)||inside(root/".cache-other"/"x",cache)||inside(cache,cache))return 5;
    if(scan(root/"linkdir",cache,limits,cancel).error.empty())return 6;
    limits.minBytes=101;r=scan(root,cache,limits,cancel);if(!r.largest.empty()||r.categories[Archives]!=100)return 7;
    limits.minBytes=0;limits.entries=1;r=scan(root,cache,limits,cancel);if(!r.limited||r.complete)return 8;
    cancel=true;r=scan(root,cache,limits,cancel);if(!r.cancelled||r.complete)return 9;
    cancel=false;limits.entries=100;limits.seconds=0;r=scan(root,cache,limits,cancel);if(!r.limited)return 10;
    struct stat st{};if(::stat((cache/"small").c_str(),&st))return 11;
    st.st_mtime=st.st_ctime=1;
    if(::geteuid()!=0&&!safeCache(st,7,1000000))return 12;
    st.st_nlink=2;if(safeCache(st,7,1000000))return 13;
    st.st_nlink=1;st.st_uid=::getuid()+1;if(safeCache(st,7,1000000))return 14;
    Entry fake;fake.path=root/"docs"/"big.iso";fake.removable=true;if(removeCache(cache,fake,7))return 15;
    fake.path=cache/"link";if(removeCache(cache,fake,7))return 16;
    fake.path=cache/"small";if(removeCache(cache,fake,7))return 17;
    if(!fs::exists(cache/"small")||!fs::exists(root/"docs"/"big.iso"))return 18;
    if(openDir(root/".."/root.filename())>=0)return 19;
    if(r.error.size())return 20;
    if(::geteuid()!=0){
        if(::stat((cache/"small").c_str(),&st))return 21;
        Entry valid{cache/"small",3,st.st_dev,st.st_ino,st.st_uid,st.st_mtim,st.st_ctim,Cache,true};
        if(removeCache(cache,valid,7))return 22;
        const auto future=std::time(nullptr)+8*86400;
        auto changed=valid;changed.bytes=4;if(removeCache(cache,changed,7,future))return 23;
        fs::create_hard_link(cache/"small",cache/"hard");if(removeCache(cache,valid,7,future))return 24;
        fs::remove(cache/"hard");if(::stat((cache/"small").c_str(),&st))return 25;valid.ctime=st.st_ctim;
        if(!removeCache(cache,valid,7,future)||fs::exists(cache/"small")||!fs::exists(root/"docs"/"big.iso"))return 26;
    }

    std::cout<<"Cleaner scan, rejection and actual fixture unlink checks passed\n";
}
