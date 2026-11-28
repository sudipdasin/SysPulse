// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from SysPulse Alpha39 cleaner.hpp bounded descriptor-based traversal,
// mount identity checks, hardlink accounting, categorization and top-N retention.
#include "syspulse/native/storage_scan.hpp"
#include <algorithm>
#include <filesystem>
#include <set>
#include <sstream>
#include <limits>
#include <memory>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
namespace syspulse::native {
namespace {
struct Fd {int n;explicit Fd(int v=-1):n(v){}~Fd(){if(n>=0)close(n);}Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;};
int openRoot(const std::string& p){std::filesystem::path root(p);if(!root.is_absolute()||p.size()>4096||p.find('\0')!=p.npos)return -1;Fd fd(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));if(fd.n<0)return -1;for(const auto& part:root.relative_path()){if(part.empty()||part=="."||part=="..")return -1;int next=openat(fd.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(next<0)return -1;close(fd.n);fd.n=next;}int n=fd.n;fd.n=-1;return n;}
std::uint64_t mountId(int fd){struct statx s{};return fd>=0&&!statx(fd,"",AT_EMPTY_PATH|AT_STATX_DONT_SYNC,STATX_MNT_ID,&s)&&(s.stx_mask&STATX_MNT_ID)?s.stx_mnt_id:0;}
FileCategory category(const std::string& path){if(path.rfind(".cache/",0)==0)return FileCategory::Cache;auto ext=std::filesystem::path(path).extension().string();for(auto& c:ext)if(c>='A'&&c<='Z')c=static_cast<char>(c+32);if(ext==".tmp"||ext==".bak"||ext==".old"||ext==".log"||ext==".part"||ext==".crdownload")return FileCategory::Review;if(ext==".zip"||ext==".gz"||ext==".xz"||ext==".7z"||ext==".tar"||ext==".iso"||ext==".deb"||ext==".rpm")return FileCategory::Archives;if(ext==".mp4"||ext==".mkv"||ext==".avi"||ext==".mp3"||ext==".wav"||ext==".jpg"||ext==".png"||ext==".webm")return FileCategory::Media;return FileCategory::Other;}
bool same(const struct stat& a,const struct stat& b){return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_size==b.st_size&&a.st_nlink==b.st_nlink&&a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;}
std::string display(std::string name){for(char& c:name)if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)>=127)c='?';return name;}
}
StorageScan scanStorage(const std::string& root,const ScanLimits& limits,const std::atomic_bool& cancel){StorageScan out;if(cancel.load()){out.cancelled=true;return out;}Fd base(openRoot(root));struct stat initial{};if(base.n<0||fstat(base.n,&initial))return out;auto mount=mountId(base.n);if(!mount)return out;out.available=true;auto maxEntries=std::clamp(limits.entries,1u,150000u), maxTop=std::clamp(limits.largest,1u,100u);const auto deadline=std::chrono::steady_clock::now()+std::clamp(limits.time,std::chrono::milliseconds(1),std::chrono::milliseconds(15000));
 std::set<std::pair<dev_t,ino_t>> links,dirs;dirs.emplace(initial.st_dev,initial.st_ino);auto stop=[&]{if(cancel.load()){out.cancelled=true;return true;}if(out.entries>=maxEntries||std::chrono::steady_clock::now()>=deadline){out.limited=true;return true;}return out.overflow;};auto add=[&](std::uint64_t& dst,std::uint64_t v){if(v>UINT64_MAX-dst)out.overflow=true;else dst+=v;};
 auto visit=[&](auto&& self,int fd,const std::string& prefix,unsigned depth)->void {if(stop())return;if(depth>=64){++out.skipped;return;}int copy=fcntl(fd,F_DUPFD_CLOEXEC,3);if(copy<0){++out.skipped;return;}DIR* stream=fdopendir(copy);if(!stream){close(copy);++out.skipped;return;}std::unique_ptr<DIR,int(*)(DIR*)> guard(stream,closedir);
  while(!stop()){errno=0;auto* e=readdir(stream);if(!e){if(errno)++out.skipped;return;}std::string name=e->d_name;if(name=="."||name=="..")continue;++out.entries;const auto path=prefix.empty()?name:prefix+"/"+name;if(path.size()>4096){++out.skipped;continue;}struct stat st{},actual{};if(fstatat(fd,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)){++out.skipped;continue;}if(!S_ISDIR(st.st_mode)&&!S_ISREG(st.st_mode))continue;Fd target(openat(fd,name.c_str(),(S_ISDIR(st.st_mode)?O_RDONLY|O_DIRECTORY:O_PATH)|O_NOFOLLOW|O_CLOEXEC));if(target.n<0||fstat(target.n,&actual)||!same(st,actual)){++out.skipped;continue;}const auto identity=mountId(target.n);if(!identity){++out.skipped;continue;}if(identity!=mount||actual.st_dev!=initial.st_dev){++out.mounts;continue;}if(S_ISDIR(actual.st_mode)){if(!dirs.emplace(actual.st_dev,actual.st_ino).second){++out.mounts;continue;}self(self,target.n,path,depth+1);continue;}
   if(!S_ISREG(actual.st_mode)||actual.st_size<0||actual.st_blocks<0||static_cast<std::uint64_t>(actual.st_blocks)>UINT64_MAX/512){++out.skipped;continue;}++out.files;if(actual.st_nlink>1&&!links.emplace(actual.st_dev,actual.st_ino).second){++out.aliases;continue;}const auto bytes=static_cast<std::uint64_t>(actual.st_size),blocks=static_cast<std::uint64_t>(actual.st_blocks)*512;const auto kind=category(path);add(out.logical,bytes);add(out.allocated,blocks);add(out.categoryBytes[static_cast<unsigned>(kind)],bytes);StorageEntry row{path,bytes,blocks,kind};auto cmp=[](const auto& a,const auto& b){return a.logical>b.logical;};if(out.largest.size()<maxTop){out.largest.push_back(std::move(row));std::push_heap(out.largest.begin(),out.largest.end(),cmp);}else if(bytes>out.largest.front().logical){std::pop_heap(out.largest.begin(),out.largest.end(),cmp);out.largest.back()=std::move(row);std::push_heap(out.largest.begin(),out.largest.end(),cmp);}
  }
 };visit(visit,base.n,"",0);std::sort(out.largest.begin(),out.largest.end(),[](const auto& a,const auto& b){return a.logical>b.logical;});out.complete=!out.limited&&!out.cancelled&&!out.skipped&&!out.mounts&&!out.overflow;return out;}
StorageScan scanAccountHome(const ScanLimits& limits,const std::atomic_bool& cancel){if(!getuid()||getuid()!=geteuid())return {};auto* pw=getpwuid(getuid());if(!pw||!pw->pw_dir||pw->pw_dir[0]!='/')return {};return scanStorage(pw->pw_dir,limits,cancel);}
std::string storageReport(const StorageScan& s){std::ostringstream o;o<<"Home storage scan: "<<(!s.available?"unavailable":s.complete?"complete within scope":"partial")<<"\n";if(!s.available)return o.str();o<<"Files "<<s.files<<", hardlink aliases "<<s.aliases<<", skipped "<<s.skipped<<", other mounts "<<s.mounts<<"\nLogical unique bytes: "<<s.logical<<"\nAllocated unique bytes: "<<s.allocated<<"\n";const char* names[]={"Cache","Review candidates","Archives","Media","Other"};for(unsigned i=0;i<5;++i)o<<names[i]<<": "<<s.categoryBytes[i]<<" logical bytes\n";o<<"Largest files (local relative paths; never automatically removed):\n";for(const auto& e:s.largest)o<<e.logical<<" B  "<<display(e.relative)<<'\n';return o.str();}
}
