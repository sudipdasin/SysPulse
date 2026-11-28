// SPDX-License-Identifier: GPL-3.0-or-later
// Fixed-vocabulary signature mining and outcome demotion ported from
// tools/syspulse_hybrid.py and tools/syspulse-diagnostics (GPL-3.0-or-later).
#include "syspulse/native/learning.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <set>
#include <sstream>
#include <locale>
#include <limits>
#include <utility>
#if defined(__linux__)
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/random.h>
#include <cerrno>
#endif
namespace syspulse::native {
namespace {
constexpr std::size_t retention=180, maxBytes=512*1024;
const std::set<std::string>& vocabulary(){static const auto words=[](){std::istringstream in("error errors failed failure critical warning panic kernel memory pressure swap oom kill killed process gpu hang reset thermal temperature throttling disk storage filesystem corrupt corruption readonly read only remount sector sectors buffer i o timeout timed out nvme ata ext4 ext3 ext2 btrfs xfs smart pending uncorrectable reallocated service unit start dependency authentication password invalid network dns dhcp link down connection unreachable dpkg apt package packages broken unmet dependencies segmentation fault segfault coredump crash watchdog lockup hardware machine check trace call permission denied allocation full exhausted no space left device block driver journal systemd mount input output");std::set<std::string> result;std::string w;while(in>>w)result.insert(w);return result;}();return words;}
int kind(unsigned char c){if((c>='A'&&c<='Z')||(c>='a'&&c<='z'))return 1;if(c>='0'&&c<='9')return 2;if(c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\v'||c=='\f')return 0;return 3;}
constexpr std::uint64_t maxGain=(1ULL<<53)-1;
bool validOutcome(const LearnedOutcome& o){return static_cast<unsigned>(o.context)<=3&&static_cast<unsigned>(o.action)<=2&&static_cast<unsigned>(o.result)<=2&&o.logicalBytes<=maxGain&&(o.result==LearningResult::Applied||!o.logicalBytes)&&(o.action!=LearningAction::OwnMemory||!o.logicalBytes)&&(bool(o.observedBefore)==bool(o.observedAfter));}
bool validAnalysis(const DiagnosticSummary& s){if(!s.available||!s.records||s.records>256||s.malformed>256||s.duplicates>256||s.oversized>256||s.recovered>s.records||s.patternOverflow>256||s.patterns.size()>6)return false;unsigned total=s.recovered;for(auto n:s.counts){if(n>256)return false;total+=n;}if(total>s.records)return false;std::set<std::pair<unsigned,std::string>> seen;std::array<unsigned,14> mined{};for(const auto& p:s.patterns){if(p.category>=14||!p.count||p.count>s.counts[p.category]||!validSignature(p.signature)||!seen.emplace(p.category,p.signature).second)return false;mined[p.category]+=p.count;if(mined[p.category]>s.counts[p.category])return false;}return true;}
std::string serialize(const LearningHistory& h){std::ostringstream out;out.imbue(std::locale::classic());out<<"SPLEARN1\n";for(const auto& a:h.analyses){out<<"A "<<a.records<<' '<<a.malformed<<' '<<a.duplicates<<' '<<a.recovered<<' '<<a.oversized<<' '<<unsigned(a.partial)<<' '<<a.patternOverflow;for(auto v:a.counts)out<<' '<<v;out<<' '<<a.patterns.size()<<'\n';for(const auto& p:a.patterns)out<<"P "<<p.category<<' '<<p.count<<' '<<p.signature<<'\n';}for(const auto& o:h.outcomes)out<<"O "<<unsigned(o.context)<<' '<<unsigned(o.action)<<' '<<unsigned(o.result)<<' '<<o.logicalBytes<<' '<<(o.observedBefore?std::to_string(*o.observedBefore):"-")<<' '<<(o.observedAfter?std::to_string(*o.observedAfter):"-")<<'\n';return out.str();}
std::optional<LearningHistory> deserialize(const std::string& data){if(data.size()>maxBytes||data.find('\0')!=data.npos||data.empty()||data.back()!='\n')return {};std::istringstream in(data);in.imbue(std::locale::classic());std::string line;if(!std::getline(in,line)||line!="SPLEARN1")return {};LearningHistory h;while(std::getline(in,line)){if(line.size()>1024)return {};std::istringstream row(line);row.imbue(std::locale::classic());char tag;std::string extra;if(!(row>>tag))return {};if(tag=='A'){DiagnosticSummary a;unsigned partial{},n{};if(h.analyses.size()>=retention||!(row>>a.records>>a.malformed>>a.duplicates>>a.recovered>>a.oversized>>partial>>a.patternOverflow)||partial>1)return {};a.partial=partial!=0;a.available=true;for(auto& v:a.counts)if(!(row>>v))return {};if(!(row>>n)||n>6||(row>>extra))return {};for(unsigned i=0;i<n;++i){if(!std::getline(in,line)||line.size()>160)return {};std::istringstream pr(line);DiagnosticPattern p;if(!(pr>>tag>>p.category>>p.count)||tag!='P'||pr.get()!=' ')return {};std::getline(pr,p.signature);a.patterns.push_back(std::move(p));}if(!validAnalysis(a))return {};h.analyses.push_back(std::move(a));}else if(tag=='O'){unsigned c{},a{},r{};std::uint64_t bytes{};std::string before,after;if(h.outcomes.size()>=retention||!(row>>c>>a>>r>>bytes>>before>>after)||(row>>extra))return {};LearnedOutcome o{static_cast<LearningContext>(c),static_cast<LearningAction>(a),static_cast<LearningResult>(r),bytes};const auto parse=[](const std::string& t)->std::optional<std::uint64_t>{std::uint64_t n{};auto r=std::from_chars(t.data(),t.data()+t.size(),n);if(r.ec!=std::errc{}||r.ptr!=t.data()+t.size())return {};return n;};if(before!="-"||after!="-"){o.observedBefore=parse(before);o.observedAfter=parse(after);if(!o.observedBefore||!o.observedAfter)return {};}if(!validOutcome(o))return {};h.outcomes.push_back(o);}else return {};}
// Require canonical encoding, rejecting signs, extra spaces, and alternate representations.
if(serialize(h)!=data)return {};
return h;}
#if defined(__linux__)
struct Fd{int n=-1;explicit Fd(int v=-1):n(v){}~Fd(){if(n>=0)close(n);}Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;};
bool secureFile(int fd){struct stat st{};return fd>=0&&!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_uid==getuid()&&st.st_nlink==1&&!(st.st_mode&0077)&&st.st_size>=0&&static_cast<std::uint64_t>(st.st_size)<=maxBytes;}
int historyDir(){if(!getuid()||getuid()!=geteuid()||getgid()!=getegid())return -1;const auto* pw=getpwuid(getuid());if(!pw||!pw->pw_dir||pw->pw_dir[0]!='/')return -1;std::string home(pw->pw_dir);Fd dir(open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));if(dir.n<0)return -1;std::size_t pos=1;for(unsigned depth=0;pos<home.size();++depth){if(depth>=128)return -1;auto end=home.find('/',pos);auto part=home.substr(pos,end==home.npos?home.npos:end-pos);if(part.empty()||part=="."||part=="..")return -1;int next=openat(dir.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(next<0)return -1;close(dir.n);dir.n=next;if(end==home.npos)break;pos=end+1;}struct stat st{};if(fstat(dir.n,&st)||st.st_uid!=getuid()||(st.st_mode&0022))return -1;if(mkdirat(dir.n,".syspulse-native-history",0700)&&errno!=EEXIST)return -1;Fd folder(openat(dir.n,".syspulse-native-history",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));if(folder.n<0||fstat(folder.n,&st)||st.st_uid!=getuid()||(st.st_mode&0077))return -1;return std::exchange(folder.n,-1);}
std::optional<LearningHistory> load(int dir){Fd f(openat(dir,"history-v1",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));if(f.n<0)return errno==ENOENT?std::optional<LearningHistory>(LearningHistory{}):std::nullopt;if(!secureFile(f.n))return {};std::string data;std::array<char,4096> b{};for(;;){auto n=read(f.n,b.data(),b.size());if(n<0&&errno==EINTR)continue;if(n<0)return {};if(!n)break;if(static_cast<std::size_t>(n)>maxBytes-data.size())return {};data.append(b.data(),static_cast<std::size_t>(n));}return deserialize(data);}
bool save(int dir,const LearningHistory& h){const auto data=serialize(h);if(data.size()>maxBytes)return false;unsigned char bytes[12]{};if(getrandom(bytes,sizeof bytes,0)!=static_cast<ssize_t>(sizeof bytes))return false;std::string tmp=".tmp-";const char* hex="0123456789abcdef";for(auto b:bytes){tmp+=hex[b>>4];tmp+=hex[b&15];}Fd f(openat(dir,tmp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));if(f.n<0)return false;struct Cleanup{int dir;const std::string& name;~Cleanup(){unlinkat(dir,name.c_str(),0);}} cleanup{dir,tmp};std::size_t offset=0;while(offset<data.size()){auto n=write(f.n,data.data()+offset,data.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0)return false;offset+=static_cast<std::size_t>(n);}if(fsync(f.n)||renameat(dir,tmp.c_str(),dir,"history-v1"))return false;return fsync(dir)==0;}
// Nonblocking advisory lock: no UI stall; old file is preserved on refusal.
template<class Update> bool edit(Update fn){Fd dir(historyDir());if(dir.n<0)return false;Fd lock(openat(dir.n,"lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600));if(!secureFile(lock.n)||flock(lock.n,LOCK_EX|LOCK_NB))return false;auto h=load(dir.n);if(!h)return false;fn(*h);return save(dir.n,*h);}
#endif
}
std::string privateSignature(std::string_view msg){msg=msg.substr(0,4096);std::vector<std::string> words;for(unsigned tokens=0;!msg.empty()&&tokens<96&&words.size()<12;){const auto k=kind(static_cast<unsigned char>(msg.front()));std::size_t n=1;while(n<msg.size()&&kind(static_cast<unsigned char>(msg[n]))==k)++n;auto token=msg.substr(0,n);msg.remove_prefix(n);if(!k)continue;++tokens;std::string w="*";if(k==1){std::string lower(token);for(auto& c:lower)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');if(vocabulary().count(lower))w=std::move(lower);}if(w=="*"&&!words.empty()&&words.back()=="*")continue;words.push_back(std::move(w));}std::string result;bool known=false;for(const auto& w:words){known|=w!="*";if(!result.empty())result+=' ';result+=w;}if(!known)return "unclassified error";while(result.size()>120){const auto p=result.rfind(' ');if(p==result.npos)return "unclassified error";result.resize(p);}return result;}
bool validSignature(std::string_view s){if(s=="unclassified error")return true;if(s.empty()||s.size()>120||s.front()==' '||s.back()==' ')return false;bool known=false,lastMask=false;unsigned n=0;while(!s.empty()){auto p=s.find(' ');auto token=s.substr(0,p);if(++n>12||token.empty())return false;const bool mask=token=="*";if(mask&&lastMask)return false;if(!mask&&!vocabulary().count(std::string(token)))return false;known|=!mask;lastMask=mask;s=p==s.npos?std::string_view{}:s.substr(p+1);}return known;}
bool ActionHistory::demoted()const noexcept{return attempts>=3&&(failures+noEffect)*2>=attempts;}
ActionHistory actionHistory(const LearningHistory& h,LearningContext c,LearningAction a)noexcept{ActionHistory r;for(const auto& o:h.outcomes)if(o.context==c&&o.action==a&&validOutcome(o)){++r.attempts;switch(o.result){case LearningResult::Applied:++r.successes;break;case LearningResult::Failure:++r.failures;break;case LearningResult::NoEffect:++r.noEffect;break;}}return r;}
std::string learningReport(const LearningHistory& h){std::ostringstream out;out<<"Local learning history (no raw logs, no commands)\n"<<h.analyses.size()<<" analysis samples; "<<h.outcomes.size()<<" action receipts.\n";const auto from=h.analyses.size()>10?h.analyses.size()-10:0;std::array<unsigned,14> recurrence{};std::map<std::pair<unsigned,std::string>,unsigned> patterns;for(std::size_t i=from;i<h.analyses.size();++i){const auto& a=h.analyses[i];for(unsigned c=0;c<14;++c)if(a.counts[c])++recurrence[c];for(const auto& p:a.patterns)if(p.category<14&&validSignature(p.signature))++patterns[{p.category,p.signature}];}out<<"Recent window: "<<h.analyses.size()-from<<" samples (may overlap).\n";for(unsigned c=0;c<14;++c)if(recurrence[c])out<<diagnosticCategories()[c].name<<": "<<recurrence[c]<<" samples\n";std::vector<std::pair<std::pair<unsigned,std::string>,unsigned>> sorted(patterns.begin(),patterns.end());std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.second!=b.second?a.second>b.second:a.first<b.first;});for(std::size_t i=0;i<std::min<std::size_t>(6,sorted.size());++i)out<<sorted[i].first.second<<": "<<sorted[i].second<<" samples\n";const char* contexts[]={"normal","memory","disk","unknown"};const char* actions[]={"thumbnails","failed markers","own memory"};for(unsigned c=0;c<4;++c)for(unsigned a=0;a<3;++a){auto r=actionHistory(h,static_cast<LearningContext>(c),static_cast<LearningAction>(a));if(r.attempts)out<<contexts[c]<<" / "<<actions[a]<<": "<<r.successes<<" successful, "<<r.failures<<" failed, "<<r.noEffect<<" no effect"<<(r.demoted()?"; manual review preferred":"")<<"\n";}if(!h.outcomes.empty()){const auto& last=h.outcomes.back();if(last.observedBefore&&last.observedAfter)out<<"Last observation: "<<*last.observedBefore<<" -> "<<*last.observedAfter<<(last.action==LearningAction::OwnMemory?" KiB own RSS":" bytes cache-volume free")<<".\n";}out<<"Repeated samples are not unique incidents. Action results do not prove speed gain.\n";return out.str();}
std::optional<LearningHistory> readLearningHistory(){
#if defined(__linux__)
Fd dir(historyDir());if(dir.n>=0)return load(dir.n);
#endif
return {};}
bool rememberAnalysis(const DiagnosticSummary& a){if(!validAnalysis(a))return false;
#if defined(__linux__)
return edit([&](auto& h){h.analyses.push_back(a);if(h.analyses.size()>retention)h.analyses.erase(h.analyses.begin());});
#else
return false;
#endif
}
bool rememberOutcome(const LearnedOutcome& o){if(!validOutcome(o))return false;
#if defined(__linux__)
return edit([&](auto& h){h.outcomes.push_back(o);if(h.outcomes.size()>retention)h.outcomes.erase(h.outcomes.begin());});
#else
return false;
#endif
}
}
