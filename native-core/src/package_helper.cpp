// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/apt_plan.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
namespace {
constexpr char apt[]="/usr/bin/apt-get";
char path[]="PATH=/usr/sbin:/usr/bin:/sbin:/bin",locale[]="LC_ALL=C",home[]="HOME=/root";
char* environment[]={path,locale,home,nullptr};
void execute(bool repair,bool simulation){
    char* args[8]{};unsigned n=0;auto push=[&](const char* v){args[n++]=const_cast<char*>(v);};
    push(apt);if(simulation)push("-s");push("--no-remove");if(!simulation)push("--assume-yes");if(repair)push("--fix-broken");push(repair?"install":"upgrade");
    execve(apt,args,environment);perror("apt-get");_exit(127);
}
std::optional<std::string> simulate(bool repair){
    int fds[2];if(pipe2(fds,O_CLOEXEC))return {};
    const auto child=fork();if(child<0){close(fds[0]);close(fds[1]);return {};}
    if(child==0){close(fds[0]);if(dup2(fds[1],STDOUT_FILENO)<0||dup2(fds[1],STDERR_FILENO)<0)_exit(126);close(fds[1]);execute(repair,true);}
    close(fds[1]);std::string result;std::array<char,4096> buf{};bool valid=true;
    for(;;){auto n=read(fds[0],buf.data(),buf.size());if(n<0&&errno==EINTR)continue;if(n<0){valid=false;break;}if(n==0)break;if(static_cast<std::size_t>(n)>65536-result.size()){valid=false;break;}result.append(buf.data(),static_cast<std::size_t>(n));}
    close(fds[0]);if(!valid)kill(child,SIGTERM);int status{};pid_t waited;do{waited=waitpid(child,&status,0);}while(waited<0&&errno==EINTR);
    if(!valid||waited!=child||!WIFEXITED(status)||WEXITSTATUS(status)!=0)return {};
    return result;
}
}
int main(int argc,char** argv){
    if(argc!=3||getuid()!=0||geteuid()!=0)return 2;
    const std::string operation=argv[1],token=argv[2];if((operation!="upgrade"&&operation!="repair")||!syspulse::native::validPlanToken(token))return 2;
    struct stat st{};if(stat(apt,&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||(st.st_mode&0022)||access(apt,X_OK))return 3;
    const bool repair=operation=="repair";auto preview=simulate(repair);if(!preview||syspulse::native::aptPlanToken(operation,*preview)!=token){std::cerr<<"Package plan changed or cannot be verified. No changes made; run a fresh simulation.\n";return 4;}
    std::cout<<"Reviewed simulation matches. APT will acquire its own transaction locks; no package removals allowed.\n"<<std::flush;
    execute(repair,false);return 127;
}
