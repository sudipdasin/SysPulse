// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/actions.hpp"
#include <algorithm>
#include <array>
#include <ctime>
#if defined(__linux__)
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>
#include <filesystem>
#include <fstream>
#endif
namespace syspulse::native {
namespace {
using Clock=std::chrono::steady_clock;
#if defined(__linux__)
struct Fd {int n;explicit Fd(int v=-1):n(v){}~Fd(){if(n>=0)close(n);}Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;};
bool ordinary(){return getuid()!=0&&getuid()==geteuid();}
// Adapted from SysPulse cleaner.hpp and boost.hpp: no-follow directory walk,
// account ownership, old single-link generated thumbnails and identity revalidation.
int openPath(const std::filesystem::path& p){if(!p.is_absolute())return -1;Fd f(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));if(f.n<0)return -1;for(const auto& part:p.relative_path()){if(part.empty()||part=="."||part=="..")return -1;int next=openat(f.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(next<0)return -1;close(f.n);f.n=next;}int result=f.n;f.n=-1;return result;}
bool ownedDir(int fd){struct stat s{};return fd>=0&&!fstat(fd,&s)&&S_ISDIR(s.st_mode)&&s.st_uid==getuid()&&!(s.st_mode&0022);}
std::uint64_t mountId(int fd){struct statx s{};return fd>=0&&!statx(fd,"",AT_EMPTY_PATH|AT_STATX_DONT_SYNC,STATX_MNT_ID,&s)&&(s.stx_mask&STATX_MNT_ID)?s.stx_mnt_id:0;}
int cacheRoot(){if(!ordinary())return -1;auto* pw=getpwuid(getuid());if(!pw||!pw->pw_dir)return -1;Fd home(openPath(pw->pw_dir));if(!ownedDir(home.n))return -1;int cache=openat(home.n,".cache",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(!ownedDir(cache)){if(cache>=0)close(cache);return -1;}return cache;}
bool thumbnail(const std::string& n){return n.size()==36&&n.substr(32)==".png"&&std::all_of(n.begin(),n.begin()+32,[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
bool eligible(const struct stat& s,std::time_t now){return ordinary()&&S_ISREG(s.st_mode)&&s.st_uid==getuid()&&s.st_nlink==1&&s.st_size>=0&&std::difftime(now,s.st_mtime)>=30*86400.0&&std::difftime(now,s.st_ctime)>=30*86400.0;}
bool equal(const struct stat& a,const struct stat& b){return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_uid==b.st_uid&&a.st_size==b.st_size&&a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;}
struct Entry{std::string size,name;struct stat info;};
int sizeDir(int cache,const std::string& size){auto m=mountId(cache);if(!m)return -1;Fd t(openat(cache,"thumbnails",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));if(!ownedDir(t.n)||mountId(t.n)!=m)return -1;int d=-1;if(size=="fail/tumbler"||size=="fail/gnome-thumbnail-factory"){Fd failed(openat(t.n,"fail",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));if(!ownedDir(failed.n)||mountId(failed.n)!=m)return -1;d=openat(failed.n,size.substr(5).c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);}else if(size=="normal"||size=="large"||size=="x-large"||size=="xx-large")d=openat(t.n,size.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);else return -1;if(!ownedDir(d)||mountId(d)!=m){if(d>=0)close(d);return -1;}return d;}
bool removeEntry(int cache,const Entry& e,std::time_t now){Fd d(sizeDir(cache,e.size));struct stat st{},actual{};if(d.n<0||!thumbnail(e.name)||fstatat(d.n,e.name.c_str(),&st,AT_SYMLINK_NOFOLLOW)||!equal(st,e.info)||!eligible(st,now))return false;Fd f(openat(d.n,e.name.c_str(),O_PATH|O_NOFOLLOW|O_CLOEXEC));return f.n>=0&&mountId(f.n)==mountId(cache)&&!fstat(f.n,&actual)&&equal(st,actual)&&eligible(actual,now)&&!unlinkat(d.n,e.name.c_str(),0);}
std::optional<Process> ownedProcess(std::uint32_t pid){if(!ordinary()||pid<=1||pid==static_cast<std::uint32_t>(getpid())||pid==static_cast<std::uint32_t>(getppid()))return {};auto path="/proc/"+std::to_string(pid);Fd dir(open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat st{};if(dir.n<0||fstat(dir.n,&st)||st.st_uid!=getuid())return {};Fd f(openat(dir.n,"stat",O_RDONLY|O_NOFOLLOW|O_CLOEXEC));if(f.n<0)return {};std::array<char,65536> b{};auto n=read(f.n,b.data(),b.size());if(n<=0||static_cast<std::size_t>(n)==b.size())return {};return parseProcess(std::string_view(b.data(),static_cast<std::size_t>(n)));}
#endif
}
struct CacheReview::Impl {
#if defined(__linux__)
    std::vector<Entry> entries; Fd cache; Clock::time_point expiry{};
#endif
};
CacheReview::CacheReview():p(std::make_unique<Impl>()){} CacheReview::~CacheReview()=default;
void CacheReview::cancel(){p=std::make_unique<Impl>();}
CleanSummary CacheReview::scan(bool failedMarkers){cancel();CleanSummary s;
#if defined(__linux__)
    p->cache.n=cacheRoot();if(p->cache.n<0||!mountId(p->cache.n)){s.status="Ordinary account cache unavailable or protected.";return s;}
    auto deadline=Clock::now()+std::chrono::seconds(3);unsigned seen=0;auto now=std::time(nullptr);
    const std::vector<const char*> sizes=failedMarkers?std::vector<const char*>{"fail/tumbler","fail/gnome-thumbnail-factory"}:std::vector<const char*>{"normal","large","x-large","xx-large"};
    for(const char* size:sizes){Fd d(sizeDir(p->cache.n,size));if(d.n<0)continue;int copy=fcntl(d.n,F_DUPFD_CLOEXEC,3);if(copy<0){cancel();return {0,0,"Scan incomplete; nothing authorized."};}auto* raw=fdopendir(copy);if(!raw){close(copy);cancel();return {0,0,"Scan incomplete; nothing authorized."};}std::unique_ptr<DIR,int(*)(DIR*)> stream(raw,closedir);
        for(;;){errno=0;auto* e=readdir(stream.get());if(!e){if(errno){cancel();return {0,0,"Scan incomplete; nothing authorized."};}break;}if(++seen>25000||Clock::now()>deadline){cancel();return {0,0,"Scan limit reached; nothing authorized."};}std::string name=e->d_name;if(!thumbnail(name))continue;struct stat st{};if(fstatat(d.n,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)||!eligible(st,now))continue;Fd f(openat(d.n,name.c_str(),O_PATH|O_NOFOLLOW|O_CLOEXEC));struct stat actual{};if(f.n<0||mountId(f.n)!=mountId(p->cache.n)||fstat(f.n,&actual)||!equal(st,actual)||!eligible(actual,now))continue;if(p->entries.size()>=100)continue;if(static_cast<std::uint64_t>(st.st_size)>UINT64_MAX-s.bytes){cancel();return {0,0,"Byte count overflow; nothing authorized."};}p->entries.push_back({size,name,st});s.bytes+=static_cast<std::uint64_t>(st.st_size);}}
    s.files=p->entries.size();p->expiry=Clock::now()+std::chrono::seconds(120);s.status=s.files?(failedMarkers?"Old failed-thumbnail markers; removal may trigger retries.":"Reviewed old generated thumbnails. Confirm to clean."):(failedMarkers?"No eligible failed-thumbnail markers.":"No eligible old generated thumbnails in accessible folders.");
#else
    (void)failedMarkers;s.status="Unavailable on this platform.";
#endif
    return s;}
CleanResult CacheReview::apply(){CleanResult out;
#if defined(__linux__)
    auto review=std::move(p);p=std::make_unique<Impl>();if(!ordinary()||review->cache.n<0||Clock::now()>review->expiry){out.status="Review expired or unavailable; analyze again.";return out;}Fd current(cacheRoot());struct stat a{},b{};if(current.n<0||fstat(current.n,&a)||fstat(review->cache.n,&b)||a.st_dev!=b.st_dev||a.st_ino!=b.st_ino||mountId(current.n)!=mountId(review->cache.n)){out.status="Cache identity changed; nothing removed.";return out;}
    for(const auto& e:review->entries){if(!removeEntry(current.n,e,std::time(nullptr))){++out.skipped;continue;}++out.removed;out.bytes+=static_cast<std::uint64_t>(e.info.st_size);}
    out.status="Cleanup complete: logical file bytes, not guaranteed reclaimed blocks.";
#else
    out.status="Unavailable on this platform.";
#endif
    return out;}
ProcessReview::~ProcessReview(){cancel();}void ProcessReview::cancel(){
#if defined(__linux__)
    if(fd>=0)close(fd);
#endif
    fd=-1;pid=0;start=0;}
bool ProcessReview::prepare(const Process& row){cancel();
#if defined(__linux__) && defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
    if(!ordinary()||!row.startTicks||row.pid<=1||row.pid==static_cast<std::uint32_t>(getpid()))return false;
    fd=static_cast<int>(syscall(SYS_pidfd_open,row.pid,0));if(fd<0)return false;auto fresh=ownedProcess(row.pid);if(!fresh||fresh->pid!=row.pid||fresh->startTicks!=row.startTicks||fresh->state=='Z'||syscall(SYS_pidfd_send_signal,fd,0,nullptr,0)){cancel();return false;}pid=row.pid;start=row.startTicks;expiry=Clock::now()+std::chrono::seconds(30);return true;
#else
    (void)row;return false;
#endif
}
bool ProcessReview::terminate(){bool ok=false;
#if defined(__linux__) && defined(SYS_pidfd_send_signal)
    if(ordinary()&&fd>=0&&Clock::now()<=expiry){auto fresh=ownedProcess(pid);if(fresh&&fresh->pid==pid&&fresh->startTicks==start&&fresh->state!='Z')ok=syscall(SYS_pidfd_send_signal,fd,SIGTERM,nullptr,0)==0;}
#endif
    cancel();return ok;}
#if defined(SYSPULSE_ACTION_FIXTURE) && defined(__linux__)
int cleanupFixture(){
    if(!ordinary())return 77;
    char name[]="/tmp/sp-clean-fixture-XXXXXX"; if(!mkdtemp(name))return 1;
    const std::filesystem::path root=name;
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}} cleanup{root};
    auto d=root/"thumbnails/normal";std::filesystem::create_directories(d);
    auto good=d/"0123456789abcdef0123456789abcdef.png", personal=d/"photo.png", linked=d/"11111111111111111111111111111111.png";
    std::ofstream(good)<<"generated";std::ofstream(personal)<<"personal";std::filesystem::create_symlink(personal,linked);
    Fd fd(openPath(root));struct stat st{};if(lstat(good.c_str(),&st))return 2;
    Entry e{"normal",good.filename().string(),st};auto now=std::time(nullptr),future=now+31*86400;
    if(removeEntry(fd.n,e,now)||!std::filesystem::exists(good))return 3;
    auto changed=e;changed.info.st_size++;if(removeEntry(fd.n,changed,future))return 4;
    auto wrong=e;wrong.name=personal.filename();if(removeEntry(fd.n,wrong,future))return 5;
    wrong.name=linked.filename();if(removeEntry(fd.n,wrong,future))return 6;
    if(!removeEntry(fd.n,e,future)||std::filesystem::exists(good)||!std::filesystem::exists(personal))return 7;
    auto fail=root/"thumbnails/fail/tumbler";std::filesystem::create_directories(fail);auto marker=fail/good.filename();std::ofstream(marker)<<"failed marker";if(lstat(marker.c_str(),&st))return 8;Entry failed{"fail/tumbler",marker.filename().string(),st};if(removeEntry(fd.n,failed,now)||!std::filesystem::exists(marker))return 9;if(!removeEntry(fd.n,failed,future)||std::filesystem::exists(marker))return 10;
    auto other=root/"thumbnails/fail/unknown";std::filesystem::create_directories(other);auto protectedMarker=other/good.filename();std::ofstream(protectedMarker)<<"protected";if(lstat(protectedMarker.c_str(),&st))return 11;Entry rejected{"fail/unknown",protectedMarker.filename().string(),st};if(removeEntry(fd.n,rejected,future)||!std::filesystem::exists(protectedMarker))return 12;
    return 0;
}
#endif

}
