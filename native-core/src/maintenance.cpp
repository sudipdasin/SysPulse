// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/maintenance.hpp"
#include "syspulse/native/apt_plan.hpp"
#include "syspulse/native/offline_storage.hpp"
#include <algorithm>
#include <sstream>
#include <set>
#if defined(__linux__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cerrno>
#include <array>
#include <cstdlib>
#endif
namespace syspulse::native {
namespace {bool unitOk(const std::string& n){return n.size()>8&&n.size()<=128&&n.front()!='-'&&n.substr(n.size()-8)==".service"&&std::all_of(n.begin(),n.end(),[](unsigned char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'||c=='@';});}}
std::optional<Command> commandFor(Operation a,const std::string& unit){
    if(a==Operation::StartService||a==Operation::StopService){if(!unitOk(unit))return {};return Command{"/usr/bin/systemctl",{"--user","--no-ask-password","--no-pager",a==Operation::StartService?"start":"stop",unit},false};}
    if(a==Operation::RestartUserService){if(!unitOk(unit))return {};return Command{"/usr/bin/systemctl",{"--user","--no-ask-password","--no-pager","restart",unit},false};}
    if(a==Operation::StartSystemService||a==Operation::StopSystemService||a==Operation::RestartSystemService){
        if(!unitOk(unit))return {};
        // Refuse stop/restart of foundational system units; recovery of a failed service can still be attempted by start.
        const bool critical=unit.rfind("systemd-",0)==0||unit.rfind("dbus",0)==0||unit=="polkit.service"||unit=="NetworkManager.service"||unit=="display-manager.service"||unit=="lightdm.service"||unit=="gdm.service"||unit=="sddm.service"||unit=="ssh.service"||unit=="sshd.service"||unit=="getty@.service"||unit.rfind("getty@",0)==0||unit.rfind("user@",0)==0||unit=="apparmor.service"||unit=="ufw.service"||unit=="firewalld.service";
        if(critical&&a!=Operation::StartSystemService)return {};
        return Command{"/usr/bin/pkexec",{"/usr/bin/systemctl","--system","--no-ask-password","--no-pager",a==Operation::StartSystemService?"start":a==Operation::StopSystemService?"stop":"restart",unit},true};
    }
    if(a==Operation::OfflineInfo){if(!offlinePath(unit))return {};return Command{"/usr/bin/udisksctl",{"info","--block-device",unit},false};}
    if(a==Operation::OfflineCheck||a==Operation::OfflineRepair){if(!offlineObject(unit))return {};return Command{"/usr/bin/gdbus",{"call","--system","--dest","org.freedesktop.UDisks2","--object-path",unit,"--method",a==Operation::OfflineCheck?"org.freedesktop.UDisks2.Filesystem.Check":"org.freedesktop.UDisks2.Filesystem.Repair","--timeout","1800","{}"},false};}
    if(a==Operation::ApplyUpgrade||a==Operation::ApplyRepair){if(!validPlanToken(unit))return {};return Command{"/usr/bin/pkexec",{"/usr/bin/syspulse-native-package-helper",a==Operation::ApplyUpgrade?"upgrade":"repair",unit},true};}
    if(!unit.empty())return {};
    switch(a){
    case Operation::OfflineInventory:return Command{"/usr/bin/lsblk",{"--json","--list","--output","PATH,UUID,FSTYPE,MAJ:MIN,TYPE,RO,MOUNTPOINTS,SIZE"},false};
    case Operation::ListSystemServices:return Command{"/usr/bin/systemctl",{"--system","--no-pager","--plain","--no-legend","list-units","--type=service","--all"},false};
    case Operation::NetworkStatus:return Command{"/usr/bin/ip",{"-brief","address","show"},false};
    case Operation::NetworkRoutes:return Command{"/usr/bin/ip",{"route","show"},false};
    case Operation::SocketStatus:return Command{"/usr/bin/ss",{"-lntu"},false};
    case Operation::WifiStatus:return Command{"/usr/bin/nmcli",{"radio","wifi"},false};
    case Operation::WifiEnable:return Command{"/usr/bin/nmcli",{"radio","wifi","on"},false};
    case Operation::WifiDisable:return Command{"/usr/bin/nmcli",{"radio","wifi","off"},false};
    case Operation::PowerStatus:return Command{"/usr/bin/powerprofilesctl",{"get"},false};
    case Operation::PowerBalanced:return Command{"/usr/bin/powerprofilesctl",{"set","balanced"},false};
    case Operation::PowerSaver:return Command{"/usr/bin/powerprofilesctl",{"set","power-saver"},false};
    case Operation::PowerPerformance:return Command{"/usr/bin/powerprofilesctl",{"set","performance"},false};
    case Operation::DiagnoseJournal:return Command{"/usr/bin/journalctl",{"--no-pager","-b","-p","warning","-n","100","-o","json","--output-fields=MESSAGE,PRIORITY,__CURSOR"},false};
    case Operation::JournalErrors:return Command{"/usr/bin/journalctl",{"--no-pager","-b","-p","err","-n","100","-o","short-iso"},false};
    case Operation::KernelErrors:return Command{"/usr/bin/journalctl",{"--no-pager","-k","-b","-p","warning","-n","100","-o","short-iso"},false};
    case Operation::FirewallStatus:return Command{"/usr/bin/pkexec",{"/usr/sbin/ufw","status","verbose"},true};
    case Operation::TimerStatus:return Command{"/usr/bin/systemctl",{"--system","--no-pager","--plain","list-timers","--all"},false};
    case Operation::CheckPackages:return Command{"/usr/bin/pkexec",{"/usr/bin/apt-get","check"},true};
    case Operation::ConfigurePackages:return Command{"/usr/bin/pkexec",{"/usr/bin/dpkg","--configure","-a"},true};
    case Operation::PreviewUpgrade:return Command{"/usr/bin/pkexec",{"/usr/bin/apt-get","-s","--no-remove","upgrade"},true};
    case Operation::PreviewRepair:return Command{"/usr/bin/pkexec",{"/usr/bin/apt-get","-s","--no-remove","--fix-broken","install"},true};
    case Operation::DiskInventory:return Command{"/usr/bin/lsblk",{"-o","NAME,TYPE,SIZE,FSTYPE,ROTA,DISC-GRAN,MOUNTPOINT"},false};
    case Operation::FailedServices:return Command{"/usr/bin/systemctl",{"--system","--no-pager","--plain","--failed","--type=service"},false};
    case Operation::PreviewTrim:return Command{"/usr/bin/pkexec",{"/usr/sbin/fstrim","--dry-run","--verbose","/"},true};
    case Operation::TrimRoot:return Command{"/usr/bin/pkexec",{"/usr/sbin/fstrim","--verbose","/"},true};
    case Operation::EnableCompression:return Command{"/usr/bin/pkexec",{"/usr/bin/syspulse-native-memory-helper","enable"},true};
    case Operation::DisableCompression:return Command{"/usr/bin/pkexec",{"/usr/bin/syspulse-native-memory-helper","disable"},true};
    case Operation::Audit:return Command{"/usr/bin/dpkg",{"--audit"},false};case Operation::PreviewClean:return Command{"/usr/bin/pkexec",{"/usr/bin/apt-get","-s","autoclean"},true};case Operation::Refresh:return Command{"/usr/bin/pkexec",{"/usr/bin/apt-get","update"},true};case Operation::Clean:return Command{"/usr/bin/pkexec",{"/usr/bin/apt-get","autoclean"},true};case Operation::ListServices:return Command{"/usr/bin/systemctl",{"--user","--no-pager","--plain","--no-legend","list-units","--type=service","--all"},false};default:return {};}}
std::vector<Service> parseServices(const std::string& text){std::vector<Service> out;if(text.size()>65536)return out;std::istringstream in(text);std::string line;std::set<std::string> seen;while(std::getline(in,line)){std::istringstream r(line);std::string name,load,active,sub;if(!(r>>name>>load>>active>>sub)||!unitOk(name)||load!="loaded")continue;if(!seen.insert(name).second||out.size()>=256)return {};if(active=="active"||active=="inactive"||active=="failed"||active=="activating"||active=="deactivating")out.push_back({name,active});}return out;}
Job::~Job(){
#if defined(__linux__)
if(fd_>=0)close(fd_);
if(pid_>0){int status;waitpid(pid_,&status,WNOHANG);}
#endif
}
namespace {
bool ordinaryRequired(const Command& c){
    if(c.admin||c.exe=="/usr/bin/gdbus")return true;
    if(c.exe=="/usr/bin/systemctl"&&c.args.size()>=5&&c.args.front()=="--user"&&(c.args[3]=="start"||c.args[3]=="stop"||c.args[3]=="restart"))return true;
    if(c.exe=="/usr/bin/nmcli"&&c.args.size()==3)return true;
    if(c.exe=="/usr/bin/powerprofilesctl"&&c.args.size()==2&&c.args.front()=="set")return true;
    return false;
}
bool permitted(const Command& c){
    const auto match=[&](const std::optional<Command>& a){return a&&a->exe==c.exe&&a->args==c.args&&a->admin==c.admin;};
    for(auto op:{Operation::Audit,Operation::PreviewClean,Operation::Refresh,Operation::Clean,Operation::ListServices,Operation::CheckPackages,Operation::ConfigurePackages,Operation::PreviewUpgrade,Operation::ApplyUpgrade,Operation::PreviewRepair,Operation::ApplyRepair,Operation::DiskInventory,Operation::FailedServices,Operation::PreviewTrim,Operation::TrimRoot,Operation::EnableCompression,Operation::DisableCompression,Operation::ListSystemServices,Operation::NetworkStatus,Operation::NetworkRoutes,Operation::SocketStatus,Operation::WifiStatus,Operation::WifiEnable,Operation::WifiDisable,Operation::PowerStatus,Operation::PowerBalanced,Operation::PowerSaver,Operation::PowerPerformance,Operation::JournalErrors,Operation::KernelErrors,Operation::DiagnoseJournal,Operation::FirewallStatus,Operation::TimerStatus,Operation::OfflineInventory})if(match(commandFor(op)))return true;
    if(c.args.size()==11&&(match(commandFor(Operation::OfflineCheck,c.args[5]))||match(commandFor(Operation::OfflineRepair,c.args[5]))))return true;
    return !c.args.empty()&&(match(commandFor(Operation::OfflineInfo,c.args.back()))||match(commandFor(Operation::StartService,c.args.back()))||match(commandFor(Operation::StopService,c.args.back()))||match(commandFor(Operation::RestartUserService,c.args.back()))||match(commandFor(Operation::StartSystemService,c.args.back()))||match(commandFor(Operation::StopSystemService,c.args.back()))||match(commandFor(Operation::RestartSystemService,c.args.back()))||match(commandFor(Operation::ApplyUpgrade,c.args.back()))||match(commandFor(Operation::ApplyRepair,c.args.back())));
}
}
bool Job::start(const Command& c){
#if defined(__linux__)
    struct stat st{};if(active()||!permitted(c)||c.exe.rfind("/usr/bin/",0)!=0||stat(c.exe.c_str(),&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||(st.st_mode&0022)||access(c.exe.c_str(),X_OK)||c.args.size()>16||(ordinaryRequired(c)&&(!getuid()||getuid()!=geteuid())))return false;
    std::vector<char*> args{const_cast<char*>(c.exe.c_str())};for(auto& a:c.args){if(a.size()>256||a.find('\0')!=a.npos)return false;args.push_back(const_cast<char*>(a.c_str()));}args.push_back(nullptr);
    int p[2];if(pipe2(p,O_CLOEXEC))return false;int flags=fcntl(p[0],F_GETFL);if(flags<0||fcntl(p[0],F_SETFL,flags|O_NONBLOCK)){close(p[0]);close(p[1]);return false;}auto child=fork();if(child<0){close(p[0]);close(p[1]);return false;}if(child==0){close(p[0]);if(dup2(p[1],1)<0||dup2(p[1],2)<0)_exit(126);close(p[1]);int n=open("/dev/null",O_RDONLY|O_CLOEXEC);if(n<0||dup2(n,0)<0)_exit(126);close(n);setenv("LC_ALL","C",1);setenv("SYSTEMD_COLORS","0",1);execv(c.exe.c_str(),args.data());_exit(127);}close(p[1]);pid_=child;fd_=p[0];exited_=false;result_={};return true;
#else
    (void)c;return false;
#endif
}
std::optional<Result> Job::poll(){
#if defined(__linux__)
    if(!active())return {};
    std::array<char,4096> b{};for(unsigned i=0;i<16&&fd_>=0;++i){auto n=read(fd_,b.data(),b.size());if(n>0){auto take=std::min<std::size_t>(65536-result_.output.size(),static_cast<std::size_t>(n));result_.output.append(b.data(),take);if(take<static_cast<std::size_t>(n))result_.truncated=true;}else if(n==0){close(fd_);fd_=-1;}else if(errno!=EAGAIN&&errno!=EINTR){close(fd_);fd_=-1;result_.truncated=true;}else break;}
    if(!exited_){int status{};auto r=waitpid(pid_,&status,WNOHANG);if(r==pid_){exited_=true;result_.code=WIFEXITED(status)?WEXITSTATUS(status):-1;}else if(r<0&&errno!=EINTR){exited_=true;}}
    if(exited_&&fd_<0){pid_=-1;return result_;}
#endif
    return {};
}
}
