#include "syspulse/boost.hpp"
#include <fstream>
#include <iostream>
int main(){
    using namespace syspulse::boost;
    const c::fs::path cache="/home/example/.cache";
    const auto good=cache/"thumbnails/normal/0123456789abcdef0123456789abcdef.png";
    if(!thumbnail(good,cache))return 1;
    for(const auto& bad:{cache/"thumbnails/normal/photo.png",cache/"thumbnails/normal/0123456789abcdef0123456789abcdef.jpg",cache/"thumbnails/fail/0123456789abcdef0123456789abcdef.png",cache/"thumbnails/normal/nested/0123456789abcdef0123456789abcdef.png",cache/"other/normal/0123456789abcdef0123456789abcdef.png",c::fs::path("/tmp/thumbnails/normal/0123456789abcdef0123456789abcdef.png")})
        if(thumbnail(bad,cache))return 2;
    char temp[]="/tmp/syspulse-boost-XXXXXX";if(!::mkdtemp(temp))return 3;
    struct Cleanup{c::fs::path p;~Cleanup(){std::error_code ec;c::fs::remove_all(p,ec);}} cleanup{temp};
    const auto root=cleanup.p/".cache",dir=root/"thumbnails/normal",file=dir/good.filename(),personal=root/"other.png";
    c::fs::create_directories(dir);std::ofstream(file)<<"generated";std::ofstream(personal)<<"personal";
    std::atomic_bool cancel{false};auto plan=prepare(root,cancel);
    if(!plan.files.empty())return 4; // fresh files are not eligible
    c::fs::create_directory_symlink(root,cleanup.p/"redirect");
    if(!prepare(cleanup.p/"redirect",cancel).files.empty())return 5;
    cancel=true;if(!prepare(root,cancel).files.empty())return 6;
    struct stat st{};if(::lstat(file.c_str(),&st))return 7;
    c::Entry entry{file,static_cast<std::uint64_t>(st.st_size),st.st_dev,st.st_ino,st.st_uid,st.st_mtim,st.st_ctim,c::Cache,true};
    plan.cache=root;plan.files={entry};plan.bytes=entry.bytes;
    if(apply(plan).removed||!c::fs::exists(file))return 8;
    auto changed=entry;changed.bytes++;plan.files={changed};
    const auto future=std::time(nullptr)+31*86400;
    if(apply(plan,future).removed||!c::fs::exists(file))return 9;
    plan.files.assign(MaxFiles+1,entry);if(apply(plan,future).removed)return 10;
    plan.files={entry};const auto result=apply(plan,future);
    if(::geteuid()!=0){if(result.removed!=1||result.bytes!=entry.bytes||c::fs::exists(file))return 11;}
    else if(result.removed)return 12;
    if(!c::fs::exists(personal))return 13;
    std::cout<<"Boost scope, age, redirected path, cancellation, changed identity, batch bounds and fixture deletion passed\n";
}
