// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/startup.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <set>
#if defined(__linux__)
#include <cerrno>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <filesystem>
#endif
namespace syspulse::native {
namespace {
constexpr std::string_view suffix=".syspulse-disabled";
constexpr std::size_t limit=16*1024*1024;
using Clock=std::chrono::steady_clock;
bool ends(std::string_view s,std::string_view tail){return s.size()>=tail.size()&&s.substr(s.size()-tail.size())==tail;}
bool valid(std::string_view name){return name.size()>8&&name.size()<=40&&ends(name,".desktop")&&name.front()!='.'&&name.find_first_of("/\\\r\n\t")==name.npos&&std::all_of(name.begin(),name.end(),[](unsigned char c){return c>=32&&c<127;});}
#if defined(__linux__)
struct Fd{int n;explicit Fd(int x=-1):n(x){}~Fd(){if(n>=0)close(n);}Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;};
bool ordinary(){return getuid()!=0&&getuid()==geteuid();}
bool owned(int f){struct stat s{};return f>=0&&!fstat(f,&s)&&S_ISDIR(s.st_mode)&&s.st_uid==getuid()&&!(s.st_mode&0022);}
int pathDir(const std::filesystem::path& path){if(!path.is_absolute())return -1;Fd fd(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));for(auto& part:path.relative_path()){if(fd.n<0||part.empty()||part=="."||part=="..")return -1;int next=openat(fd.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(fd.n);fd.n=next;}int r=fd.n;fd.n=-1;return r;}
int startupDir(){if(!ordinary())return -1;auto* pw=getpwuid(getuid());if(!pw||!pw->pw_dir)return -1;std::filesystem::path home(pw->pw_dir);const char* xdg=getenv("XDG_CONFIG_HOME");if(xdg&&*xdg&&std::filesystem::path(xdg)!=home/".config")return -1;Fd h(pathDir(home));if(!owned(h.n))return -1;Fd config(openat(h.n,".config",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));if(!owned(config.n))return -1;int d=openat(config.n,"autostart",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(!owned(d)){if(d>=0)close(d);return -1;}return d;}
std::string source(const StartupEntry& e){return e.name+(e.disabled?std::string(suffix):"");}
std::string destination(const StartupEntry& e){return e.name+(e.disabled?"":std::string(suffix));}
bool inspect(int dir,const std::string& name,struct stat& st){if(fstatat(dir,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)||!S_ISREG(st.st_mode)||st.st_uid!=getuid()||st.st_nlink!=1||(st.st_mode&0022)||st.st_size<=0||st.st_size>65536)return false;Fd file(openat(dir,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));struct stat actual{};if(file.n<0||fstat(file.n,&actual)||actual.st_dev!=st.st_dev||actual.st_ino!=st.st_ino||actual.st_size!=st.st_size||actual.st_ctim.tv_sec!=st.st_ctim.tv_sec||actual.st_ctim.tv_nsec!=st.st_ctim.tv_nsec)return false;std::array<char,65537> buf{};auto n=read(file.n,buf.data(),buf.size());if(n!=st.st_size)return false;std::string_view text(buf.data(),static_cast<std::size_t>(n));return text.find('\0')==text.npos&&(text.rfind("[Desktop Entry]\n",0)==0||text.rfind("[Desktop Entry]\r\n",0)==0);}
bool same(const struct stat& a,const struct stat& b){return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_uid==b.st_uid&&a.st_size==b.st_size&&a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec&&a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec;}
bool absent(int dir,const std::string& name){struct stat s{};return fstatat(dir,name.c_str(),&s,AT_SYMLINK_NOFOLLOW)!=0&&errno==ENOENT;}
// Removing a user override could expose a system-wide entry with the same name.
// Refuse when any configured XDG system directory has such an entry or cannot be checked.
bool noGlobalEntry(const std::string& name){
    if(!valid(name))return false;
    const char* env=getenv("XDG_CONFIG_DIRS");std::string dirs=env&&*env?env:"/etc/xdg";
    if(dirs.size()>8192)return false;
    std::size_t pos=0;unsigned count=0;
    while(pos<=dirs.size()){
        auto end=dirs.find(':',pos);auto part=dirs.substr(pos,end-pos);
        if(++count>32||part.empty()||part.front()!='/')return false;
        std::error_code ec;auto st=std::filesystem::symlink_status(std::filesystem::path(part)/"autostart"/name,ec);
        if(ec&&ec!=std::errc::no_such_file_or_directory)return false;
        if(st.type()!=std::filesystem::file_type::not_found)return false;
        if(end==std::string::npos)break;
        pos=end+1;
    }
    return true;
}
bool moveEntry(int dir,const StartupEntry& e,const struct stat& expected){
#if defined(SYS_renameat2)
    struct stat current{};if(!valid(e.name)||!inspect(dir,source(e),current)||!same(expected,current)||!absent(dir,destination(e)))return false;
    // RENAME_NOREPLACE preserves existing overrides and avoids overwriting any file.
    return syscall(SYS_renameat2,dir,source(e).c_str(),dir,destination(e).c_str(),1u)==0;
#else
    (void)dir;(void)e;(void)expected;return false;
#endif
}
#endif
}
struct StartupReview::Impl {
#if defined(__linux__)
    Fd dir; StartupEntry entry; struct stat identity{}; Clock::time_point expiry{};
#endif
};
StartupReview::StartupReview():p(std::make_unique<Impl>()){} StartupReview::~StartupReview()=default;
void StartupReview::cancel(){p=std::make_unique<Impl>();}
StartupInventory StartupReview::scan(){cancel();StartupInventory out;
#if defined(__linux__)
    Fd dir(startupDir());if(dir.n<0){out.status="Standard user startup directory unavailable or protected.";return out;}int copy=fcntl(dir.n,F_DUPFD_CLOEXEC,3);if(copy<0)return out;auto* raw=fdopendir(copy);if(!raw){close(copy);return out;}std::unique_ptr<DIR,int(*)(DIR*)> stream(raw,closedir);unsigned examined=0;
    for(;;){errno=0;auto* item=readdir(stream.get());if(!item){out.complete=errno==0;break;}if(++examined>2048){break;}std::string name=item->d_name;bool disabled=ends(name,suffix);if(disabled)name.resize(name.size()-suffix.size());if(!valid(name))continue;StartupEntry e{name,disabled};struct stat st{};if(noGlobalEntry(e.name)&&inspect(dir.n,source(e),st)&&absent(dir.n,destination(e)))out.entries.push_back(std::move(e));}
    std::sort(out.entries.begin(),out.entries.end(),[](auto& a,auto& b){return a.name<b.name;});out.status=out.complete?"User startup files only; system entries unchanged.":"Incomplete inventory; review unavailable.";if(!out.complete)out.entries.clear();
#else
    out.status="Unavailable on this platform.";
#endif
    return out;}
bool StartupReview::prepare(const StartupEntry& e){cancel();
#if defined(__linux__)
    if(!valid(e.name)||!noGlobalEntry(e.name))return false;
    p->dir.n=startupDir();if(p->dir.n<0||!inspect(p->dir.n,source(e),p->identity)||!absent(p->dir.n,destination(e))){cancel();return false;}p->entry=e;p->expiry=Clock::now()+std::chrono::seconds(60);return true;
#else
    (void)e;return false;
#endif
}
bool StartupReview::apply(){bool result=false;
#if defined(__linux__)
    Fd current(startupDir());struct stat a{},b{};if(current.n>=0&&p->dir.n>=0&&Clock::now()<=p->expiry&&!fstat(current.n,&a)&&!fstat(p->dir.n,&b)&&a.st_dev==b.st_dev&&a.st_ino==b.st_ino)result=noGlobalEntry(p->entry.name)&&moveEntry(current.n,p->entry,p->identity);
#endif
    cancel();return result;}
PackageAudit parsePackageStatus(std::string_view text){PackageAudit out;if(text.size()>limit||text.find('\0')!=text.npos)return out;bool seenPackage=false,seenStatus=false;std::string_view status;std::set<std::string> ids;std::string id,arch;auto finish=[&](){if(!seenPackage&&!seenStatus)return true;if(!seenPackage||!seenStatus||id.empty()||arch.empty()||!ids.insert(id+":"+arch).second)return false;if(status=="install ok installed"||status=="hold ok installed")++out.installed;else if(ends(status," config-files")||ends(status," not-installed"))++out.residual;else if(ends(status," unpacked")||ends(status," half-installed")||ends(status," half-configured")||ends(status," triggers-awaited")||ends(status," triggers-pending")||status.find(" reinstreq ")!=status.npos)++out.incomplete;else ++out.other;seenPackage=seenStatus=false;id.clear();arch.clear();return true;};
    while(!text.empty()){auto e=text.find('\n');auto row=text.substr(0,e);text=e==text.npos?std::string_view{}:text.substr(e+1);if(row.size()>65536)return {};if(row.empty()){if(!finish())return {};continue;}if(row.rfind("Package: ",0)==0){if(seenPackage)return {};seenPackage=true;id=row.substr(9);}else if(row.rfind("Architecture: ",0)==0){if(!arch.empty())return {};arch=row.substr(14);}else if(row.rfind("Status: ",0)==0){if(seenStatus)return {};seenStatus=true;status=row.substr(8);}}
    if(!finish())return {};
    out.complete=!ids.empty();return out;}
PackageAudit auditPackages(){
#if defined(__linux__)
    std::ifstream in("/var/lib/dpkg/status",std::ios::binary);if(!in)return {};std::string data;std::array<char,8192> b{};while(in){in.read(b.data(),b.size());auto n=static_cast<std::size_t>(in.gcount());if(n>limit-data.size())return {};data.append(b.data(),n);}if(in.bad()||!in.eof())return {};return parsePackageStatus(data);
#else
    return {};
#endif
}
#if defined(SYSPULSE_STARTUP_FIXTURE) && defined(__linux__)
int startupFixture(){if(!ordinary())return 77;char path[]="/tmp/sp-startup-XXXXXX";if(!mkdtemp(path))return 1;struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);}} cleanup{path};Fd d(pathDir(path));StartupEntry e{"fixture.desktop",false};auto f=cleanup.p/e.name;std::ofstream(f)<<"[Desktop Entry]\nType=Application\nExec=/bin/true\n";struct stat st{};if(!inspect(d.n,e.name,st))return 2;std::ofstream(cleanup.p/destination(e))<<"collision";if(moveEntry(d.n,e,st))return 3;std::filesystem::remove(cleanup.p/destination(e));auto altered=st;altered.st_size++;if(moveEntry(d.n,e,altered))return 4;if(!moveEntry(d.n,e,st)||std::filesystem::exists(f))return 5;e.disabled=true;if(!inspect(d.n,source(e),st)||!moveEntry(d.n,e,st)||!std::filesystem::exists(f))return 6;std::ifstream in(f);std::string line;std::getline(in,line);if(line!="[Desktop Entry]")return 7;StartupEntry linked{"link.desktop",false};std::filesystem::create_symlink(f,cleanup.p/linked.name);if(inspect(d.n,linked.name,st))return 8;return 0;}
#endif
}
