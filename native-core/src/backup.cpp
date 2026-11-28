// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/backup.hpp"
#include <array>
#include <chrono>
#include <limits>
#include <utility>
#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <openssl/evp.h>
#include <cerrno>
#endif
namespace syspulse::native {
#if defined(__linux__)
namespace {
struct Fd {int n=-1;explicit Fd(int x=-1):n(x){}~Fd(){if(n>=0)close(n);}Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;Fd(Fd&& x)noexcept:n(std::exchange(x.n,-1)){}Fd& operator=(Fd&& x)noexcept{if(this!=&x){if(n>=0)close(n);n=std::exchange(x.n,-1);}return *this;}};
constexpr std::uint64_t limit=64ULL*1024*1024;
bool ordinary(){return getuid()!=0&&getuid()==geteuid()&&getgid()==getegid();}
bool partOk(const std::string& s){if(s.empty()||s=="."||s==".."||s.size()>255)return false;for(unsigned char ch:s)if(ch<32||ch==127)return false;return true;}
struct Parent {Fd fd;std::string leaf;};
std::optional<Parent> parent(const std::string& path){if(path.size()<2||path.size()>4096||path.front()!='/')return {};Fd dir(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));if(dir.n<0)return {};std::size_t pos=1;for(unsigned i=0;i<128;++i){auto end=path.find('/',pos);auto part=path.substr(pos,end==path.npos?path.npos:end-pos);if(!partOk(part))return {};if(end==path.npos)return Parent{std::move(dir),part};Fd next(openat(dir.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));if(next.n<0)return {};dir=std::move(next);pos=end+1;}return {};}
bool same(const struct stat& a,const struct stat& b){return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_uid==b.st_uid&&a.st_size==b.st_size&&a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;}
bool validFile(const struct stat& a){return S_ISREG(a.st_mode)&&a.st_uid==getuid()&&a.st_nlink==1&&a.st_size>=0&&static_cast<std::uint64_t>(a.st_size)<=limit;}
bool absent(int fd,const std::string& leaf){struct stat st{};return fstatat(fd,leaf.c_str(),&st,AT_SYMLINK_NOFOLLOW)<0&&errno==ENOENT;}
bool ownedDir(int fd,struct stat& s){return !fstat(fd,&s)&&S_ISDIR(s.st_mode)&&s.st_uid==getuid()&&!(s.st_mode&0022);}
std::string hex(const unsigned char* b,std::size_t n){const char* d="0123456789abcdef";std::string out;out.reserve(n*2);for(std::size_t i=0;i<n;++i){out+=d[b[i]>>4];out+=d[b[i]&15];}return out;}
bool writeAll(int fd,const unsigned char* b,std::size_t n){while(n){auto w=write(fd,b,n);if(w<0&&errno==EINTR)continue;if(w<=0)return false;b+=w;n-=static_cast<std::size_t>(w);}return true;}
}
struct BackupReview::Impl {std::optional<CopyPlan> plan;struct stat source{},destDir{};std::chrono::steady_clock::time_point expires;};
#else
struct BackupReview::Impl {};
#endif
BackupReview::BackupReview():p(std::make_unique<Impl>()){} BackupReview::~BackupReview()=default;
void BackupReview::cancel(){
#if defined(__linux__)
    p->plan.reset();
#endif
}
std::optional<CopyPlan> BackupReview::prepare(const std::string& source,const std::string& destination){cancel();
#if defined(__linux__)
    if(!ordinary()||source==destination)return {};
    auto s=parent(source),d=parent(destination);if(!s||!d)return {};
    struct stat st{},dt{};Fd input(openat(s->fd.n,s->leaf.c_str(),O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC));
    if(input.n<0||fstat(input.n,&st)||!validFile(st)||!ownedDir(d->fd.n,dt)||!absent(d->fd.n,d->leaf))return {};
    p->source=st;p->destDir=dt;p->expires=std::chrono::steady_clock::now()+std::chrono::seconds(60);p->plan=CopyPlan{source,destination,static_cast<std::uint64_t>(st.st_size)};return p->plan;
#else
    (void)source;(void)destination;return {};
#endif
}
CopyOutcome BackupReview::apply(){CopyOutcome out;out.message="Copy refused; no destination published.";
#if defined(__linux__)
    auto plan=std::move(p->plan);p->plan.reset();if(!ordinary()||!plan||std::chrono::steady_clock::now()>p->expires)return out;
    auto s=parent(plan->source),d=parent(plan->destination);if(!s||!d)return out;
    struct stat st{},dt{};Fd input(openat(s->fd.n,s->leaf.c_str(),O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC));if(input.n<0||fstat(input.n,&st)||!validFile(st)||!same(st,p->source)||!ownedDir(d->fd.n,dt)||dt.st_dev!=p->destDir.st_dev||dt.st_ino!=p->destDir.st_ino||!absent(d->fd.n,d->leaf))return out;
    std::array<unsigned char,16> entropy{};if(getrandom(entropy.data(),entropy.size(),0)!=static_cast<ssize_t>(entropy.size()))return out;
    const auto name=".syspulse-copy-"+hex(entropy.data(),entropy.size());Fd temp(openat(d->fd.n,name.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));if(temp.n<0)return out;
    struct Temp {int fd;const std::string& name;~Temp(){if(fd>=0)unlinkat(fd,name.c_str(),0);}} cleanup{d->fd.n,name};
    auto ctx=std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>(EVP_MD_CTX_new(),EVP_MD_CTX_free);if(!ctx||EVP_DigestInit_ex(ctx.get(),EVP_sha256(),nullptr)!=1)return out;
    std::array<unsigned char,65536> buf{};std::uint64_t total=0;for(;;){auto n=read(input.n,buf.data(),buf.size());if(n<0&&errno==EINTR)continue;if(n<0)return out;if(!n)break;auto len=static_cast<std::size_t>(n);if(len>plan->bytes-total||!writeAll(temp.n,buf.data(),len)||EVP_DigestUpdate(ctx.get(),buf.data(),len)!=1)return out;total+=len;}
    struct stat finished{};if(total!=plan->bytes||fstat(input.n,&finished)||!same(st,finished)||!validFile(finished)||fsync(temp.n))return out;
    std::array<unsigned char,EVP_MAX_MD_SIZE> digest{},copied{};unsigned n=0,m=0;if(EVP_DigestFinal_ex(ctx.get(),digest.data(),&n)!=1||n!=32||lseek(temp.n,0,SEEK_SET)<0||EVP_DigestInit_ex(ctx.get(),EVP_sha256(),nullptr)!=1)return out;
    std::uint64_t verified=0;for(;;){auto got=read(temp.n,buf.data(),buf.size());if(got<0&&errno==EINTR)continue;if(got<0)return out;if(!got)break;auto len=static_cast<std::size_t>(got);if(len>total-verified||EVP_DigestUpdate(ctx.get(),buf.data(),len)!=1)return out;verified+=len;}
    if(verified!=total||EVP_DigestFinal_ex(ctx.get(),copied.data(),&m)!=1||m!=n||hex(digest.data(),n)!=hex(copied.data(),m))return out;
    // Verify the destination pathname still identifies the reviewed directory and the
    // temporary name still refers to the open file. linkat publishes without overwrite.
    auto current=parent(plan->destination);auto currentSource=parent(plan->source);struct stat now{},tempName{},tempFd{},sourceName{};
    if(!currentSource||fstatat(currentSource->fd.n,currentSource->leaf.c_str(),&sourceName,AT_SYMLINK_NOFOLLOW)||!same(st,sourceName)||!validFile(sourceName)||!current||!ownedDir(current->fd.n,now)||now.st_dev!=dt.st_dev||now.st_ino!=dt.st_ino||fstat(temp.n,&tempFd)||fstatat(d->fd.n,name.c_str(),&tempName,AT_SYMLINK_NOFOLLOW)||!same(tempName,tempFd)||!validFile(tempFd)||linkat(d->fd.n,name.c_str(),d->fd.n,d->leaf.c_str(),0))return out;
    out.published=true;out.bytes=total;out.sha256=hex(digest.data(),n);const bool removed=unlinkat(d->fd.n,name.c_str(),0)==0;if(removed)cleanup.fd=-1;out.durable=fsync(d->fd.n)==0;out.message=!removed?"Copy published; temporary name cleanup failed.":out.durable?"Verified copy created; original unchanged.":"Verified copy created; directory sync failed.";
#endif
    return out;}
}
