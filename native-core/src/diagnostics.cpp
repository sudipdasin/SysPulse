// SPDX-License-Identifier: GPL-3.0-or-later
// Port of SysPulse tools/syspulse-diagnostics PATTERNS and syspulse_recovery.py.
#include "syspulse/native/diagnostics.hpp"
#include <regex>
#include <sstream>
#include <set>
#include <memory>
#include <chrono>
#include <json-c/json.h>
namespace syspulse::native {
namespace {
using Rx=std::regex;
const std::array<Rx,14>& patterns(){static const std::array<Rx,14> p{{
 Rx(R"(\b(?:i/o error|buffer i/o|blk_update_request|ata[0-9].*failed command|nvme.*(?:timeout|reset|abort)|medium error|uncorrectable error)\b)",Rx::icase),
 Rx(R"((?:ext[234]-fs error|btrfs.*(?:error|corrupt)|xfs.*(?:corrupt|metadata error)|filesystem.*(?:corrupt|read-only)|remounting filesystem read-only))",Rx::icase),
 Rx(R"((?:smart.*(?:fail|critical|bad)|pending sectors|reallocated sector.*(?:fail|critical)|critical warning.*(?:nvme|disk)))",Rx::icase),
 Rx(R"((?:out of memory|oom-kill|oom_reaper|killed process.*total-vm|systemd-oomd.*killed))",Rx::icase),
 Rx(R"((?:memory pressure|swap.*(?:exhausted|full)|allocation failure))",Rx::icase),
 Rx(R"((?:gpu hang|gpu reset|amdgpu.*(?:fault|timeout|error)|i915.*(?:hang|error)|nvrm: xid|drm.*(?:error|failed)))",Rx::icase),
 Rx(R"((?:critical temperature|thermal throttling|temperature above threshold|overheat))",Rx::icase),
 Rx(R"((?:kernel panic|kernel bug|general protection fault|soft lockup|hard lockup|watchdog.*lockup|call trace:|machine check|hardware error))",Rx::icase),
 Rx(R"((?:failed to start|failed with result|main process exited|dependency failed|start request repeated too quickly|unit .* failed))",Rx::icase),
 Rx(R"((?:dpkg: error|unmet dependencies|sub-process.*returned an error|failed to fetch|package.*(?:broken|failed)|error processing package))",Rx::icase),
 Rx(R"((?:network.*(?:unreachable|failed|down)|dns.*(?:failure|timeout)|dhcp.*(?:fail|timeout)|connection timed out|link is down))",Rx::icase),
 Rx(R"((?:authentication failure|failed password|invalid user|pam_.*(?:failure|failed)))",Rx::icase),
 Rx(R"((?:segfault|segmentation fault|core dumped|coredump.*(?:crash|dumped)|fatal exception|aborted.*core))",Rx::icase),
 Rx(R"((?:\berror\b|\bfailed\b|\bfailure\b|\bcritical\b|\bpanic\b))",Rx::icase)
 }};return p;}
constexpr std::array<unsigned,14> order{{0,1,2,3,4,5,6,7,9,11,10,12,8,13}};
const std::array<Rx,8>& recoveries(){static const std::array<Rx,8> r{{
 Rx(R"(SMART\s+health\s+passed[;,. ]+\s*0\s+pending\s+sectors[.]?)",Rx::icase),
 Rx(R"(SMART\s*:\s*0\s+pending\s+sectors\s*,\s*0\s+offline\s+uncorrectable\s+sectors[.]?)",Rx::icase),
 Rx(R"(No\s+errors?\s+detected\s+in\s+(?:the\s+)?(?:memory|RAM)\s+test[.]?)",Rx::icase),
 Rx(R"(Memory\s+pressure\s+(?:has\s+)?returned\s+to\s+normal[.]?)",Rx::icase),
 Rx(R"(Previous\s+error\s+resolved[;,. ]+\s*service\s+started\s+successfully[.]?)",Rx::icase),
 Rx(R"(Previous\s+package\s+error\s+(?:was\s+)?resolved\s+during\s+recovery[.]?)",Rx::icase),
 Rx(R"(Connection\s+timed\s+out\s+earlier[;,. ]+\s*link\s+restored[.]?)",Rx::icase),
 Rx(R"(GPU\s+reset\s+completed\s+successfully(?:[;,. ]+\s*display\s+restored)?[.]?)",Rx::icase)
 }};return r;}
std::optional<std::string_view> stringField(json_object* object,const char* key){json_object* v=nullptr;if(!json_object_object_get_ex(object,key,&v)||!v||json_object_get_type(v)!=json_type_string)return {};return std::string_view(json_object_get_string(v),static_cast<std::size_t>(json_object_get_string_len(v)));}
std::optional<int> priority(json_object* object){json_object* v=nullptr;if(!json_object_object_get_ex(object,"PRIORITY",&v))return 7;if(!v)return {};int n=-1;if(json_object_get_type(v)==json_type_int){auto i=json_object_get_int64(v);if(i>=0&&i<=7)n=static_cast<int>(i);}else if(auto s=stringField(object,"PRIORITY")){if(s->size()==1&&s->front()>='0'&&s->front()<='7')n=s->front()-'0';}return n>=0?std::optional<int>(n):std::nullopt;}
std::string_view trim(std::string_view s){auto b=s.find_first_not_of(" \t\r\n");if(b==s.npos)return {};return s.substr(b,s.find_last_not_of(" \t\r\n")-b+1);}
}
const std::array<DiagnosticCategory,14>& diagnosticCategories(){static const std::array<DiagnosticCategory,14> c{{
 {"storage_io","Back up important files; inspect device health and I/O errors.",true},
 {"filesystem","Back up and review offline filesystem consistency; do not repair mounted volumes.",true},
 {"disk_health","Review SMART evidence and backup; do not assume health from one log line.",true},
 {"oom","Save work and inspect high-memory processes and swap.",true},
 {"memory_pressure","Review sustained pressure and swap before tuning.",false},
 {"gpu","Inspect graphics driver and kernel evidence.",false},
 {"thermal","Reduce load and inspect cooling and sensors.",true},
 {"kernel_fault","Save work; review kernel and hardware evidence before changes.",true},
 {"service_failure","Inspect the selected service and dependencies before restart.",false},
 {"package_failure","Inspect package health and review the no-removal repair simulation.",false},
 {"network","Review link, routes and DNS before changing network state.",false},
 {"authentication","Review authentication records locally; no automatic security changes.",false},
 {"application_crash","Review application and crash evidence; preserve user data.",false},
 {"other_error","Inspect the local evidence; no automatic repair inferred.",false}
 }};return c;}
DiagnosticSummary diagnoseJournal(std::string_view input,bool commandSucceeded,bool outputCapped){DiagnosticSummary out;out.partial=outputCapped;if(!commandSucceeded)return out;if(input.size()>65536){out.partial=true;return out;}std::set<std::string> cursors;unsigned lines=0;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);while(!input.empty()){
    if(std::chrono::steady_clock::now()>=deadline){out.partial=true;break;}
    auto end=input.find('\n');auto line=trim(input.substr(0,end));input=end==input.npos?std::string_view{}:input.substr(end+1);if(line.empty())continue;if(++lines>256){out.partial=true;break;}if(line.size()>16384||line.find('\0')!=line.npos){++out.oversized;out.partial=true;continue;}
    std::unique_ptr<json_tokener,decltype(&json_tokener_free)> tok(json_tokener_new_ex(16),json_tokener_free);if(!tok){out.partial=true;break;}json_tokener_set_flags(tok.get(),JSON_TOKENER_STRICT|JSON_TOKENER_VALIDATE_UTF8);
    std::unique_ptr<json_object,decltype(&json_object_put)> obj(json_tokener_parse_ex(tok.get(),line.data(),static_cast<int>(line.size())),json_object_put);
    if(!obj||json_tokener_get_error(tok.get())!=json_tokener_success||json_tokener_get_parse_end(tok.get())!=line.size()||json_object_get_type(obj.get())!=json_type_object){++out.malformed;out.partial=true;continue;}
    auto msg=stringField(obj.get(),"MESSAGE");auto pr=priority(obj.get());if(!msg||!pr||msg->find('\0')!=msg->npos){++out.malformed;out.partial=true;continue;}
    if(auto cursor=stringField(obj.get(),"__CURSOR")){if(cursor->size()>1024||cursor->find('\0')!=cursor->npos){++out.malformed;out.partial=true;continue;}if(!cursors.emplace(*cursor).second){++out.duplicates;continue;}}
    ++out.records;out.available=true;if(msg->size()>2048){++out.oversized;out.partial=true;continue;}auto text=trim(*msg);bool recovered=false;
    if(*pr>3)for(const auto& rx:recoveries())if(std::regex_match(text.begin(),text.end(),rx)){++out.recovered;recovered=true;break;}
    if(recovered)continue;
    bool found=false;for(auto i:order)if(std::regex_search(text.begin(),text.end(),patterns()[i])){++out.counts[i];found=true;break;}if(!found&&*pr<=3)++out.counts[13];
 }if(!out.available)out.partial=true;return out;}
std::string diagnosticReport(const DiagnosticSummary& s){std::ostringstream o;o<<"Journal classification: "<<(!s.available?"unavailable":s.partial?"partial":"bounded sample")<<". Not a diagnosis or repair guarantee.\n";o<<"Records "<<s.records<<", invalid "<<s.malformed<<", duplicates "<<s.duplicates<<", oversized "<<s.oversized<<", recovery observations "<<s.recovered<<".\n";for(std::size_t i=0;i<s.counts.size();++i)if(s.counts[i]){const auto& c=diagnosticCategories()[i];o<<(c.critical?"PRIORITY REVIEW ":"REVIEW ")<<c.name<<": "<<s.counts[i]<<"\n"<<c.advice<<"\n";}o<<"Counts may overlap across samples. Recovery observations do not prove repair.\n";return o.str();}
}
