// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/system_memory.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/magic.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
namespace {
int fail(const char* reason){std::fprintf(stderr,"%s\n",reason);return 1;}
}
int main(int argc,char** argv){
    using namespace syspulse::system_memory;
    if(argc!=2||(std::strcmp(argv[1],"enable")&&std::strcmp(argv[1],"disable")))return fail("Only enable or disable is accepted.");
    if(::geteuid()!=0)return fail("Administrator authorization required.");
    const bool enable=std::strcmp(argv[1],"enable")==0;
    const auto state=readEnabled();
    if(state==State::Unknown)return fail("Kernel zswap runtime control unavailable.");
    const auto desired=enable?State::Enabled:State::Disabled;
    if(state==desired){std::puts("Requested zswap state already present; no change.");return 0;}
    if(enable&&!eligible())return fail("Enable refused: ordinary active swap and no zram devices must be verified.");
    // Fixed kernel parameter only. No path, arbitrary values, configuration files or subprocesses from callers.
    const int fd=::open("/sys/module/zswap/parameters/enabled",O_WRONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return fail("Kernel zswap control cannot be opened.");
    struct statfs fs{};struct stat st{};
    if(::fstatfs(fd,&fs)||fs.f_type!=SYSFS_MAGIC||::fstat(fd,&st)||!S_ISREG(st.st_mode)){::close(fd);return fail("Refusing non-sysfs control.");}
    const char value=enable?'Y':'N';ssize_t n;
    do{n=::write(fd,&value,1);}while(n<0&&errno==EINTR);
    const int closed=::close(fd);
    if(n!=1||closed||readEnabled()!=desired)return fail("Kernel state not verified; inspect zswap status before retrying.");
    std::puts(enable?"System-wide zswap enabled for future swap writes. No immediate RAM increase guaranteed.":"New zswap admissions disabled. Existing compressed entries can remain until reclaimed or faulted in.");
    return 0;
}
