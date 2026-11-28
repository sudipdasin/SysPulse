// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/system_tools.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <linux/magic.h>
#include <iostream>
#include <string_view>
using namespace syspulse::native;
int main(int argc,char** argv){
    if(argc!=2||getuid()!=0||geteuid()!=0){std::cerr<<"Requires administrator authorization and one fixed action.\n";return 2;}
    const std::string_view action(argv[1]);if(action!="enable"&&action!="disable")return 2;
    const bool enable=action=="enable";if(!compressionChangeAllowed(compressionEvidence(),enable)){std::cerr<<"Current swap/compression configuration is unavailable or ineligible. No write.\n";return 3;}
    int fd=open("/sys/module/zswap/parameters/enabled",O_WRONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0){perror("Open zswap");return 4;}
    struct stat st{};struct statfs fs{};if(fstat(fd,&st)||fstatfs(fd,&fs)||!S_ISREG(st.st_mode)||st.st_uid!=0||fs.f_type!=SYSFS_MAGIC||!compressionChangeAllowed(compressionEvidence(),enable)){close(fd);return 5;}
    const char value=enable?'Y':'N';const auto n=write(fd,&value,1);const auto closed=close(fd);if(n!=1||closed){std::cerr<<"Write failed; check actual kernel state.\n";return 6;}
    if(compressionEvidence().state!=(enable?ZswapState::Enabled:ZswapState::Disabled)){std::cerr<<"Requested state not verified. Review current state.\n";return 7;}
    std::cout<<"Verified runtime zswap state: "<<(enable?"enabled":"disabled")<<". No persistent settings changed.\n";return 0;
}
