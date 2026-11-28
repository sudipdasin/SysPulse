// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/health.hpp"
#include "syspulse/native/diagnostics.hpp"
#include "syspulse/native/learning.hpp"
#include "syspulse/native/storage_scan.hpp"
#include "syspulse/native/task_view.hpp"
#include "syspulse/native/desktop.hpp"
#include "syspulse/native/maintenance.hpp"
#include "syspulse/native/system_tools.hpp"
#include "syspulse/native/apt_plan.hpp"
#include "syspulse/native/offline_storage.hpp"
#include <unistd.h>
#include "syspulse/native/backup.hpp"
#include <future>
#include <cmath>
#include <sstream>
#include "syspulse/native/startup.hpp"
#include "syspulse/native/actions.hpp"
#include "syspulse/native/device_rates.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <poll.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
using namespace syspulse::native;
namespace triage=syspulse::triage;
namespace {
struct Canvas {
    Display* d; Window w; GC gc; int screen; XFontStruct* normal; XFontStruct* heading;
    unsigned long background,card,ink,muted,accent,line,green;
    unsigned long color(const char* hex){XColor actual{},exact{};return XAllocNamedColor(d,DefaultColormap(d,screen),hex,&actual,&exact)?actual.pixel:BlackPixel(d,screen);}
    int textWidth(const std::string& value)const{return normal?XTextWidth(normal,value.c_str(),static_cast<int>(value.size())):static_cast<int>(value.size())*6;}
    void rect(int x,int y,int width,int height,unsigned long pixel){if(width<=0||height<=0)return;XSetForeground(d,gc,pixel);XFillRectangle(d,w,gc,x,y,width,height);}
    void text(int x,int y,const std::string& value,unsigned long pixel,bool large=false){auto* font=large&&heading?heading:normal;if(font)XSetFont(d,gc,font->fid);XSetForeground(d,gc,pixel);XDrawString(d,w,gc,x,y,value.c_str(),static_cast<int>(value.size()));}
};
std::string percent(const std::optional<double>& v){if(!v)return "Unavailable";char b[32];std::snprintf(b,sizeof b,"%.1f%%",*v);return b;}
std::string capacity(const std::optional<triage::Counters>& v){if(!v||!triage::valid(v))return "Unavailable";char b[64];std::snprintf(b,sizeof b,"%.1f GiB free",static_cast<double>(v->available)/1073741824.0);return b;}
const char* severity(triage::Severity s){switch(s){case triage::Severity::Unknown:return "UNKNOWN";case triage::Severity::Ok:return "OK";case triage::Severity::Info:return "INFO";case triage::Severity::Warning:return "REVIEW";case triage::Severity::Critical:return "CRITICAL";}return "UNKNOWN";}
struct UI {
    LearningAction cacheAction=LearningAction::Thumbnails; LearningContext cacheContext=LearningContext::Unknown;
    bool hubPage=false; DesktopLauncher desktop; std::string report; std::array<bool,12> available{};
    bool backupPage=false; int backupField=0; std::string backupSource,backupDestination,backupDigest; BackupReview backup; std::optional<CopyPlan> backupPlan; std::future<CopyOutcome> copying; History history{120};
    TaskView taskModel; std::vector<TaskRow> taskRows; TaskSort taskSort=TaskSort::Pid; std::string taskQuery; bool taskSearch=false;
    std::future<StorageScan> scanning; bool scanPage=false; std::vector<std::string> scanRows;
    bool busy()const{return job.active()||copying.valid()||scanning.valid();}
    CacheReview cache; ProcessReview process; CleanSummary summary; std::string message="Choose an action. Nothing runs automatically.";
    StartupReview startup; StartupInventory startupList; PackageAudit packages; std::optional<StartupEntry> startupSelected;
    bool tasks=false, startupPage=false, maintenancePage=false, toolsPage=false, advancedPage=false, systemServices=false, storagePage=false; std::vector<OfflineDevice> offlineList; std::optional<OfflineDevice> offlineSelected,offlineChecked,offlineTarget; bool offlineRevalidate=false, offlineFix=false; std::chrono::steady_clock::time_point offlineCheckedAt,serviceExpires; CompressionEvidence compression; bool trimPreview=false; int packagePreview=0; std::string packageToken; std::chrono::steady_clock::time_point packageExpires; std::chrono::steady_clock::time_point trimExpires; Job job; Operation operation=Operation::Audit; std::optional<Command> planned; std::chrono::steady_clock::time_point expires; std::vector<Service> services; std::optional<Service> selectedService; std::vector<std::string> output; bool serviceListFresh=false, cleanPreview=false; std::chrono::steady_clock::time_point previewExpires; std::size_t outputPage=0; int pending=0; std::size_t page=0; std::optional<Process> selected;
    void cancel(){backup.cancel();backupPlan.reset();pending=0;planned.reset();cache.cancel();process.cancel();startup.cancel();}
};
LearningContext contextFor(const HealthEvidence& h){if(h.rootBytes&&h.rootBytes->total&&h.rootBytes->available<=h.rootBytes->total&&static_cast<double>(h.rootBytes->available)/h.rootBytes->total<0.15)return LearningContext::Disk;if(h.memory&&h.memory->totalKiB&&h.memory->availableKiB<=h.memory->totalKiB&&static_cast<double>(h.memory->availableKiB)/h.memory->totalKiB<0.15)return LearningContext::Memory;if(!h.memory||!h.rootBytes||!h.memory->totalKiB||h.memory->availableKiB>h.memory->totalKiB||!h.rootBytes->total||h.rootBytes->available>h.rootBytes->total)return LearningContext::Unknown;return LearningContext::Normal;}
std::string learnedAdvice(LearningContext context,LearningAction action){const auto h=readLearningHistory();return h&&actionHistory(*h,context,action).demoted()?" History: repeated poor outcomes; manual review preferred.":"";}
std::string fitText(const Canvas& c,std::string text,int pixels){if(c.textWidth(text)<=pixels)return text;while(!text.empty()&&c.textWidth(text+"...")>pixels)text.pop_back();return text+"...";}
std::vector<std::string> reviewLines(const Canvas& c,const std::string& message,int width){std::vector<std::string> result;std::string line;for(char ch:message){if(c.textWidth(line+ch)>width){if(line.empty())return {};result.push_back(line);line.clear();}line+=ch;}if(!line.empty())result.push_back(line);return result;}
void button(Canvas& c,int x,int y,int w,const std::string& label,bool primary=false){c.rect(x,y,w,30,primary?c.accent:c.line);c.text(x+8,y+19,fitText(c,label,w-16),primary?c.card:c.ink);}
std::string safeName(const std::string& s){std::string out;for(unsigned char ch:s){if(out.size()>=32)break;out+=ch>=32&&ch<127?static_cast<char>(ch):'?';}return out;}
void render(Canvas& c,int width,int height,const std::optional<Rates>& rates,const HealthEvidence& health,const Inspection& inventory,const UI& ui){
    c.rect(0,0,width,height,c.background);c.rect(0,0,width,6,c.accent);
    c.text(24,36,"SysPulse Native",c.ink,true);if(!ui.scanPage)button(c,width-220,18,196,"Desktop / reports",ui.hubPage);c.text(24,58,ui.scanPage?"STORAGE / HOME USAGE ANALYSIS":ui.hubPage?"DESKTOP / DIAGNOSTICS / REPORTS":ui.backupPage?"RECOVERY / VERIFIED FILE COPY":ui.storagePage?"STORAGE / OFFLINE FILESYSTEM RECOVERY":ui.advancedPage?"NETWORK / POWER / DIAGNOSTICS":ui.toolsPage?"SYSTEM TOOLS / MEMORY / STORAGE":ui.maintenancePage?"PACKAGE MAINTENANCE / USER SERVICES":ui.startupPage?"STARTUP / PACKAGE HEALTH":ui.tasks?"TASK MANAGER / REVIEWED PROCESS ACTIONS":"SYSTEM OVERVIEW / MAINTENANCE",c.muted);
    c.rect(24,76,width-48,63,c.card);c.rect(24,76,4,63,c.accent);
    c.text(40,100,ui.scanPage?"Scoped storage evidence":"System care and live evidence",c.ink,true);const int bw=(width-80-30)/4;
    if(ui.scanPage)c.text(40,128,"Read-only home inventory. Use Back to storage to return.",c.muted);else {
    button(c,40,108,bw,(ui.backupPage||ui.tasks||ui.startupPage||ui.maintenancePage)?"Overview":"Analyze cache",!ui.backupPage&&!ui.tasks&&!ui.startupPage&&!ui.maintenancePage);
    button(c,40+bw+10,108,bw,ui.backupPage?"Task manager":ui.storagePage?"Package / services":ui.advancedPage?"Storage / recovery":ui.toolsPage?"Network / power":ui.maintenancePage?"System tools":ui.startupPage?"Review toggle":ui.tasks?"Review end task":"Task manager");
    button(c,40+2*(bw+10),108,bw,"Startup / APT");
    button(c,40+3*(bw+10),108,bw,ui.maintenancePage&&!ui.toolsPage&&!ui.advancedPage&&!ui.storagePage?"Refresh services":"Maintenance");}
    if(ui.hubPage){
        c.rect(24,153,width-48,390,c.card);c.text(38,176,"Desktop tools and local reports",c.ink,true);
        const int mw=(width-100)/3;
        for(std::size_t i=0;i<desktopTools().size();++i){const auto label=std::string(desktopTools()[i].label)+(ui.available[i]?"":" (missing)");button(c,38+static_cast<int>(i%3)*(mw+8),188+static_cast<int>(i/3)*37,mw,label);}
        button(c,38,344,mw,"Refresh health report");button(c,46+mw,344,mw,"Save report to home",true);button(c,54+2*mw,344,mw,"Overview");
        button(c,38,380,190,"Local learning history");c.text(240,399,fitText(c,"External tools manage their own actions.",width-270),c.muted);
        c.text(38,416,"Reports exclude hostnames, process names, addresses and raw logs.",c.muted);
        std::istringstream report(ui.report);std::string row;unsigned index=0;
        while(std::getline(report,row)){if(index>=ui.outputPage*6&&index<ui.outputPage*6+6)c.text(38,438+static_cast<int>(index-ui.outputPage*6)*16,fitText(c,row,width-76),c.ink);++index;}
        c.text(width-200,535,"PgUp / PgDn: report",c.muted);
    }else if(ui.backupPage){
        c.rect(24,153,width-48,390,c.card);c.text(38,178,"Verified file backup and recovery",c.ink,true);
        c.text(38,202,"Copy a user-owned file to a NEW path. Restore by copying the backup to a new name.",c.muted);
        c.text(38,224,"Up to 64 MiB; no overwrite, no links, no root; existing owned destination directory.",c.muted);
        auto field=[&](int y,const char* title,const std::string& value,int i){c.text(38,y,title,c.ink);c.rect(38,y+10,width-76,31,ui.backupField==i?c.line:c.background);c.text(46,y+30,fitText(c,value,width-92),c.ink);};
        field(252,"Source absolute path",ui.backupSource,0);field(322,"New destination absolute path",ui.backupDestination,1);
        button(c,38,380,180,"Review copy",true);button(c,230,380,150,"Clear paths");
        c.text(38,438,"Type paths (Tab selects a field). Full reviewed paths must fit the view.",c.muted);
        c.text(38,459,"Destination is private (0600); metadata, directory trees and snapshots are not copied.",c.muted);
        if(!ui.backupDigest.empty())c.text(38,490,"SHA-256: "+ui.backupDigest,c.green);
    }else if(ui.scanPage){
        c.rect(24,153,width-48,390,c.card);c.text(38,176,"Home storage analysis",c.ink,true);
        button(c,38,188,160,"Refresh scan",true);button(c,208,188,160,"Back to storage");
        c.text(38,240,"Local report; hardlinks counted once, other mounts excluded.",c.muted);
        for(std::size_t i=0;i<15&&ui.outputPage*15+i<ui.scanRows.size();++i)c.text(38,260+static_cast<int>(i)*16,fitText(c,ui.scanRows[ui.outputPage*15+i],width-76),c.ink);
        button(c,38,508,100,"Previous");button(c,150,508,100,"Next");
    }else if(ui.storagePage){
        c.rect(24,153,width-48,390,c.card);c.text(38,176,"Offline ext filesystem recovery",c.ink,true);
        const int mw=(width-100)/3;
        button(c,38,188,mw,"Refresh devices");button(c,46+mw,188,mw,"Check selected");button(c,54+2*mw,188,mw,"Repair selected");
        c.text(38,239,"Only unmounted ext2/3/4 with unique UUID and device identity.",c.muted);
        for(std::size_t i=0;i<5&&ui.page*5+i<ui.offlineList.size();++i){const auto& v=ui.offlineList[ui.page*5+i];int y=250+static_cast<int>(i)*25;if(ui.offlineSelected&&*ui.offlineSelected==v)c.rect(32,y,width-64,25,c.line);c.text(38,y+16,v.path+" | "+v.type+" | "+v.size+" | "+v.majorMinor,c.ink);}
        button(c,38,385,100,"Previous");button(c,150,385,100,"Next");c.text(262,404,"Page "+std::to_string(ui.page+1),c.muted);
        if(ui.offlineSelected)c.text(38,433,"UUID: "+ui.offlineSelected->uuid,c.ink);
        c.text(38,454,"Repair may change or discard damaged data; back up before repair.",c.accent);
        c.text(38,473,"UDisks performs authorization and rejects mounted filesystems.",c.muted);
        button(c,width-370,508,155,"Scan home storage");button(c,width-205,508,161,"Review failed cache");
        c.text(38,493,ui.offlineChecked?"Inconsistency reported. Review repair only after backup.":"A current failed consistency check is required before repair.",c.muted);
    }else if(ui.advancedPage){
        c.rect(24,153,width-48,390,c.card);c.text(38,176,"Network, power and diagnostic evidence",c.ink,true);
        const int mw=(width-100)/3;
        const char* labels[]={"Network addresses","Routing table","Listening sockets","Wi-Fi status","Enable Wi-Fi","Disable Wi-Fi","Power profile","Balanced profile","Power saver","Performance profile","System errors","Analyze journal","Firewall status","Scheduled timers","Storage / recovery"};
        for(int i=0;i<15;++i)button(c,38+(i%3)*(mw+8),188+(i/3)*37,mw,labels[i]);
        c.text(38,385,"Wi-Fi and power changes require confirmation; system policy applies.",c.muted);
        c.text(38,402,"Logs and addresses remain local. Missing utilities are reported.",c.muted);
        c.text(38,419,"Firewall inspection only; no automatic security policy changes.",c.muted);
        c.text(38,442,"Last command output (bounded; PgUp/PgDn): "+std::to_string(ui.outputPage+1),c.ink,true);
        for(std::size_t i=0;i<5&&ui.outputPage*5+i<ui.output.size();++i)c.text(38,460+static_cast<int>(i)*14,fitText(c,ui.output[ui.outputPage*5+i],width-76),c.muted);
    }else if(ui.toolsPage){
        c.rect(24,153,width-48,390,c.card);c.text(38,176,"System repair, storage and memory",c.ink,true);
        const int mw=(width-100)/3;
        const char* labels[]={"Check dependencies","Configure pending","Preview upgrades","Disk inventory","Preview root TRIM","TRIM root filesystem","Failed system services","Enable zswap","Disable zswap","Reclaim own RAM","Refresh evidence","Apply upgrades","Preview repair","Apply repair","Network / power"};
        for(int i=0;i<15;++i)button(c,38+(i%3)*(mw+8),188+(i/3)*37,mw,labels[i]);
        c.text(38,385,std::string("Runtime zswap: ")+(ui.compression.state==ZswapState::Enabled?"enabled":ui.compression.state==ZswapState::Disabled?"disabled":"unknown"),c.ink);
        c.text(38,402,"Changes require review. Root TRIM only; no filesystem repair.",c.muted);
        c.text(38,419,"APT rechecks the reviewed plan; no removals. Transaction timing can still change.",c.muted);
        c.text(38,442,"Last command output (bounded; PgUp/PgDn): "+std::to_string(ui.outputPage+1),c.ink,true);
        for(std::size_t i=0;i<5&&ui.outputPage*5+i<ui.output.size();++i)c.text(38,460+static_cast<int>(i)*14,fitText(c,ui.output[ui.outputPage*5+i],width-76),c.muted);
    }else if(ui.maintenancePage){
        c.rect(24,153,width-48,390,c.card);c.text(38,176,ui.systemServices?"Package maintenance / system services":"Package maintenance / user services",c.ink,true);
        const int mw=(width-100)/3;
        button(c,38,188,mw,"Package audit");button(c,46+mw,188,mw,"Preview autoclean");button(c,54+2*mw,188,mw,"Refresh APT lists");
        button(c,38,225,mw,"Clean obsolete cache");button(c,46+mw,225,mw,ui.systemServices?"Start system service":"Start user service");button(c,54+2*mw,225,mw,ui.systemServices?"Stop system service":"Stop user service");
        c.text(38,274,ui.systemServices?"System services: select a row; admin authorization required":"User services: select a row (refresh to reload)",c.muted);
        for(std::size_t i=0;i<5&&ui.page*5+i<ui.services.size();++i){const auto& v=ui.services[ui.page*5+i];int y=282+static_cast<int>(i)*22;if(ui.selectedService&&ui.selectedService->name==v.name)c.rect(32,y,width-64,22,c.line);c.text(38,y+15,v.name.substr(0,58),c.ink);if(width>=760)c.text(width-150,y+15,v.state,c.muted);}
        button(c,38,395,100,"Previous");button(c,150,395,100,"Next");button(c,262,395,128,ui.systemServices?"User services":"System services");button(c,402,395,112,"Restart service");c.text(524,414,"Page "+std::to_string(ui.page+1),c.muted);
        c.text(38,442,"Last command output (bounded; PgUp/PgDn): "+std::to_string(ui.outputPage+1),c.ink,true);
        for(std::size_t i=0;i<5&&ui.outputPage*5+i<ui.output.size();++i)c.text(38,460+static_cast<int>(i)*14,fitText(c,ui.output[ui.outputPage*5+i],width-76),c.muted);
    }else if(ui.startupPage){
        c.rect(24,153,width-48,360,c.card);c.text(38,176,"User startup files / reversible local toggle",c.ink,true);
        c.text(38,196,"Package database: "+(ui.packages.complete?std::to_string(ui.packages.installed)+" installed / "+std::to_string(ui.packages.incomplete)+" incomplete":"unavailable"),c.muted);
        for(std::size_t i=0;i<10&&ui.page*10+i<ui.startupList.entries.size();++i){const auto& row=ui.startupList.entries[ui.page*10+i];int y=208+static_cast<int>(i)*22;if(ui.startupSelected&&ui.startupSelected->name==row.name)c.rect(30,y,width-60,22,c.line);c.text(38,y+15,row.name,c.ink);c.text(300,y+15,row.disabled?"Moved aside":"Present",c.muted);}
        button(c,40,437,100,"Previous");button(c,150,437,100,"Next");c.text(270,456,"Page "+std::to_string(ui.page+1),c.muted);
        c.text(38,486,"Select a file, then Review toggle. System entries unchanged.",c.muted);
    }else if(ui.tasks){
        c.rect(24,153,width-48,390,c.card);
        const char* sorts[]={"PID","CPU","Memory","Name"};button(c,38,164,122,std::string("Sort: ")+sorts[static_cast<unsigned>(ui.taskSort)]);button(c,170,164,100,"Search",ui.taskSearch);c.text(280,183,fitText(c,ui.taskQuery.empty()?"All processes":ui.taskQuery,width-325),c.ink);
        const int processWidth=std::max(110,width-520),stateX=110+processWidth,parentX=stateX+48,cpuX=parentX+76,rssX=cpuX+100;
        c.text(38,217,"PID",c.muted);c.text(105,217,"PROCESS",c.muted);c.text(stateX,217,"STATE",c.muted);c.text(parentX,217,"PARENT",c.muted);c.text(cpuX,217,"CPU %*",c.muted);c.text(rssX,217,"RSS MiB",c.muted);
        for(std::size_t i=0;i<10&&ui.page*10+i<ui.taskRows.size();++i){const auto& entry=ui.taskRows[ui.page*10+i];const auto& row=entry.process;int y=226+static_cast<int>(i)*22;if(ui.selected&&ui.selected->pid==row.pid&&ui.selected->startTicks==row.startTicks)c.rect(30,y,width-60,22,c.line);c.text(38,y+15,std::to_string(row.pid),c.ink);c.text(105,y+15,fitText(c,safeName(row.name),processWidth-12),c.ink);c.text(stateX,y+15,std::string(1,row.state),c.ink);c.text(parentX,y+15,std::to_string(row.parent),c.ink);c.text(cpuX,y+15,fitText(c,percent(entry.cpuPercent),90),c.ink);char rss[32];if(entry.rssBytes)std::snprintf(rss,sizeof rss,"%.1f",static_cast<double>(*entry.rssBytes)/1048576.0);else std::snprintf(rss,sizeof rss,"unknown");c.text(rssX,y+15,rss,c.ink);}
        button(c,40,457,100,"Previous");button(c,150,457,100,"Next");c.text(270,476,"Page "+std::to_string(ui.page+1)+" | "+std::to_string(ui.taskRows.size())+" rows"+(ui.taskModel.partial?" (partial)":""),c.muted);
        c.text(38,508,"*CPU: percent of one logical CPU. Select process, then Review end task.",c.muted);
    }else{
    const int gap=12,columns=width>=960?4:2,cardWidth=(width-48-(columns-1)*gap)/columns;
    const std::string labels[]={"CPU usage","Memory used","Root storage","Readable processes"};
    const std::string values[]={percent(rates?rates->cpu:std::nullopt),percent(rates?rates->memoryUsed:std::nullopt),capacity(health.rootBytes),std::to_string(inventory.processes.size())};
    for(int i=0;i<4;++i){const int x=24+i%columns*(cardWidth+gap),y=153+i/columns*88;c.rect(x,y,cardWidth,76,c.card);c.rect(x,y,cardWidth,2,c.line);c.text(x+12,y+25,labels[i],c.muted);c.text(x+12,y+54,values[i],i==0?c.green:c.ink,true);}
    const int top=153+4/columns*88;
    c.rect(24,top,width-48,150,c.card);c.text(40,top+24,"Health findings",c.ink,true);
    const char* names[]={"Memory","Root storage","Swap"};int i=0;
    for(auto f:assessHealth(health)){const int y=top+50+i*30;c.text(40,y,names[i],c.ink);c.text(154,y,severity(f.severity),f.severity==triage::Severity::Ok?c.green:c.accent);if(width>=840)c.text(252,y,f.advice,c.muted);++i;}
    const int chartTop=top+158;const int chartHeight=height-76-chartTop;
    if(chartHeight>=48){const int half=(width-60)/2;c.rect(24,chartTop,width-48,chartHeight,c.card);
        auto graph=[&](int x,int w,const char* title,bool cpu){c.text(x+8,chartTop+15,title,c.muted);if(chartHeight<60)return;const auto& rows=ui.history.rows();const int y0=chartTop+22,h=chartHeight-30;int px=0,py=0;bool previous=false;for(std::size_t i=0;i<rows.size();++i){const auto v=cpu?rows[i].cpu:rows[i].memoryUsed;if(!v||!std::isfinite(*v)||*v<0||*v>100){previous=false;continue;}int xx=x+8+static_cast<int>(i)*std::max(1,w-20)/119,yy=y0+h-static_cast<int>(*v*h/100.0);XSetForeground(c.d,c.gc,cpu?c.green:c.accent);if(previous)XDrawLine(c.d,c.w,c.gc,px,py,xx,yy);else XDrawPoint(c.d,c.w,c.gc,xx,yy);px=xx;py=yy;previous=true;}};
        graph(24,half,"CPU history (0-100%)",true);graph(36+half,half,"Memory history (0-100%)",false);
    }
    button(c,width-204,top+4,160,"Backup / restore");
    }
    c.rect(24,height-68,width-48,60,c.card);
    const auto lines=reviewLines(c,ui.message,width-72);
    if(ui.pending&&lines.size()>0&&lines.size()<=2){c.text(36,height-55,lines[0],c.ink);if(lines.size()==2)c.text(36,height-42,lines[1],c.ink);}
    else if(ui.pending)c.text(36,height-48,"Review too long to display. Widen window or cancel.",c.accent);
    else c.text(36,height-48,fitText(c,ui.message,width-72),c.ink);
    if(ui.pending){if(lines.size()>0&&lines.size()<=2)button(c,36,height-39,230,ui.pending==1?"Confirm cache cleanup":ui.pending==3?"Confirm startup toggle":ui.pending==4?"Confirm maintenance":ui.pending==5?"Confirm filesystem action":ui.pending==6?"Confirm verified copy":"Confirm SIGTERM",true);button(c,280,height-39,110,"Cancel");}
    else c.text(36,height-20,ui.busy()?"Operation running. Keep this window open.":"R: refresh evidence   Q: close",c.muted);
    XFlush(c.d);
}
}
int main(int argc,char** argv){
    const bool smoke=argc==2&&std::strcmp(argv[1],"--smoke")==0;
    if(argc>1&&!smoke)return 2;
    Display* d=XOpenDisplay(nullptr);if(!d){std::cerr<<"X11 display unavailable\n";return 1;}
    const int s=DefaultScreen(d);int width=1100,height=620;
    Window w=XCreateSimpleWindow(d,RootWindow(d,s),30,30,width,height,0,BlackPixel(d,s),WhitePixel(d,s));
    XStoreName(d,w,"SysPulse Native - Linux Administration Build");XSizeHints hints{};hints.flags=PMinSize;hints.min_width=640;hints.min_height=620;XSetWMNormalHints(d,w,&hints);
    XSelectInput(d,w,ExposureMask|StructureNotifyMask|KeyPressMask|ButtonPressMask);Atom close=XInternAtom(d,"WM_DELETE_WINDOW",False);XSetWMProtocols(d,w,&close,1);
    GC gc=XCreateGC(d,w,0,nullptr);auto* normal=XLoadQueryFont(d,"-*-helvetica-medium-r-normal--14-*-*-*-*-*-iso8859-1");if(!normal)normal=XLoadQueryFont(d,"fixed");auto* heading=XLoadQueryFont(d,"-*-helvetica-bold-r-normal--18-*-*-*-*-*-iso8859-1");if(!heading)heading=XLoadQueryFont(d,"9x15bold");
    Canvas c{d,w,gc,s,normal,heading,0,0,0,0,0,0,0};c.background=c.color("#edf2f8");c.card=c.color("#ffffff");c.ink=c.color("#142336");c.muted=c.color("#506079");c.accent=c.color("#b51235");c.line=c.color("#cbd6e2");c.green=c.color("#148344");
    XMapWindow(d,w);XSync(d,False);
    auto previous=captureSample();auto health=collectHealth();auto inventory=inspect(256);std::optional<Rates> rates; UI ui;
    auto lastTask=Sample::Clock::now();const auto positive=[](long n){return n>0?static_cast<std::uint64_t>(n):0;};
    auto refreshTasks=[&](bool reset){const auto now=Sample::Clock::now();auto current=inspect(1024);const Inspection empty;ui.taskModel=buildTaskView(reset?empty:inventory,current,std::chrono::duration<double>(now-lastTask).count(),positive(sysconf(_SC_CLK_TCK)),positive(sysconf(_SC_PAGESIZE)));inventory=std::move(current);lastTask=now;ui.selected=retainTaskSelection(ui.taskModel,ui.selected);ui.taskRows=filterTasks(ui.taskModel,ui.taskQuery,ui.taskSort);if(ui.page*10>=ui.taskRows.size())ui.page=0;};
    bool running=true,mapped=true,dirty=true;int draws=0;bool narrowRequested=false;auto next=Sample::Clock::now()+std::chrono::seconds(1);const auto deadline=Sample::Clock::now()+std::chrono::seconds(8);
    auto launch=[&](Operation op,const std::string& unit=std::string{}){auto cmd=commandFor(op,unit);ui.operation=op;ui.output.clear();ui.outputPage=0;ui.cleanPreview=false;ui.trimPreview=false;ui.packagePreview=0;if(!cmd||!ui.job.start(*cmd)){ui.message="Action unavailable or refused (required OS utility missing).";return false;}ui.message="Operation running. Wait for the result.";return true;};
    auto review=[&](Operation op,const std::string& unit=std::string{}){ui.cancel();auto cmd=commandFor(op,(op==Operation::ApplyUpgrade||op==Operation::ApplyRepair)?ui.packageToken:unit);if(op!=Operation::Clean)ui.cleanPreview=false;
    const int required=op==Operation::ApplyUpgrade?1:op==Operation::ApplyRepair?2:0;
    if(!required)ui.packagePreview=0;
    if(required&&(ui.packagePreview!=required||std::chrono::steady_clock::now()>ui.packageExpires)){ui.packagePreview=0;ui.message="Run the matching successful package simulation first.";return;}if(op!=Operation::TrimRoot)ui.trimPreview=false;if(op==Operation::TrimRoot&&(!ui.trimPreview||std::chrono::steady_clock::now()>ui.trimExpires)){ui.message="Run a successful root TRIM preview first.";return;}if((op==Operation::EnableCompression||op==Operation::DisableCompression)&&!compressionChangeAllowed(compressionEvidence(),op==Operation::EnableCompression)){ui.message="Current swap/compression evidence is ineligible or unavailable.";return;}if(op==Operation::Clean&&(!ui.cleanPreview||std::chrono::steady_clock::now()>ui.previewExpires)){ui.message="Run Preview autoclean successfully before cleaning.";return;}if(unit.size()>40){ui.message="Service name too long for exact confirmation; refused.";return;}if(!cmd){ui.message="Action refused by scope or protected-service policy.";return;}if(getuid()==0||getuid()!=geteuid()){ui.message="Use an ordinary desktop account. Review refused.";return;}ui.operation=op;ui.planned=std::move(cmd);ui.expires=std::chrono::steady_clock::now()+std::chrono::seconds(30);ui.pending=4;ui.message=op==Operation::CheckPackages?"Check package dependencies with administrator authorization?":op==Operation::ConfigurePackages?"Configure pending packages? May restart services; save work.":op==Operation::PreviewUpgrade?"Simulate package upgrades only? No installation.":op==Operation::ApplyUpgrade?"Upgrade installed packages (no removals)? Rechecks plan; save work.":op==Operation::PreviewRepair?"Simulate dependency repair with no removals?":op==Operation::ApplyRepair?"Repair dependencies (may install; no removals)? Rechecks plan; save work.":op==Operation::PreviewTrim?"Authorize root filesystem TRIM dry-run?":op==Operation::TrimRoot?"Discard unused blocks on root filesystem only?":op==Operation::EnableCompression?"Enable runtime zswap? Uses RAM and CPU for compression.":op==Operation::DisableCompression?"Disable new runtime zswap compression?":op==Operation::PreviewClean?"Authorize an APT cleanup simulation only (no changes)?":op==Operation::Refresh?"Refresh APT package lists with administrator authorization?":op==Operation::Clean?"Delete obsolete downloaded APT archives? This cannot be undone." :op==Operation::WifiEnable?"Enable Wi-Fi radio?":op==Operation::WifiDisable?"Disable Wi-Fi? This may disconnect your network.":op==Operation::PowerBalanced?"Set the balanced power profile?":op==Operation::PowerSaver?"Set the power-saver profile?":op==Operation::PowerPerformance?"Set performance profile? Battery use and heat may increase.":op==Operation::FirewallStatus?"Authorize read-only firewall status inspection?":(op==Operation::StartSystemService?"Start system service ":op==Operation::StopSystemService?"Stop system service (may interrupt apps) ":op==Operation::RestartSystemService?"Restart system service (may interrupt apps) ":op==Operation::RestartUserService?"Restart user service (may interrupt apps) ":op==Operation::StartService?"Start user service ":"Stop user service (may interrupt apps) ")+unit+"?";};
    while(running){
        ui.desktop.reap();
        if(ui.scanning.valid()&&ui.scanning.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{auto r=ui.scanning.get();std::istringstream in(storageReport(r));std::string line;ui.scanRows.clear();while(std::getline(in,line))ui.scanRows.push_back(line);ui.outputPage=0;ui.message=r.complete?"Storage scan complete within its scope.":"Storage scan partial or unavailable; review coverage.";}catch(...){ui.message="Storage scan failed.";}dirty=true;}
        if(ui.copying.valid()&&ui.copying.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{auto result=ui.copying.get();ui.message=result.message;ui.backupDigest=result.sha256;}catch(const std::exception&){ui.message="Copy worker failed. Inspect the destination before retrying.";}dirty=true;}
        if(auto result=ui.job.poll()){ui.message="Command exit: "+std::to_string(result->code)+(result->truncated?" (output capped)":"")+". Review output; success is not a repair guarantee.";if(ui.operation==Operation::DiagnoseJournal){const auto finding=diagnoseJournal(result->output,result->code==0,result->truncated);result->output=diagnosticReport(finding);const bool saved=rememberAnalysis(finding);ui.message=saved?"Diagnostic sample saved privately. View Local learning history.":"Diagnostic sample unavailable or history not saved.";}std::istringstream in(result->output);std::string line;ui.output.clear();ui.outputPage=0;bool displayComplete=true;while(std::getline(in,line)){for(auto& ch:line)if(static_cast<unsigned char>(ch)<32||static_cast<unsigned char>(ch)>=127)ch='?';for(std::size_t offset=0;offset<std::max<std::size_t>(1,line.size());offset+=88){if(ui.output.size()>=1500){displayComplete=false;break;}ui.output.push_back(line.substr(offset,88));}if(!displayComplete)break;}if(ui.operation==Operation::PreviewClean){ui.cleanPreview=result->code==0&&!result->truncated;ui.previewExpires=std::chrono::steady_clock::now()+std::chrono::seconds(60);}if(ui.operation==Operation::Clean)ui.cleanPreview=false;
    if(ui.operation==Operation::PreviewUpgrade||ui.operation==Operation::PreviewRepair){auto token=result->code==0&&!result->truncated&&displayComplete?aptPlanToken(ui.operation==Operation::PreviewUpgrade?"upgrade":"repair",result->output):std::nullopt;ui.packageToken=token.value_or("");ui.packagePreview=token?(ui.operation==Operation::PreviewUpgrade?1:2):0;if(!token)ui.message="Package plan incomplete or unsafe; apply disabled.";ui.packageExpires=std::chrono::steady_clock::now()+std::chrono::seconds(60);}
    if(ui.operation==Operation::ApplyUpgrade||ui.operation==Operation::ApplyRepair){ui.packagePreview=0;}
    if(ui.operation==Operation::PreviewTrim){ui.trimPreview=result->code==0&&!result->truncated;ui.trimExpires=std::chrono::steady_clock::now()+std::chrono::seconds(60);}if(ui.operation==Operation::TrimRoot)ui.trimPreview=false;if(ui.operation==Operation::EnableCompression||ui.operation==Operation::DisableCompression)ui.compression=compressionEvidence();if(ui.operation==Operation::OfflineInventory){
            auto list=result->code==0&&!result->truncated?offlineDevices(result->output):std::nullopt;
            if(ui.offlineRevalidate){ui.offlineRevalidate=false;
                if(!list||!ui.offlineTarget||std::find(list->begin(),list->end(),*ui.offlineTarget)==list->end()||!mapped){ui.offlineTarget.reset();ui.offlineChecked.reset();ui.message="Device changed, mounted, unavailable or review cancelled.";}
                else{auto cmd=commandFor(Operation::OfflineInfo,ui.offlineTarget->path);if(cmd&&ui.job.start(*cmd)){ui.operation=Operation::OfflineInfo;ui.message="Verifying UDisks device identity...";}else{ui.offlineTarget.reset();ui.offlineChecked.reset();ui.message="UDisks utility missing or refused.";}}
            }else{ui.offlineList=list?*list:std::vector<OfflineDevice>{};ui.offlineSelected.reset();ui.offlineChecked.reset();ui.page=0;ui.message=!list?"Cannot verify offline device inventory.":ui.offlineList.empty()?"No eligible unmounted ext filesystem found.":"Select an eligible offline device.";}
        }else if(ui.operation==Operation::OfflineInfo){
            auto object=result->code==0&&!result->truncated&&ui.offlineTarget&&mapped?offlineObjectInfo(result->output,*ui.offlineTarget):std::nullopt;
            auto op=ui.offlineFix?Operation::OfflineRepair:Operation::OfflineCheck;
            auto cmd=object?commandFor(op,*object):std::nullopt;
            if(cmd&&ui.job.start(*cmd)){ui.operation=op;ui.message="Waiting for UDisks. Do not disconnect or shut down.";}
            else{ui.offlineTarget.reset();ui.offlineChecked.reset();ui.message="UDisks identity changed, unavailable or action refused.";}
        }else if(ui.operation==Operation::OfflineCheck||ui.operation==Operation::OfflineRepair){
            int answer=result->code==0&&!result->truncated?offlineReply(result->output):-1;ui.offlineChecked.reset();
            if(answer==1)ui.message=ui.operation==Operation::OfflineRepair?"UDisks reported repair success. Run a fresh check.":"UDisks reported filesystem consistent.";
            else if(answer==0&&ui.operation==Operation::OfflineCheck&&ui.offlineTarget){ui.offlineChecked=ui.offlineTarget;ui.offlineCheckedAt=std::chrono::steady_clock::now();ui.message="UDisks reported inconsistency. Back up before reviewing repair.";}
            else ui.message="Filesystem operation not confirmed; inspect UDisks before retrying.";
            ui.offlineTarget.reset();
        }else if(ui.operation==Operation::ListServices||ui.operation==Operation::ListSystemServices){ui.services=result->code==0&&!result->truncated?parseServices(result->output):std::vector<Service>{};ui.serviceListFresh=result->code==0&&!result->truncated&&!ui.services.empty();ui.serviceExpires=std::chrono::steady_clock::now()+std::chrono::seconds(60);ui.page=0;ui.selectedService.reset();}else if(ui.operation==Operation::StartService||ui.operation==Operation::StopService||ui.operation==Operation::RestartUserService||ui.operation==Operation::StartSystemService||ui.operation==Operation::StopSystemService||ui.operation==Operation::RestartSystemService){ui.serviceListFresh=false;ui.services.clear();ui.selectedService.reset();}dirty=true;}
        while(XPending(d)){XEvent ev;XNextEvent(d,&ev);switch(ev.type){case Expose:dirty=true;break;case ConfigureNotify:width=ev.xconfigure.width;height=ev.xconfigure.height;if(ui.pending==6){ui.cancel();ui.message="Copy review cancelled after resize.";}dirty=true;break;case UnmapNotify:mapped=false;if(!ui.copying.valid())ui.cancel();ui.serviceListFresh=false;ui.selectedService.reset();ui.offlineTarget.reset();ui.offlineChecked.reset();ui.offlineRevalidate=false;ui.packagePreview=0;ui.trimPreview=false;ui.cleanPreview=false;if(!ui.copying.valid())ui.message="Review cancelled when hidden.";break;case MapNotify:mapped=true;previous=captureSample();rates.reset();ui.history.push({});next=Sample::Clock::now()+std::chrono::seconds(1);dirty=true;break;case ClientMessage:if(static_cast<Atom>(ev.xclient.data.l[0])==close&&!ui.busy())running=false;break;case ButtonPress:{
            const int x=ev.xbutton.x,y=ev.xbutton.y,bw=(width-80-30)/4;
            if(ev.xbutton.button!=Button1||ui.busy())break;
            if(ui.scanPage&&!ui.pending){if(y>=188&&y<218&&x>=38&&x<198){ui.scanRows.clear();ui.outputPage=0;ui.scanning=std::async(std::launch::async,[]{std::atomic_bool cancel{false};return scanAccountHome({},cancel);});ui.message="Scanning home storage...";}else if(y>=188&&y<218&&x>=208&&x<368){ui.scanPage=false;ui.message="Storage recovery.";}else if(y>=508&&y<538&&x>=38&&x<138&&ui.outputPage)--ui.outputPage;else if(y>=508&&y<538&&x>=150&&x<250&&(ui.outputPage+1)*15<ui.scanRows.size())++ui.outputPage;dirty=true;break;}
            if(y>=height-39&&y<height-9&&ui.pending){
                if(x>=36&&x<266){const auto lines=reviewLines(c,ui.message,width-72);if(lines.empty()||lines.size()>2){dirty=true;break;}if(ui.pending==1){auto result=ui.cache.apply();const auto status=!result.completed||result.skipped?LearningResult::Failure:result.removed?LearningResult::Applied:LearningResult::NoEffect;const bool recorded=rememberOutcome({ui.cacheContext,ui.cacheAction,status,status==LearningResult::Applied?result.bytes:0,result.availableBefore,result.availableAfter});ui.message="Removed "+std::to_string(result.removed)+", skipped "+std::to_string(result.skipped)+", "+std::to_string(result.bytes)+" logical bytes. "+result.status+(recorded?"":" History not saved.");}
                else if(ui.pending==4){const bool serviceAction=ui.operation==Operation::StartService||ui.operation==Operation::StopService||ui.operation==Operation::RestartUserService||ui.operation==Operation::StartSystemService||ui.operation==Operation::StopSystemService||ui.operation==Operation::RestartSystemService;if((!serviceAction||(ui.serviceListFresh&&std::chrono::steady_clock::now()<=ui.serviceExpires))&&ui.planned&&std::chrono::steady_clock::now()<=ui.expires&&(ui.operation!=Operation::Clean||(ui.cleanPreview&&std::chrono::steady_clock::now()<=ui.previewExpires))&&(ui.operation!=Operation::TrimRoot||(ui.trimPreview&&std::chrono::steady_clock::now()<=ui.trimExpires))&&((ui.operation!=Operation::ApplyUpgrade&&ui.operation!=Operation::ApplyRepair)||(ui.packagePreview==(ui.operation==Operation::ApplyUpgrade?1:2)&&std::chrono::steady_clock::now()<=ui.packageExpires))&&ui.job.start(*ui.planned)){ui.output.clear();ui.message="Operation running. Keep this window open.";}else ui.message="Action refused, expired or required OS utility unavailable.";}
                else if(ui.pending==5){
                    if(ui.offlineTarget&&getuid()!=0&&getuid()==geteuid()&&std::chrono::steady_clock::now()<=ui.expires&&(!ui.offlineFix||(ui.offlineChecked&&*ui.offlineChecked==*ui.offlineTarget&&std::chrono::steady_clock::now()-ui.offlineCheckedAt<=std::chrono::minutes(5)))){
                        auto cmd=commandFor(Operation::OfflineInventory);ui.offlineChecked.reset();ui.offlineRevalidate=true;ui.operation=Operation::OfflineInventory;
                        if(cmd&&ui.job.start(*cmd))ui.message="Revalidating device and mount state...";else{ui.offlineRevalidate=false;ui.offlineTarget.reset();ui.message="Cannot revalidate offline device.";}
                    }else{ui.offlineTarget.reset();ui.offlineChecked.reset();ui.message="Offline review expired or no ordinary account.";}
                }
                else if(ui.pending==6){ui.copying=std::async(std::launch::async,[&ui]{return ui.backup.apply();});ui.pending=0;ui.backupPlan.reset();ui.message="Copying and verifying. Keep this window open.";dirty=true;break;}
                else if(ui.pending==3){ui.message=ui.startup.apply()?"Startup file moved. Effective at next login.":"Toggle refused, expired or file changed.";ui.startupList=ui.startup.scan();ui.startupSelected.reset();}
                else ui.message=ui.process.terminate()?"SIGTERM accepted by kernel. Refresh to check exit.":"Request refused, expired or process already gone.";
                ui.cancel();health=collectHealth();if(ui.tasks)refreshTasks(true);else inventory=inspect(256);ui.selected.reset();ui.page=0;}
                else if(x>=280&&x<390){ui.cancel();ui.offlineTarget.reset();ui.offlineChecked.reset();ui.packagePreview=0;ui.cleanPreview=false;ui.trimPreview=false;ui.trimPreview=false;ui.cleanPreview=false;ui.message="Cancelled. No changes made.";}
                dirty=true;break;
            }
            if(y>=18&&y<48&&x>=width-220&&x<width-24){ui.cancel();ui.hubPage=!ui.hubPage;ui.backupPage=ui.tasks=ui.startupPage=ui.maintenancePage=ui.toolsPage=ui.advancedPage=ui.storagePage=false;ui.packagePreview=0;ui.cleanPreview=ui.trimPreview=false;ui.offlineChecked.reset();ui.offlineTarget.reset();ui.serviceListFresh=false;ui.outputPage=0;
                if(ui.hubPage){for(std::size_t i=0;i<ui.available.size();++i)ui.available[i]=desktopAvailable(i);ui.report=healthReport(collectHealth(),collectPressure(),inspect(256));ui.message="Desktop tools and local report. Nothing runs automatically.";}else ui.message="Overview.";dirty=true;break;}
            if(y>=108&&y<138){int which=-1;for(int i=0;i<4;++i)if(x>=40+i*(bw+10)&&x<40+i*(bw+10)+bw)which=i;if(which<0)break;ui.hubPage=false;ui.cancel();ui.offlineTarget.reset();ui.offlineChecked.reset();ui.packagePreview=0;
                if(which==0){if(ui.backupPage){ui.backupPage=false;ui.message="Overview.";}else if(ui.tasks||ui.startupPage||ui.maintenancePage){ui.tasks=false;ui.startupPage=false;ui.maintenancePage=false;ui.toolsPage=false;ui.advancedPage=false;ui.storagePage=false;ui.trimPreview=false;ui.message="Overview.";}else{ui.cacheAction=LearningAction::Thumbnails;ui.cacheContext=contextFor(health);ui.summary=ui.cache.scan();ui.pending=ui.summary.files?1:0;ui.message=std::to_string(ui.summary.files)+" files / "+std::to_string(ui.summary.bytes)+" bytes. "+ui.summary.status+learnedAdvice(ui.cacheContext,ui.cacheAction);}}
                if(which==1){ui.backupPage=false;if(ui.maintenancePage){if(ui.storagePage){ui.storagePage=false;ui.advancedPage=false;ui.toolsPage=false;}else if(ui.advancedPage){ui.advancedPage=false;ui.storagePage=true;ui.offlineList.clear();ui.offlineSelected.reset();ui.page=0;launch(Operation::OfflineInventory);}else if(ui.toolsPage){ui.toolsPage=false;ui.advancedPage=true;}else{ui.toolsPage=true;ui.compression=compressionEvidence();}ui.trimPreview=false;if(!ui.storagePage)ui.message=ui.advancedPage?"Network, power and logs.":ui.toolsPage?"System tools: select a reviewed action.":"Package and service controls.";}else if(ui.startupPage){if(ui.startupSelected&&ui.startup.prepare(*ui.startupSelected)){ui.pending=3;ui.message=(ui.startupSelected->disabled?"Restore ":"Move aside ")+ui.startupSelected->name+" at next login?";}else ui.message="Select a current eligible startup file.";}else if(!ui.tasks){ui.tasks=true;ui.selected.reset();ui.page=0;ui.taskSearch=false;ui.taskQuery.clear();refreshTasks(true);ui.message="Select a process to review.";}else if(ui.selected&&ui.process.prepare(*ui.selected)){ui.pending=2;ui.message="End PID "+std::to_string(ui.selected->pid)+" ("+safeName(ui.selected->name)+")? Save work first.";}else ui.message="Select a current owned, unprotected process. Review refused.";}
                if(which==2){ui.backupPage=false;ui.storagePage=false;ui.advancedPage=false;ui.toolsPage=false;ui.trimPreview=false;ui.maintenancePage=false;ui.tasks=false;ui.startupPage=true;ui.page=0;ui.startupSelected.reset();ui.startupList=ui.startup.scan();ui.packages=auditPackages();ui.message=ui.startupList.status;}
                if(which==3){ui.backupPage=false;ui.storagePage=false;ui.advancedPage=false;ui.toolsPage=false;ui.trimPreview=false;ui.tasks=false;ui.startupPage=false;ui.maintenancePage=true;ui.page=0;ui.selectedService.reset();ui.services.clear();ui.serviceListFresh=false;launch(ui.systemServices?Operation::ListSystemServices:Operation::ListServices);}dirty=true;break;
            }
            if(ui.hubPage){const int mw=(width-100)/3;int index=-1;for(int i=0;i<15;++i){const int bx=38+(i%3)*(mw+8),by=188+(i/3)*37+(i>=12?8:0);if(x>=bx&&x<bx+mw&&y>=by&&y<by+30)index=i;}
                if(index>=0&&index<12){ui.available[index]=desktopAvailable(index);ui.message=ui.desktop.launch(index)?"Desktop tool launched; changes are managed by that tool.":"Tool unavailable or launch refused. Use a normal desktop account.";}
                else if(index==12){for(std::size_t i=0;i<ui.available.size();++i)ui.available[i]=desktopAvailable(i);ui.report=healthReport(collectHealth(),collectPressure(),inspect(256));ui.outputPage=0;ui.message="Local report refreshed.";}
                else if(index==13){auto result=exportHealthReport(ui.report);ui.message=result.message;}
                else if(index==14){ui.hubPage=false;ui.message="Overview.";}else if(x>=38&&x<228&&y>=380&&y<410){auto h=readLearningHistory();ui.report=h?learningReport(*h):"Local learning history unavailable.\n";ui.outputPage=0;ui.message="Local evidence history; no automatic actions.";}
                dirty=true;break;
            }
            if(ui.backupPage){
                if(y>=262&&y<293){ui.cancel();ui.backupField=0;}
                else if(y>=332&&y<363){ui.cancel();ui.backupField=1;}
                else if(y>=380&&y<410&&x>=38&&x<218){ui.cancel();ui.backupDigest.clear();if(c.textWidth(ui.backupSource)>width-92||c.textWidth(ui.backupDestination)>width-92)ui.message="Widen window until both full paths are visible before review.";else{ui.backupPlan=ui.backup.prepare(ui.backupSource,ui.backupDestination);ui.pending=ui.backupPlan?6:0;ui.message=ui.backupPlan?"Copy "+std::to_string(ui.backupPlan->bytes)+" bytes to the displayed NEW path?":"Review refused: path, ownership, type, size or existing target.";}}
                else if(y>=380&&y<410&&x>=230&&x<380){ui.cancel();ui.backupSource.clear();ui.backupDestination.clear();ui.backupDigest.clear();ui.message="Paths cleared.";}
                dirty=true;break;
            }
            if(!ui.tasks&&!ui.startupPage&&!ui.maintenancePage){const int top=153+4/(width>=960?4:2)*88;if(x>=width-204&&x<width-44&&y>=top+4&&y<top+34){ui.cancel();ui.backupPage=true;ui.message="Enter the source and a new destination path.";dirty=true;break;}}
            if(ui.storagePage){if(y>=508&&y<538&&x>=width-370&&x<width-215){ui.cancel();ui.scanPage=true;ui.scanRows.clear();ui.outputPage=0;ui.scanning=std::async(std::launch::async,[]{std::atomic_bool cancel{false};return scanAccountHome({},cancel);});ui.message="Scanning home storage...";dirty=true;break;}if(y>=508&&y<538&&x>=width-205&&x<width-44){ui.cancel();ui.cacheAction=LearningAction::FailedThumbnails;ui.cacheContext=contextFor(health);ui.summary=ui.cache.scan(true);ui.pending=ui.summary.files?1:0;ui.message=std::to_string(ui.summary.files)+" failed thumbnail markers / "+std::to_string(ui.summary.bytes)+" bytes. Removal may trigger retries."+learnedAdvice(ui.cacheContext,ui.cacheAction);dirty=true;break;}const int mw=(width-100)/3;int col=-1;for(int i=0;i<3;++i)if(x>=38+i*(mw+8)&&x<38+i*(mw+8)+mw)col=i;
                if(col>=0&&y>=188&&y<218){ui.cancel();ui.offlineTarget.reset();
                    if(col==0){ui.offlineChecked.reset();ui.offlineSelected.reset();ui.offlineList.clear();launch(Operation::OfflineInventory);}
                    else if(getuid()==0||getuid()!=geteuid())ui.message="Use an ordinary desktop account.";
                    else if(!ui.offlineSelected)ui.message="Select an eligible offline filesystem first.";
                    else if(col==2&&(!ui.offlineChecked||!(*ui.offlineChecked==*ui.offlineSelected)||std::chrono::steady_clock::now()-ui.offlineCheckedAt>std::chrono::minutes(5)))ui.message="Run a fresh check reporting inconsistency before repair.";
                    else{ui.offlineFix=col==2;ui.offlineTarget=ui.offlineSelected;ui.expires=std::chrono::steady_clock::now()+std::chrono::seconds(30);ui.pending=5;ui.message=ui.offlineFix?"Repair "+ui.offlineTarget->path+"? Confirm only after backup; data may be lost.":"Check "+ui.offlineTarget->path+" without repairing?";}}
                else if(x>=32&&x<width-32&&y>=250&&y<375){ui.cancel();ui.offlineChecked.reset();ui.offlineTarget.reset();auto i=ui.page*5+static_cast<std::size_t>((y-250)/25);if(i<ui.offlineList.size()){ui.offlineSelected=ui.offlineList[i];ui.message="Selected "+ui.offlineSelected->path;}}
                else if(y>=385&&y<415){ui.cancel();ui.offlineSelected.reset();ui.offlineChecked.reset();if(x>=38&&x<138&&ui.page)--ui.page;else if(x>=150&&x<250&&(ui.page+1)*5<ui.offlineList.size())++ui.page;}
                dirty=true;break;}
            if(ui.advancedPage){const int mw=(width-100)/3;int index=-1;for(int i=0;i<15;++i){int bx=38+(i%3)*(mw+8),by=188+(i/3)*37;if(x>=bx&&x<bx+mw&&y>=by&&y<by+30)index=i;}
                if(index>=0){ui.cancel();ui.cleanPreview=false;ui.packagePreview=0;ui.trimPreview=false;switch(index){
                    case 0:launch(Operation::NetworkStatus);break;case 1:launch(Operation::NetworkRoutes);break;case 2:launch(Operation::SocketStatus);break;case 3:launch(Operation::WifiStatus);break;
                    case 4:review(Operation::WifiEnable);break;case 5:review(Operation::WifiDisable);break;case 6:launch(Operation::PowerStatus);break;
                    case 7:review(Operation::PowerBalanced);break;case 8:review(Operation::PowerSaver);break;case 9:review(Operation::PowerPerformance);break;
                    case 10:launch(Operation::JournalErrors);break;case 11:launch(Operation::DiagnoseJournal);break;case 12:review(Operation::FirewallStatus);break;case 13:launch(Operation::TimerStatus);break;
                    case 14:ui.advancedPage=false;ui.toolsPage=false;ui.storagePage=true;ui.offlineList.clear();ui.offlineSelected.reset();ui.offlineChecked.reset();ui.page=0;launch(Operation::OfflineInventory);break;default:break;}}
                dirty=true;break;}
            if(ui.toolsPage){const int mw=(width-100)/3;int index=-1;for(int i=0;i<15;++i){int bx=38+(i%3)*(mw+8),by=188+(i/3)*37;if(x>=bx&&x<bx+mw&&y>=by&&y<by+30)index=i;}
                if(index>=0){ui.cancel();if(index!=11&&index!=13)ui.packagePreview=0;switch(index){
                    case 0:review(Operation::CheckPackages);break;case 1:review(Operation::ConfigurePackages);break;case 2:review(Operation::PreviewUpgrade);break;
                    case 3:ui.trimPreview=false;launch(Operation::DiskInventory);break;case 4:review(Operation::PreviewTrim);break;case 5:review(Operation::TrimRoot);break;
                    case 6:ui.trimPreview=false;launch(Operation::FailedServices);break;case 7:review(Operation::EnableCompression);break;case 8:review(Operation::DisableCompression);break;
                    case 9:{ui.trimPreview=false;if(getuid()==0||getuid()!=geteuid()){ui.message="Use an ordinary desktop account.";break;}auto r=trimOwnMemory();const auto state=r.supported&&r.before&&r.after?(r.released&&*r.after<*r.before?LearningResult::Applied:LearningResult::NoEffect):LearningResult::Failure;const bool saved=rememberOutcome({contextFor(health),LearningAction::OwnMemory,state,0,r.before&&r.after?r.before:std::nullopt,r.before&&r.after?r.after:std::nullopt});ui.message=r.supported&&r.before&&r.after?"Own RSS: "+std::to_string(*r.before)+" -> "+std::to_string(*r.after)+" KiB (not system-wide)":"Own memory measurement unavailable.";if(!saved)ui.message+=" History not saved.";break;}
                    case 10:ui.trimPreview=false;ui.compression=compressionEvidence();health=collectHealth();ui.message="Evidence refreshed.";break;
                    case 11:review(Operation::ApplyUpgrade);break;case 12:review(Operation::PreviewRepair);break;case 13:review(Operation::ApplyRepair);break;case 14:ui.toolsPage=false;ui.advancedPage=true;ui.trimPreview=false;ui.message="Network, power and logs.";break;default:break;}}
                dirty=true;break;}
            if(ui.maintenancePage){const int mw=(width-100)/3;int col=-1;for(int i=0;i<3;++i)if(x>=38+i*(mw+8)&&x<38+i*(mw+8)+mw)col=i;
                if(col>=0&&y>=188&&y<218){ui.cancel();if(col==0)launch(Operation::Audit);else if(col==1)review(Operation::PreviewClean);else review(Operation::Refresh);}
                else if(col>=0&&y>=225&&y<255){if(col==0)review(Operation::Clean);else if(ui.serviceListFresh&&std::chrono::steady_clock::now()<=ui.serviceExpires&&ui.selectedService)review(ui.systemServices?(col==1?Operation::StartSystemService:Operation::StopSystemService):(col==1?Operation::StartService:Operation::StopService),ui.selectedService->name);else ui.message="Refresh services and select a current row.";}
                else if(x>=32&&x<width-32&&y>=282&&y<392){ui.cancel();auto i=ui.page*5+static_cast<std::size_t>((y-282)/22);if(i<ui.services.size()){ui.selectedService=ui.services[i];ui.message="Selected "+ui.selectedService->name;}}
                else if(y>=395&&y<425){ui.cancel();if(x>=402&&x<514){if(ui.serviceListFresh&&std::chrono::steady_clock::now()<=ui.serviceExpires&&ui.selectedService)review(ui.systemServices?Operation::RestartSystemService:Operation::RestartUserService,ui.selectedService->name);else ui.message="Refresh services and select a current row.";}
                    else{ui.selectedService.reset();if(x>=38&&x<138&&ui.page)--ui.page;else if(x>=150&&x<250&&(ui.page+1)*5<ui.services.size())++ui.page;else if(x>=262&&x<390){ui.systemServices=!ui.systemServices;ui.services.clear();ui.serviceListFresh=false;ui.page=0;launch(ui.systemServices?Operation::ListSystemServices:Operation::ListServices);}}}
                dirty=true;break;}
            if(ui.startupPage&&x>=30&&x<width-30){if(y>=208&&y<428){auto i=ui.page*10+static_cast<std::size_t>((y-208)/22);if(i<ui.startupList.entries.size()){ui.cancel();ui.startupSelected=ui.startupList.entries[i];ui.message="Selected "+ui.startupSelected->name;}}else if(y>=437&&y<467){ui.cancel();ui.startupSelected.reset();if(x>=40&&x<140&&ui.page)--ui.page;else if(x>=150&&x<250&&(ui.page+1)*10<ui.startupList.entries.size())++ui.page;}
                dirty=true;break;}
            if(ui.tasks&&x>=30&&x<width-30){if(y>=164&&y<194){ui.cancel();ui.selected.reset();ui.page=0;if(x>=38&&x<160){ui.taskSort=static_cast<TaskSort>((static_cast<unsigned>(ui.taskSort)+1)%4);ui.taskRows=filterTasks(ui.taskModel,ui.taskQuery,ui.taskSort);}else if(x>=170&&x<270){ui.taskSearch=!ui.taskSearch;ui.taskQuery.clear();ui.taskRows=filterTasks(ui.taskModel,ui.taskQuery,ui.taskSort);ui.message=ui.taskSearch?"Type a process name or PID; Enter finishes search.":"Search cleared.";}dirty=true;break;}if(y>=226&&y<446){auto i=ui.page*10+static_cast<std::size_t>((y-226)/22);if(i<ui.taskRows.size()){ui.cancel();ui.selected=ui.taskRows[i].process;ui.message="Selected PID "+std::to_string(ui.selected->pid)+". Review end task to continue.";}}
            else if(y>=457&&y<487){ui.cancel();ui.selected.reset();if(x>=40&&x<140&&ui.page) --ui.page;else if(x>=150&&x<250&&(ui.page+1)*10<ui.taskRows.size())++ui.page;ui.message="Select a process to review.";}
                dirty=true;}break;}
            case KeyPress:{auto key=XLookupKeysym(&ev.xkey,0);if(ui.busy())break;if(ui.tasks&&ui.taskSearch&&!ui.pending){if(key==XK_Return||key==XK_Escape)ui.taskSearch=false;else if(key==XK_BackSpace){if(!ui.taskQuery.empty())ui.taskQuery.pop_back();}else{char typed[16];KeySym actual;int n=XLookupString(&ev.xkey,typed,sizeof typed,&actual,nullptr);for(int i=0;i<n&&ui.taskQuery.size()<128;++i)if(static_cast<unsigned char>(typed[i])>=32&&static_cast<unsigned char>(typed[i])<127)ui.taskQuery+=typed[i];}ui.selected.reset();ui.page=0;ui.taskRows=filterTasks(ui.taskModel,ui.taskQuery,ui.taskSort);dirty=true;break;}if(ui.backupPage&&key!=XK_Escape){ui.cancel();if(key==XK_Tab)ui.backupField=1-ui.backupField;else{auto& field=ui.backupField?ui.backupDestination:ui.backupSource;if(key==XK_BackSpace){if(!field.empty())field.pop_back();}else{char typed[16];KeySym actual;const int n=XLookupString(&ev.xkey,typed,sizeof typed,&actual,nullptr);for(int i=0;i<n&&field.size()<240;++i)if(static_cast<unsigned char>(typed[i])>=32&&static_cast<unsigned char>(typed[i])<127)field+=typed[i];}}ui.message="Edit paths, then Review copy.";dirty=true;break;}if(ui.hubPage&&key==XK_Page_Down){if((ui.outputPage+1)*6<static_cast<std::size_t>(std::count(ui.report.begin(),ui.report.end(),'\n')))++ui.outputPage;dirty=true;break;}if(ui.hubPage&&key==XK_Page_Up){if(ui.outputPage)--ui.outputPage;dirty=true;break;}if(ui.maintenancePage&&key==XK_Page_Down){if((ui.outputPage+1)*5<ui.output.size())++ui.outputPage;dirty=true;break;}if(ui.maintenancePage&&key==XK_Page_Up){if(ui.outputPage)--ui.outputPage;dirty=true;break;}if(key==XK_Escape&&ui.pending){ui.cancel();ui.offlineTarget.reset();ui.offlineChecked.reset();ui.packagePreview=0;ui.trimPreview=false;ui.cleanPreview=false;ui.message="Cancelled. No changes made.";dirty=true;}else if(key==XK_q||key==XK_Escape)running=false;else if(key==XK_r){ui.cancel();ui.offlineTarget.reset();ui.offlineChecked.reset();ui.packagePreview=0;ui.trimPreview=false;ui.cleanPreview=false;if(ui.startupPage){ui.startupList=ui.startup.scan();ui.packages=auditPackages();ui.startupSelected.reset();}ui.selected.reset();ui.page=0;health=collectHealth();if(ui.tasks)refreshTasks(true);else inventory=inspect(256);ui.message="Evidence refreshed. Pending action cancelled.";dirty=true;}break;}default:break;}}
        const auto now=Sample::Clock::now();if(mapped&&ui.tasks&&!ui.pending&&!ui.busy()&&now-lastTask>=std::chrono::seconds(2)) {refreshTasks(false);dirty=true;}
        if(mapped&&now>=next){auto current=captureSample();rates=sampleRates(previous,current);if(rates)ui.history.push(*rates);else ui.history.push({});previous=std::move(current);health.memory=previous.system.memory;next=now+std::chrono::seconds(1);dirty=true;}
        if(mapped&&dirty){render(c,width,height,rates,health,inventory,ui);dirty=false;++draws;}
        if(smoke&&draws>=2&&rates&&rates->cpu){
            XSync(d,False);auto* image=XGetImage(d,w,10,2,1,1,AllPlanes,ZPixmap);const bool ok=image&&XGetPixel(image,0,0)==c.accent;if(image)XDestroyImage(image);if(!ok)return 3;
            if(!narrowRequested){narrowRequested=true;XResizeWindow(d,w,640,620);XFlush(d);}
            else if(width==640&&height==620){auto* cardPixel=XGetImage(d,w,30,245,1,1,AllPlanes,ZPixmap);const bool wrapped=cardPixel&&XGetPixel(cardPixel,0,0)==c.card;if(cardPixel)XDestroyImage(cardPixel);if(!wrapped)return 5;std::cout<<"X11 wide/narrow rendering and live sampling smoke passed\n";break;}
        }
        if(smoke&&now>=deadline)return 4;
        pollfd fd{ConnectionNumber(d),POLLIN,0};const auto wait=std::chrono::duration_cast<std::chrono::milliseconds>(next-Sample::Clock::now()).count();poll(&fd,1,(smoke||ui.busy())?100:mapped?static_cast<int>(std::clamp<long long>(wait,1,1000)):-1);
    }
    if(normal)XFreeFont(d,normal);
    if(heading)XFreeFont(d,heading);
    XFreeGC(d,gc);XDestroyWindow(d,w);XCloseDisplay(d);return 0;
}
