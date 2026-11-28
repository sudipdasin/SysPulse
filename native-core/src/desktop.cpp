// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/desktop.hpp"
#include <sstream>
#include <iomanip>
#include <locale>
#include <ctime>
#include <cmath>
#if defined(__linux__)
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/random.h>
#include <fcntl.h>
#include <pwd.h>
#include <spawn.h>
#include <cerrno>
extern char **environ;
#endif
namespace syspulse::native {
const std::array<DesktopTool,12>& desktopTools() noexcept {
    static constexpr std::array<DesktopTool,12> tools{{
        {"XFCE settings","/usr/bin/xfce4-settings-manager"},
        {"Display settings","/usr/bin/xfce4-display-settings"},
        {"Keyboard settings","/usr/bin/xfce4-keyboard-settings"},
        {"Mouse / touchpad","/usr/bin/xfce4-mouse-settings"},
        {"Appearance","/usr/bin/xfce4-appearance-settings"},
        {"Window manager","/usr/bin/xfwm4-tweaks-settings"},
        {"Power settings","/usr/bin/xfce4-power-manager-settings"},
        {"Session and startup","/usr/bin/xfce4-session-settings"},
        {"Disks and SMART","/usr/bin/gnome-disks"},
        {"Snapshot manager","/usr/bin/timeshift-gtk"},
        {"Firewall settings","/usr/bin/gufw"},
        {"Disk usage analyzer","/usr/bin/baobab"}
    }};return tools;
}
bool desktopAvailable(std::size_t index) noexcept {
#if defined(__linux__)
    if(index>=desktopTools().size())return false;
    const auto* path=desktopTools()[index].path;struct stat st{};
    return !stat(path,&st)&&S_ISREG(st.st_mode)&&st.st_uid==0&&!(st.st_mode&0022)&&!access(path,X_OK);
#else
    (void)index;return false;
#endif
}
DesktopLauncher::DesktopLauncher(){children_.reserve(12);}
void DesktopLauncher::reap() noexcept {
#if defined(__linux__)
    for(auto it=children_.begin();it!=children_.end();){int status{};auto r=waitpid(*it,&status,WNOHANG);if(r==*it||(r<0&&errno==ECHILD))it=children_.erase(it);else ++it;}
#endif
}
bool DesktopLauncher::launch(std::size_t index){
#if defined(__linux__)
    reap();if(!getuid()||getuid()!=geteuid()||getgid()!=getegid()||children_.size()>=12||!desktopAvailable(index))return false;
    const auto* path=desktopTools()[index].path;char* argv[]={const_cast<char*>(path),nullptr};pid_t child{};
    posix_spawn_file_actions_t actions;if(posix_spawn_file_actions_init(&actions))return false;
    int e=posix_spawn_file_actions_addopen(&actions,0,"/dev/null",O_RDONLY,0);
    if(!e)e=posix_spawn_file_actions_addopen(&actions,1,"/dev/null",O_WRONLY,0);
    if(!e)e=posix_spawn_file_actions_addopen(&actions,2,"/dev/null",O_WRONLY,0);
    if(!e)e=posix_spawn(&child,path,&actions,nullptr,argv,environ);
    posix_spawn_file_actions_destroy(&actions);if(e)return false;children_.push_back(child);return true;
#else
    (void)index;return false;
#endif
}
std::string healthReport(const HealthEvidence& e,const PressureEvidence& p,const Inspection& inventory){
    std::ostringstream out;out.imbue(std::locale::classic());out<<"SysPulse local system health report\n";
    const auto now=std::time(nullptr);std::tm tm{};
#if defined(__linux__)
    if(gmtime_r(&now,&tm))out<<"UTC: "<<std::put_time(&tm,"%Y-%m-%dT%H:%M:%SZ")<<'\n';
#else
    (void)now;(void)tm;
#endif
    out<<"Read-only snapshot; not a repair or security guarantee.\n";
    auto quantity=[&](const char* label,const std::optional<std::uint64_t>& value){out<<label<<": ";if(value)out<<*value;else out<<"unknown";out<<'\n';};
    const bool valid=e.memory&&e.memory->totalKiB&&e.memory->availableKiB<=e.memory->totalKiB;
    quantity("RAM total KiB",valid?std::optional<std::uint64_t>(e.memory->totalKiB):std::nullopt);
    quantity("RAM available KiB",valid?std::optional<std::uint64_t>(e.memory->availableKiB):std::nullopt);
    quantity("Swap total KiB",valid?e.memory->swapTotalKiB:std::nullopt);
    quantity("Swap used KiB",valid&&e.memory->swapTotalKiB&&e.memory->swapUsedKiB&&*e.memory->swapUsedKiB<=*e.memory->swapTotalKiB?e.memory->swapUsedKiB:std::nullopt);
    quantity("Root available bytes",triage::valid(e.rootBytes)?std::optional<std::uint64_t>(e.rootBytes->available):std::nullopt);
    quantity("CPU core rows",inventory.cpuAvailable?std::optional<std::uint64_t>(inventory.cores.size()):std::nullopt);
    quantity("Readable process rows (not total)",inventory.processAvailable?std::optional<std::uint64_t>(inventory.processes.size()):std::nullopt);
    if(inventory.processTruncated)out<<"Process inventory capped.\n";
    auto pressure=[&](const char* label,const std::optional<Pressure>& v){out<<label<<": ";if(v&&std::isfinite(v->some.avg10)&&v->some.avg10>=0&&v->some.avg10<=100)out<<std::fixed<<std::setprecision(2)<<v->some.avg10<<"%";else out<<"unknown";out<<'\n';};
    pressure("CPU stall avg10",p.cpu);pressure("Memory stall avg10",p.memory);pressure("I/O stall avg10",p.io);
    for(const auto& f:assessHealth(e)){const char* state="unknown";switch(f.severity){case triage::Severity::Ok:state="ok";break;case triage::Severity::Info:state="info";break;case triage::Severity::Warning:state="warning";break;case triage::Severity::Critical:state="critical";break;case triage::Severity::Unknown:break;}out<<state<<": "<<f.advice<<'\n';}
    return out.str();
}
ReportOutcome exportHealthReport(const std::string& text){ReportOutcome result;result.message="Report not saved.";
#if defined(__linux__)
    if(!getuid()||getuid()!=geteuid()||getgid()!=getegid()||text.empty()||text.size()>16384||text.find('\0')!=text.npos)return result;
    const auto* pw=getpwuid(getuid());if(!pw||!pw->pw_dir||pw->pw_dir[0]!='/')return result;std::string home(pw->pw_dir);
    // Resolve the account home via no-follow directory traversal, never trust HOME.
    int fd=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(fd<0)return result;
    struct Guard{int& fd;~Guard(){if(fd>=0)close(fd);}} guard{fd};
    std::size_t pos=1;for(unsigned depth=0;pos<home.size();++depth){if(depth>=128)return result;auto end=home.find('/',pos);auto part=home.substr(pos,end==home.npos?home.npos:end-pos);if(part.empty()||part=="."||part=="..")return result;int next=openat(fd,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(next<0)return result;close(fd);fd=next;if(end==home.npos)break;pos=end+1;}
    struct stat st{};if(fstat(fd,&st)||st.st_uid!=getuid()||(st.st_mode&0022))return result;
    unsigned char random[12]{};if(getrandom(random,sizeof random,0)!=static_cast<ssize_t>(sizeof random))return result;const char* hex="0123456789abcdef";std::string name="SysPulse-health-";for(auto b:random){name+=hex[b>>4];name+=hex[b&15];}name+=".txt";const auto tmp="."+name+".tmp";
    int file=openat(fd,tmp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(file<0)return result;
    struct Temp{int dir,file;const std::string& name;~Temp(){close(file);unlinkat(dir,name.c_str(),0);}} temp{fd,file,tmp};
    std::size_t offset=0;while(offset<text.size()){auto n=write(file,text.data()+offset,text.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0)return result;offset+=static_cast<std::size_t>(n);}
    if(fsync(file)||linkat(fd,tmp.c_str(),fd,name.c_str(),0))return result;
    result.published=true;result.path=home+"/"+name;bool removed=unlinkat(fd,tmp.c_str(),0)==0;result.durable=fsync(fd)==0;result.message=removed&&result.durable?"Report saved in your home folder: "+name:"Report published; verify directory sync or temporary cleanup: "+name;
#else
    (void)text;
#endif
    return result;}
}
