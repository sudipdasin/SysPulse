// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/admin_protocol.hpp"
#include "tux/trusted_command.hpp"
#include <QCoreApplication>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
int main(int argc,char** argv){
    if(::geteuid()!=0||argc!=1)return 1;
    // Do not inherit caller-controlled application config, shell or loader settings.
    ::clearenv();::setenv("PATH","/usr/bin:/usr/sbin:/bin:/sbin",1);::setenv("HOME","/root",1);
    ::setenv("LANG","C",1);::setenv("LC_ALL","C",1);::setenv("DEBIAN_FRONTEND","noninteractive",1);
    if(::chdir("/")!=0)return 1;
    ::umask(0077);
    QCoreApplication app(argc,argv);
    ::signal(SIGPIPE,SIG_IGN);
    std::cout<<"SYSPULSE_ADMIN_V1\n"<<std::flush;
    // Private inherited channel only. No listening socket, password or persistent token.
    while(std::cout){
        char buffer[65];size_t n=0;char ch;
        while(std::cin.get(ch)&&ch!='\n'){if(n>=64)return 1;buffer[n++]=ch;}
        if(!std::cin||ch!='\n')return 0;
        const auto id=QString::fromLatin1(buffer,static_cast<int>(n));
        if(!tux::admin::supported(id))return 1;
        tux::Result result;result.code=1;
        try{
            const auto& a=tux::action(id);
            if(!a.admin||a.command.isEmpty())return 1;
            const auto problem=tux::unavailable(a);
            if(!problem.isEmpty())result.output=problem.toUtf8();
            else{
                auto cmd=a.command;const auto executable=tux::trustedSystemExecutable(cmd.first());
                if(executable.isEmpty())return 1;
                cmd[0]=executable;
                result=tux::runCommand(cmd,120000);
            }
        }catch(const std::exception&){result.code=1;result.output="Administrative action failed. Review system state.";}
        const auto reply=tux::admin::encode(result);std::cout.write(reply.constData(),reply.size());std::cout<<'\n'<<std::flush;
    }
    return 0;
}
