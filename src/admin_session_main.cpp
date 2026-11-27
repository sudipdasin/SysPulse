// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/admin_protocol.hpp"
#include "tux/trusted_command.hpp"
#include <QCoreApplication>
#include <QProcess>
#include <QElapsedTimer>
#include <array>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
namespace {
// Same reviewed command path as separate PolicyKit calls, with bounded retained output.
// Read-only volume helper enforces its own hard deadline. Mutations are not killed on timeout.
tux::Result runTyped(QStringList command,int limit){
    tux::Result r;r.code=125;
    auto program=tux::trustedSystemExecutable(command.takeFirst());
    if(program.isEmpty()){r.output="Trusted helper unavailable.";return r;}
    QProcess proc;proc.setProcessChannelMode(QProcess::MergedChannels);proc.setStandardInputFile(QProcess::nullDevice());
    QElapsedTimer timer;timer.start();proc.start(program,command);
    if(!proc.waitForStarted(10000)){r.output="Unable to launch trusted helper.";return r;}
    auto drain=[&]{auto data=proc.readAll();const int room=qMax(0,limit-r.output.size());if(data.size()>room)r.truncated=true;r.output.append(data.left(room));};
    while(proc.state()!=QProcess::NotRunning){proc.waitForReadyRead(100);drain();if(proc.state()!=QProcess::NotRunning)proc.waitForFinished(10);}
    drain();r.code=proc.exitStatus()==QProcess::NormalExit?qBound(0,proc.exitCode(),255):125;r.seconds=timer.elapsed()/1000.0;return r;
}
}
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
        QByteArray request;char ch;
        while(std::cin.get(ch)&&ch!='\n'){if(request.size()>=tux::admin::MaxRequest)return 1;request.append(ch);}
        if(!std::cin||ch!='\n')return 0;
        if(request.startsWith('[')){
            const auto typed=tux::admin::decodeRequest(request);
            if(typed.isEmpty())return 1;
            const auto command=tux::admin::typedCommand(typed);
            const auto result=runTyped(command,typed[0]=="volume"?tux::admin::MaxScanOutput:typed[0]=="service"?65536:4096);
            const auto reply=tux::admin::encode(result);std::cout.write(reply.constData(),reply.size());std::cout<<'\n'<<std::flush;
            continue;
        }
        const auto id=QString::fromLatin1(request);
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
