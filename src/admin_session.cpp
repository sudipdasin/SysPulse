// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/admin_session.hpp"
#include "tux/admin_protocol.hpp"
#include "tux/trusted_command.hpp"
#include <QElapsedTimer>
#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <algorithm>
extern char** environ;
namespace tux::admin {
namespace {
struct State{
    std::mutex mutex;std::atomic_bool live{false},working{false};
    int fd=-1;pid_t pid=-1;
    void close(){
        live=false;
        if(fd>=0){::shutdown(fd,SHUT_RDWR);::close(fd);fd=-1;}
        if(pid>0){
            auto child=pid;pid=-1;
            // EOF revokes idle broker; reap without blocking GUI during cancelled authentication.
            std::thread([child]{while(::waitpid(child,nullptr,0)<0&&errno==EINTR){};}).detach();
        }
    }
    ~State(){if(fd>=0){::shutdown(fd,SHUT_RDWR);::close(fd);}}
};
State& state(){static State s;return s;}
struct Work{std::atomic_bool& flag;explicit Work(std::atomic_bool& f):flag(f){flag=true;}~Work(){flag=false;}};
bool receive(int fd,QByteArray& line,int timeout,int limit){
    QElapsedTimer clock;clock.start();line.clear();
    while(clock.elapsed()<timeout){
        pollfd p{fd,POLLIN,0};int rc=::poll(&p,1,static_cast<int>(std::min<qint64>(1000,timeout-clock.elapsed())));
        if(rc<0){if(errno==EINTR)continue;return false;}if(!rc)continue;
        char buf[4096];const auto n=::recv(fd,buf,sizeof(buf),MSG_DONTWAIT);
        if(n<0&&(errno==EINTR||errno==EAGAIN))continue;
        if(n<=0)return false;
        line.append(buf,static_cast<int>(n));if(line.size()>limit)return false;
        if(line.contains('\n')){if(line.indexOf('\n')!=line.size()-1)return false;line.chop(1);return true;}
    }return false;
}
bool sendLine(int fd,const QByteArray& data){
    int sent=0;while(sent<data.size()){auto n=::send(fd,data.constData()+sent,data.size()-sent,MSG_NOSIGNAL|MSG_DONTWAIT);if(n<0&&errno==EINTR)continue;if(n<=0)return false;sent+=static_cast<int>(n);}return true;
}
}
bool active(){return state().live.load();}
bool busy(){return state().working.load();}
bool lock(){auto& s=state();std::unique_lock<std::mutex> guard(s.mutex,std::try_to_lock);if(!guard.owns_lock())return false;s.close();return true;}
bool unlock(QString& error){
    auto& s=state();std::lock_guard<std::mutex> guard(s.mutex);Work work(s.working);
    if(s.live)return true;
    s.close();
    if(::geteuid()==0){error="Use the normal desktop account.";return false;}
    const auto auth=trustedSystemExecutable("pkexec"),helper=trustedSystemExecutable("syspulse-admin-session");
    if(auth.isEmpty()||helper.isEmpty()){error="Administrator session helper or PolicyKit unavailable.";return false;}
    int raw[2];if(::socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,raw)){error="Cannot create private session transport.";return false;}
    int fd[2]={::fcntl(raw[0],F_DUPFD_CLOEXEC,3),::fcntl(raw[1],F_DUPFD_CLOEXEC,3)};::close(raw[0]);::close(raw[1]);
    if(fd[0]<0||fd[1]<0){for(int x:fd)if(x>=0)::close(x);error="Cannot allocate session transport.";return false;}
    posix_spawn_file_actions_t files;int rc=posix_spawn_file_actions_init(&files);
    if(rc){::close(fd[0]);::close(fd[1]);error="Cannot initialize authorization.";return false;}
    auto add=[&](int code){if(!rc)rc=code;};
    add(posix_spawn_file_actions_adddup2(&files,fd[1],STDIN_FILENO));
    add(posix_spawn_file_actions_adddup2(&files,fd[1],STDOUT_FILENO));
    add(posix_spawn_file_actions_addopen(&files,STDERR_FILENO,"/dev/null",O_WRONLY,0));
    add(posix_spawn_file_actions_addclose(&files,fd[0]));add(posix_spawn_file_actions_addclose(&files,fd[1]));
    auto a=auth.toUtf8(),h=helper.toUtf8();char option[]="--disable-internal-agent";
    char* argv[]={a.data(),option,h.data(),nullptr};pid_t pid=-1;
    if(!rc)rc=posix_spawn(&pid,argv[0],&files,nullptr,argv,environ);
    posix_spawn_file_actions_destroy(&files);::close(fd[1]);
    if(rc){::close(fd[0]);error="Could not start desktop authorization.";return false;}
    s.fd=fd[0];s.pid=pid;QByteArray hello;
    if(!receive(s.fd,hello,90000,128)||hello!="SYSPULSE_ADMIN_V1"){s.close();error="Authorization cancelled, denied, timed out or unavailable. No session opened.";return false;}
    s.live=true;return true;
}
std::optional<Result> execute(const QString& id){
    if(!supported(id))return std::nullopt;
    auto& s=state();std::lock_guard<std::mutex> guard(s.mutex);
    if(!s.live)return std::nullopt;
    Work work(s.working);Result r;r.action=action(id).title;r.command=action(id).command;
    QByteArray line;
    if(!sendLine(s.fd,id.toUtf8()+"\n")||!receive(s.fd,line,125000,400001)||!decode(line,r)){
        s.close();r.code=125;r.output="Administrator session interrupted. Outcome uncertain; inspect system state before retrying. No automatic replay.\n";return r;
    }
    return r;
}
}
