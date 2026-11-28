// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/task_view.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
namespace syspulse::native {
namespace {
std::string lower(std::string_view s){std::string r(s);for(auto& c:r)if(c>='A'&&c<='Z')c=static_cast<char>(c+32);return r;}
}
TaskView buildTaskView(const Inspection& before,const Inspection& after,double seconds,std::uint64_t ticks,std::uint64_t page){TaskView out;out.partial=!after.processAvailable||after.processTruncated;if(after.processes.size()>4096||before.processes.size()>4096){out.partial=true;return out;}std::map<std::uint32_t,const Process*> old;std::set<std::uint32_t> ambiguous,seen;for(const auto& p:before.processes)if(!old.emplace(p.pid,&p).second)ambiguous.insert(p.pid);for(const auto& p:after.processes)if(!p.pid||!seen.insert(p.pid).second){out.partial=true;return out;}for(const auto& p:after.processes){TaskRow row{p,{},{}};if(page&&p.rssPages&&*p.rssPages<=UINT64_MAX/page)row.rssBytes=*p.rssPages*page;if(ticks&&seconds>0&&seconds<=30&&std::isfinite(seconds)&&!ambiguous.count(p.pid)){auto i=old.find(p.pid);if(i!=old.end()){auto rate=processTicksPerSecond(*i->second,p,seconds);if(rate){const double v=*rate*100/static_cast<double>(ticks);if(std::isfinite(v))row.cpuPercent=v;}}}out.rows.push_back(std::move(row));}return out;}
std::vector<TaskRow> filterTasks(const TaskView& view,std::string_view search,TaskSort sort){std::vector<TaskRow> out;if(search.size()>128)return out;const auto query=lower(search);for(const auto& row:view.rows)if(query.empty()||lower(row.process.name).find(query)!=std::string::npos||std::to_string(row.process.pid).find(query)!=std::string::npos)out.push_back(row);std::stable_sort(out.begin(),out.end(),[&](const auto& a,const auto& b){switch(sort){case TaskSort::Cpu:if(a.cpuPercent!=b.cpuPercent)return a.cpuPercent>b.cpuPercent;break;case TaskSort::Memory:if(a.rssBytes!=b.rssBytes)return a.rssBytes>b.rssBytes;break;case TaskSort::Name:{auto x=lower(a.process.name),y=lower(b.process.name);if(x!=y)return x<y;break;}case TaskSort::Pid:break;}return a.process.pid<b.process.pid;});return out;}
std::optional<Process> retainTaskSelection(const TaskView& view,const std::optional<Process>& selected){if(!selected||!selected->startTicks)return {};for(const auto& row:view.rows)if(row.process.pid==selected->pid&&row.process.startTicks==selected->startTicks)return row.process;return {};}
}
