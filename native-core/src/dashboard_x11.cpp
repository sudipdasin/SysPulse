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
#include <map>
#include <sstream>
#include "syspulse/native/startup.hpp"
#include "syspulse/native/actions.hpp"
#include "syspulse/native/device_rates.hpp"
#include <X11/Xlib.h>
#include "syspulse/native/gui_layout.hpp"
#include "syspulse/native/gui_cairo.hpp"
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
    Display* d; Drawable w; GC gc; int screen; XFontStruct* normal; XFontStruct* heading; XFontStruct* titleFont=nullptr;
    unsigned long background{},card{},ink{},muted{},accent{},line{},green{},soft{},selected{},blue{},purple{},disabledInk{};
    int origin=0,focus=-1; bool blocked=false,frameReady=false; std::vector<GuiRect> targets,disabled;
#if defined(SYSPULSE_GUI_CAIRO)
    cairo_surface_t* surface=nullptr; cairo_t* context=nullptr;
    std::map<unsigned long,std::array<double,3>> colors;
    void font(double size,bool bold)const {cairo_select_font_face(context,"sans-serif",CAIRO_FONT_SLANT_NORMAL,bold?CAIRO_FONT_WEIGHT_BOLD:CAIRO_FONT_WEIGHT_NORMAL);cairo_set_font_size(context,size);}
    void source(unsigned long pixel){const auto it=colors.find(pixel);const auto rgb=it==colors.end()?std::array<double,3>{0,0,0}:it->second;cairo_set_source_rgb(context,rgb[0],rgb[1],rgb[2]);}
    // UI data is ASCII in this backend; never feed malformed UTF-8 to Cairo.
    static std::string printable(const std::string& v){std::string out=v;for(auto& ch:out)if(static_cast<unsigned char>(ch)<32||static_cast<unsigned char>(ch)>=127)ch='?';return out;}
#endif
    Canvas(Display* display,Window win,GC context_,int scr,XFontStruct* font_,XFontStruct* bold):d(display),w(win),gc(context_),screen(scr),normal(font_),heading(bold){}
    void endFrame(){
#if defined(SYSPULSE_GUI_CAIRO)
        if(context){cairo_destroy(context);context=nullptr;}if(surface){cairo_surface_flush(surface);cairo_surface_destroy(surface);surface=nullptr;}
#endif
    }
    void flushFrame(){
#if defined(SYSPULSE_GUI_CAIRO)
        if(surface)cairo_surface_flush(surface);
#endif
    }
    void beginFrame(Drawable target,int width,int height){
#if defined(SYSPULSE_GUI_CAIRO)
        if(context&&w==target&&cairo_status(context)==CAIRO_STATUS_SUCCESS)return;
#endif
        endFrame();w=target;
#if defined(SYSPULSE_GUI_CAIRO)
        surface=cairo_xlib_surface_create(d,target,DefaultVisual(d,screen),width,height);
        if(surface&&cairo_surface_status(surface)==CAIRO_STATUS_SUCCESS){context=cairo_create(surface);if(cairo_status(context)!=CAIRO_STATUS_SUCCESS)endFrame();}else endFrame();
#else
        (void)width;(void)height;
#endif
    }
    unsigned long color(const char* hex){XColor actual{},exact{};if(!XAllocNamedColor(d,DefaultColormap(d,screen),hex,&actual,&exact))return BlackPixel(d,screen);
#if defined(SYSPULSE_GUI_CAIRO)
        colors[actual.pixel]={actual.red/65535.0,actual.green/65535.0,actual.blue/65535.0};
#endif
        return actual.pixel;}
    int textWidth(const std::string& value,bool large=false)const{
#if defined(SYSPULSE_GUI_CAIRO)
        if(context){font(large?18:14,large);cairo_text_extents_t ext{};cairo_text_extents(context,printable(value).c_str(),&ext);return static_cast<int>(std::ceil(std::max(ext.x_advance,ext.x_bearing+ext.width)));}
#endif
        auto* f=large&&heading?heading:normal;return f?XTextWidth(f,value.c_str(),static_cast<int>(value.size())):static_cast<int>(value.size())*6;
    }
    void rect(int x,int y,int width,int height,unsigned long pixel){if(width<=0||height<=0)return;
#if defined(SYSPULSE_GUI_CAIRO)
        if(context){source(pixel);cairo_rectangle(context,x+origin,y,width,height);cairo_fill(context);return;}
#endif
        XSetForeground(d,gc,pixel);XFillRectangle(d,w,gc,x+origin,y,width,height);}
    void stroke(int x,int y,int width,int height,unsigned long pixel){if(width<=0||height<=0)return;
#if defined(SYSPULSE_GUI_CAIRO)
        if(context){source(pixel);cairo_set_line_width(context,1.5);cairo_rectangle(context,x+origin+0.5,y+0.5,width-1,height-1);cairo_stroke(context);return;}
#endif
        XSetForeground(d,gc,pixel);XDrawRectangle(d,w,gc,x+origin,y,width-1,height-1);}
    void rounded(int x,int y,int width,int height,unsigned long pixel,int radius=8){if(width<=0||height<=0)return;const int r=std::min({radius,width/2,height/2});if(r<2){rect(x,y,width,height,pixel);return;}
#if defined(SYSPULSE_GUI_CAIRO)
        if(context){const double pi=3.14159265358979323846;const double left=x+origin,right=left+width,bottom=y+height;source(pixel);cairo_new_path(context);cairo_arc(context,right-r,y+r,r,-pi/2,0);cairo_arc(context,right-r,bottom-r,r,0,pi/2);cairo_arc(context,left+r,bottom-r,r,pi/2,pi);cairo_arc(context,left+r,y+r,r,pi,3*pi/2);cairo_close_path(context);cairo_fill(context);return;}
#endif
        XSetForeground(d,gc,pixel);x+=origin;XFillRectangle(d,w,gc,x+r,y,width-2*r,height);XFillRectangle(d,w,gc,x,y+r,width,height-2*r);for(int i=0;i<4;++i)XFillArc(d,w,gc,x+(i%2)*(width-2*r),y+(i/2)*(height-2*r),2*r,2*r,0,360*64);}
    void panel(int x,int y,int width,int height){rounded(x,y,width,height,line);rounded(x+1,y+1,width-2,height-2,card,7);}
    void drawLine(int x,int y,int x2,int y2,unsigned long pixel){
#if defined(SYSPULSE_GUI_CAIRO)
        if(context){source(pixel);cairo_set_line_width(context,1.0);cairo_move_to(context,x+origin+0.5,y+0.5);cairo_line_to(context,x2+origin+0.5,y2+0.5);cairo_stroke(context);return;}
#endif
        XSetForeground(d,gc,pixel);XDrawLine(d,w,gc,x+origin,y,x2+origin,y2);}
    void text(int x,int y,const std::string& value,unsigned long pixel,bool large=false){
#if defined(SYSPULSE_GUI_CAIRO)
        if(context){font(large?18:14,large);source(pixel);cairo_move_to(context,x+origin,y);cairo_show_text(context,printable(value).c_str());return;}
#endif
        auto* f=large&&heading?heading:normal;if(f)XSetFont(d,gc,f->fid);XSetForeground(d,gc,pixel);XDrawString(d,w,gc,x+origin,y,value.c_str(),static_cast<int>(value.size()));}
    void title(int x,int y,const std::string& value){
#if defined(SYSPULSE_GUI_CAIRO)
        if(context){font(26,true);source(ink);cairo_move_to(context,x+origin,y);cairo_show_text(context,printable(value).c_str());return;}
#endif
        if(titleFont){XSetFont(d,gc,titleFont->fid);XSetForeground(d,gc,ink);XDrawString(d,w,gc,x+origin,y,value.c_str(),static_cast<int>(value.size()));}else text(x,y,value,ink,true);}
    void registerButton(int x,int y,int width,int height,bool enabled){GuiRect r{x+origin,y,width,height};if(enabled)targets.push_back(r);else disabled.push_back(r);}
};
std::string percent(const std::optional<double>& v){if(!v)return "Unavailable";char b[32];std::snprintf(b,sizeof b,"%.1f%%",*v);return b;}
std::string capacity(const std::optional<triage::Counters>& v){if(!v||!triage::valid(v))return "Unavailable";char b[64];std::snprintf(b,sizeof b,"%.1f GiB free",static_cast<double>(v->available)/1073741824.0);return b;}
const char* severity(triage::Severity s){switch(s){case triage::Severity::Unknown:return "UNKNOWN";case triage::Severity::Ok:return "OK";case triage::Severity::Info:return "INFO";case triage::Severity::Warning:return "REVIEW";case triage::Severity::Critical:return "CRITICAL";}return "UNKNOWN";}
struct UI {
    LearningAction cacheAction=LearningAction::Thumbnails; LearningContext cacheContext=LearningContext::Unknown;
    bool hubPage=false; DesktopLauncher desktop; std::string report; std::array<bool,12> available{};
    bool backupPage=false; int backupField=0; std::string backupSource,backupDestination,backupDigest; BackupReview backup; std::optional<CopyPlan> backupPlan; std::future<CopyOutcome> copying; History history{120};
    TaskView taskModel; std::vector<TaskRow> taskRows; TaskSort taskSort=TaskSort::Pid; std::string taskQuery; bool taskSearch=false;
    std::future<StorageScan> scanning; bool scanPage=false,performancePage=false; std::optional<DeviceSample> deviceBaseline;std::vector<DeviceRate> deviceRows; std::vector<std::string> scanRows;
    bool busy()const{return job.active()||copying.valid()||scanning.valid();}
    CacheReview cache; ProcessReview process; CleanSummary summary; std::string message="Choose an action. Nothing runs automatically.";
    StartupReview startup; StartupInventory startupList; PackageAudit packages; std::optional<StartupEntry> startupSelected;
    bool tasks=false, startupPage=false, maintenancePage=false, toolsPage=false, advancedPage=false, systemServices=false, storagePage=false; std::vector<OfflineDevice> offlineList; std::optional<OfflineDevice> offlineSelected,offlineChecked,offlineTarget; bool offlineRevalidate=false, offlineFix=false; std::chrono::steady_clock::time_point offlineCheckedAt,serviceExpires; CompressionEvidence compression; bool trimPreview=false; int packagePreview=0; std::string packageToken; std::chrono::steady_clock::time_point packageExpires; std::chrono::steady_clock::time_point trimExpires; Job job; Operation operation=Operation::Audit; std::optional<Command> planned; std::chrono::steady_clock::time_point expires; std::vector<Service> services; std::optional<Service> selectedService; std::vector<std::string> output; bool serviceListFresh=false, cleanPreview=false; std::chrono::steady_clock::time_point previewExpires; std::size_t outputPage=0; int pending=0; std::size_t page=0; std::optional<Process> selected;
    void cancel(){backup.cancel();backupPlan.reset();pending=0;planned.reset();cache.cancel();process.cancel();startup.cancel();}
};
LearningContext contextFor(const HealthEvidence& h){if(h.rootBytes&&h.rootBytes->total&&h.rootBytes->available<=h.rootBytes->total&&static_cast<double>(h.rootBytes->available)/h.rootBytes->total<0.15)return LearningContext::Disk;if(h.memory&&h.memory->totalKiB&&h.memory->availableKiB<=h.memory->totalKiB&&static_cast<double>(h.memory->availableKiB)/h.memory->totalKiB<0.15)return LearningContext::Memory;if(!h.memory||!h.rootBytes||!h.memory->totalKiB||h.memory->availableKiB>h.memory->totalKiB||!h.rootBytes->total||h.rootBytes->available>h.rootBytes->total)return LearningContext::Unknown;return LearningContext::Normal;}
std::string learnedAdvice(LearningContext context,LearningAction action){const auto h=readLearningHistory();return h&&actionHistory(*h,context,action).demoted()?" History: repeated poor outcomes; manual review preferred.":"";}
std::string fitText(const Canvas& c,std::string text,int pixels,bool large=false){if(pixels<=0)return {};if(c.textWidth(text,large)<=pixels)return text;if(c.textWidth("...",large)>pixels)return {};while(!text.empty()&&c.textWidth(text+"...",large)>pixels)text.pop_back();return text+"...";}
std::vector<std::string> reviewLines(const Canvas& c,const std::string& message,int width){for(unsigned char ch:message)if(ch<32||ch>=127)return {};std::vector<std::string> result;std::string line;for(char ch:message){if(c.textWidth(line+ch)>width){if(line.empty())return {};result.push_back(line);line.clear();}line+=ch;}if(!line.empty())result.push_back(line);for(const auto& row:result)if(c.textWidth(row)>width)return {};return result;}
void button(Canvas& c,int x,int y,int w,const std::string& label,bool primary=false,bool enabled=true){
    if(w<=0)return;
    enabled=enabled&&!c.blocked;
    c.rounded(x,y,w,30,enabled&&primary?c.accent:c.line,6);
    if(!primary||!enabled)c.rounded(x+1,y+1,w-2,28,enabled?c.card:c.soft,5);
    c.text(x+9,y+20,fitText(c,label,w-18),!enabled?c.disabledInk:primary?c.card:c.ink);
    c.registerButton(x,y,w,30,enabled);
}
std::string safeName(const std::string& s){std::string out;for(unsigned char ch:s){if(out.size()>=32)break;out+=ch>=32&&ch<127?static_cast<char>(ch):'?';}return out;}
GuiPage pageOf(const UI& ui){if(ui.performancePage)return GuiPage::Performance;if(ui.scanPage)return GuiPage::Storage;if(ui.hubPage)return GuiPage::Reports;if(ui.backupPage)return GuiPage::Recovery;if(ui.storagePage)return GuiPage::Storage;if(ui.advancedPage)return GuiPage::Network;if(ui.toolsPage)return GuiPage::SystemTools;if(ui.maintenancePage)return GuiPage::Services;if(ui.startupPage)return GuiPage::Startup;if(ui.tasks)return GuiPage::Processes;return GuiPage::Overview;}
void navIcon(Canvas& c,GuiPage page,int x,int y,unsigned long color){
    const auto line=[&](int a,int b,int d,int e){c.drawLine(x+a,y+b,x+d,y+e,color);};
    const auto box=[&](int a,int b,int w,int h){c.stroke(x+a,y+b,w,h,color);};
    switch(page){
    case GuiPage::Overview:box(1,1,7,7);box(11,1,7,7);box(1,11,7,7);box(11,11,7,7);break;
    case GuiPage::Performance:line(0,10,4,10);line(4,10,7,2);line(7,2,11,17);line(11,17,14,8);line(14,8,18,8);break;
    case GuiPage::Processes:box(0,1,18,16);for(int i=0;i<3;++i){line(3,5+i*4,5,5+i*4);line(8,5+i*4,15,5+i*4);}break;
    case GuiPage::Storage:box(0,4,18,12);line(0,12,17,12);c.rect(x+13,y+13,2,2,color);break;
    case GuiPage::Startup:line(10,0,3,10);line(3,10,10,10);line(10,10,8,19);line(8,19,16,7);line(16,7,9,7);line(9,7,10,0);break;
    case GuiPage::Services:for(int i=0;i<3;++i){line(0,3+i*6,18,3+i*6);box(3+5*(i%2),1+i*6,4,5);}break;
    case GuiPage::SystemTools:line(2,16,11,7);line(4,18,13,9);line(11,7,11,2);line(11,2,15,0);line(15,0,14,5);line(14,5,18,4);line(18,4,18,8);line(18,8,13,9);line(2,16,4,18);break;
    case GuiPage::Network:box(6,0,7,6);box(0,12,7,6);box(12,12,7,6);line(9,6,9,9);line(3,9,15,9);line(3,9,3,12);line(15,9,15,12);break;
    case GuiPage::Recovery:box(1,4,17,13);line(5,4,5,1);line(5,1,14,1);line(14,1,14,4);line(9,7,9,14);line(6,11,9,14);line(9,14,12,11);break;
    case GuiPage::Reports:box(2,0,14,18);for(int i=0;i<3;++i)line(5,5+i*4,13,5+i*4);break;
    }
}
void sidebar(Canvas& c,const GuiLayout& l,int height,const UI& ui){
    if(!l.sidebar)return;
    c.rect(0,0,l.sidebar,height,c.card);c.rect(l.sidebar-1,0,1,height,c.line);
    c.rounded(9,16,34,34,c.accent,9);c.drawLine(14,33,20,33,c.card);c.drawLine(20,33,24,24,c.card);c.drawLine(24,24,29,42,c.card);c.drawLine(29,42,33,31,c.card);c.drawLine(33,31,38,31,c.card);
    if(l.expanded()){c.text(56,31,"SysPulse",c.ink,true);c.text(56,47,"NATIVE EDITION",c.muted);}
    for(std::size_t i=0;i<guiPages.size();++i){const auto y=l.navY(i);const auto& item=guiPages[i];if(l.expanded()&&(i==0||i==4||i==8))c.text(15,y-8,i==0?"WORKSPACE":i==4?"SYSTEM":"TOOLS",c.muted);
        bool active=pageOf(ui)==item.page;if(active){c.rounded(8,y,l.sidebar-16,36,c.selected,7);c.rect(8,y+7,3,22,c.accent);}
        if(l.expanded()){navIcon(c,item.page,18,y+9,active?c.accent:c.muted);c.text(50,y+23,item.label,active?c.accent:c.ink);}else c.text(13,y+23,item.shortLabel,active?c.accent:c.muted,true);
        c.registerButton(8,y,l.sidebar-16,36,!ui.busy()&&!ui.pending);
    }
    if(l.expanded()){c.drawLine(14,height-62,l.sidebar-14,height-62,c.line);c.text(15,height-41,"Local system only",c.ink);c.text(15,height-22,"Automation off",c.muted);}
}
std::string throughput(double rate){if(!std::isfinite(rate)||rate<0)return "Unavailable";char b[48];if(rate>=1048576.0)std::snprintf(b,sizeof b,"%.1f MiB/s",rate/1048576.0);else std::snprintf(b,sizeof b,"%.1f KiB/s",rate/1024.0);return b;}
const DeviceRate* displayDevice(const UI& ui,DeviceKind kind){for(const auto& r:ui.deviceRows)if(r.kind==kind){if(kind==DeviceKind::Disk&&(r.name.rfind("loop",0)==0||r.name.rfind("ram",0)==0||r.name.rfind("dm-",0)==0||r.name.rfind("zram",0)==0))continue;if(kind==DeviceKind::Network&&r.name=="lo")continue;return &r;}return nullptr;}
std::string memoryLabel(const HealthEvidence& health){if(!health.memory||!health.memory->totalKiB||health.memory->availableKiB>health.memory->totalKiB)return "Unavailable";const auto& m=*health.memory;char b[48];std::snprintf(b,sizeof b,"%.1f / %.1f GiB",static_cast<double>(m.totalKiB-m.availableKiB)/1048576.0,static_cast<double>(m.totalKiB)/1048576.0);return b;}
void render(Canvas& c,int width,int height,const std::optional<Rates>& rates,const HealthEvidence& health,const Inspection& inventory,const UI& ui){
    const GuiLayout layout(width);c.origin=0;const auto oldTargets=c.targets;c.targets.clear();c.disabled.clear();c.blocked=ui.pending||ui.busy();
    c.rect(0,0,width,height,c.background);c.rect(0,0,width,4,c.accent);sidebar(c,layout,height,ui);c.origin=layout.sidebar;width=layout.content;
    c.title(24,37,ui.scanPage?"Home storage analysis":guiPages[static_cast<std::size_t>(pageOf(ui))].label);
    c.text(24,58,fitText(c,ui.pending?"Review the exact action before confirming.":"SysPulse / Native system administration",width-48),c.muted);
    if(!ui.scanPage)button(c,width-220,18,196,ui.hubPage?"Back to overview":"Desktop / reports",false,!ui.pending&&!ui.busy());
    c.panel(24,76,width-48,69);c.rect(25,86,3,43,c.accent);
    c.text(40,99,ui.scanPage?"Scoped storage evidence":ui.performancePage?"Live resource observatory":ui.tasks?"Process controls":ui.startupPage?"Startup controls":ui.maintenancePage?"Maintenance workspace":ui.backupPage?"Recovery tools":ui.hubPage?"Local desktop and reports":"System care and maintenance",c.ink,true);const int bw=(width-80-30)/4;
    if(ui.scanPage)c.text(40,128,"Read-only home inventory. Use Back to storage to return.",c.muted);else {
    const bool enabled=!ui.pending&&!ui.busy();
    if(pageOf(ui)==GuiPage::Overview){
        button(c,40,108,bw,width<780?"Analyze health":"Analyze system health",true,enabled);
        button(c,40+bw+10,108,bw,width<780?"Review cache":"Review cache cleanup",false,enabled);
        button(c,40+2*(bw+10),108,bw,"Task manager",false,enabled);
        button(c,40+3*(bw+10),108,bw,"System tools",false,enabled);
    }else{
    button(c,40,108,bw,(ui.backupPage||ui.tasks||ui.startupPage||ui.maintenancePage||ui.hubPage||ui.performancePage)?"Overview":"Analyze cache",!ui.backupPage&&!ui.tasks&&!ui.startupPage&&!ui.maintenancePage&&!ui.hubPage&&!ui.performancePage,enabled);
    button(c,40+bw+10,108,bw,ui.backupPage?"Task manager":ui.storagePage?(width<780?"Services":"Package / services"):ui.advancedPage?(width<780?"Storage":"Storage / recovery"):ui.toolsPage?(width<780?"Network":"Network / power"):ui.maintenancePage?"System tools":ui.startupPage?"Review toggle":ui.tasks?"Review end task":"Task manager",false,enabled&&(!ui.tasks||ui.selected.has_value())&&(!ui.startupPage||ui.startupSelected.has_value()));
    button(c,40+2*(bw+10),108,bw,"Startup / APT",false,enabled);
    button(c,40+3*(bw+10),108,bw,ui.maintenancePage&&!ui.toolsPage&&!ui.advancedPage&&!ui.storagePage?(width<780?"Refresh list":"Refresh services"):"Maintenance",false,enabled);}}
    if(ui.hubPage){
        c.panel(24,153,width-48,390);c.text(38,176,"Desktop tools and local reports",c.ink,true);
        const int mw=(width-100)/3;
        for(std::size_t i=0;i<desktopTools().size();++i){const auto label=std::string(desktopTools()[i].label)+(ui.available[i]?"":" (missing)");button(c,38+static_cast<int>(i%3)*(mw+8),188+static_cast<int>(i/3)*37,mw,label,false,ui.available[i]);}
        button(c,38,344,mw,"Refresh health report");button(c,46+mw,344,mw,"Save report to home",true);button(c,54+2*mw,344,mw,"Overview");
        button(c,38,380,190,"Local learning history");c.text(240,399,fitText(c,"External tools manage their own actions.",width-270),c.muted);
        c.text(38,416,"Reports exclude hostnames, process names, addresses and raw logs.",c.muted);
        std::istringstream report(ui.report);std::string row;unsigned index=0;
        while(std::getline(report,row)){if(index>=ui.outputPage*6&&index<ui.outputPage*6+6)c.text(38,438+static_cast<int>(index-ui.outputPage*6)*16,fitText(c,row,width-76),c.ink);++index;}
        c.text(width-200,535,"PgUp / PgDn: report",c.muted);
    }else if(ui.backupPage){
        c.panel(24,153,width-48,390);c.text(38,178,"Verified file backup and recovery",c.ink,true);
        c.text(38,202,"Copy a user-owned file to a NEW path. Restore by copying the backup to a new name.",c.muted);
        c.text(38,224,"Up to 64 MiB; no overwrite, no links, no root; existing owned destination directory.",c.muted);
        auto field=[&](int y,const char* title,const std::string& value,int i){c.text(38,y,title,c.ink);c.rect(38,y+10,width-76,31,ui.backupField==i?c.line:c.background);c.text(46,y+30,fitText(c,value,width-92),c.ink);};
        field(252,"Source absolute path",ui.backupSource,0);field(322,"New destination absolute path",ui.backupDestination,1);
        button(c,38,380,180,"Review copy",true);button(c,230,380,150,"Clear paths");
        c.text(38,438,"Type paths (Tab selects a field). Full reviewed paths must fit the view.",c.muted);
        c.text(38,459,"Destination is private (0600); metadata, directory trees and snapshots are not copied.",c.muted);
        if(!ui.backupDigest.empty())c.text(38,490,"SHA-256: "+ui.backupDigest,c.green);
    }else if(ui.scanPage){
        c.panel(24,153,width-48,390);c.text(38,176,"Home storage analysis",c.ink,true);
        button(c,38,188,160,"Refresh scan",true);button(c,208,188,160,"Back to storage");
        c.text(38,240,"Local report; hardlinks counted once, other mounts excluded.",c.muted);
        for(std::size_t i=0;i<15&&ui.outputPage*15+i<ui.scanRows.size();++i)c.text(38,260+static_cast<int>(i)*16,fitText(c,ui.scanRows[ui.outputPage*15+i],width-76),c.ink);
        button(c,38,508,100,"Previous");button(c,150,508,100,"Next");
    }else if(ui.storagePage){
        c.panel(24,153,width-48,390);c.text(38,176,"Offline ext filesystem recovery",c.ink,true);
        const int mw=(width-100)/3;
        button(c,38,188,mw,"Refresh devices");button(c,46+mw,188,mw,"Check selected",false,ui.offlineSelected.has_value());button(c,54+2*mw,188,mw,"Repair selected",false,ui.offlineSelected&&ui.offlineChecked&&*ui.offlineSelected==*ui.offlineChecked&&std::chrono::steady_clock::now()-ui.offlineCheckedAt<=std::chrono::minutes(5));
        c.text(38,239,"Only unmounted ext2/3/4 with unique UUID and device identity.",c.muted);
        for(std::size_t i=0;i<5&&ui.page*5+i<ui.offlineList.size();++i){const auto& v=ui.offlineList[ui.page*5+i];int y=250+static_cast<int>(i)*25;if(ui.offlineSelected&&*ui.offlineSelected==v)c.rounded(32,y,width-64,25,c.selected,4);c.text(38,y+16,fitText(c,v.path+" | "+v.type+" | "+v.size+" | "+v.majorMinor,width-76),c.ink);c.registerButton(32,y,width-64,25,!c.blocked);}
        button(c,38,385,100,"Previous");button(c,150,385,100,"Next");c.text(262,404,"Page "+std::to_string(ui.page+1),c.muted);
        if(ui.offlineSelected)c.text(38,433,"UUID: "+ui.offlineSelected->uuid,c.ink);
        c.text(38,454,"Repair may change or discard damaged data; back up before repair.",c.accent);
        c.text(38,473,"UDisks performs authorization and rejects mounted filesystems.",c.muted);
        button(c,width-370,508,155,"Scan home storage");button(c,width-205,508,161,"Review failed cache");
        c.text(38,493,fitText(c,ui.offlineChecked?"Inconsistency reported. Review repair only after backup.":"A current failed consistency check is required before repair.",width-76),c.muted);
    }else if(ui.advancedPage){
        c.panel(24,153,width-48,390);c.text(38,176,"Network, power and diagnostic evidence",c.ink,true);
        const int mw=(width-100)/3;
        const char* labels[]={"Network addresses","Routing table","Listening sockets","Wi-Fi status","Enable Wi-Fi","Disable Wi-Fi","Power profile","Balanced profile","Power saver","Performance profile","System errors","Analyze journal","Firewall status","Scheduled timers","Storage / recovery"};
        for(int i=0;i<15;++i)button(c,38+(i%3)*(mw+8),188+(i/3)*37,mw,labels[i]);
        c.text(38,385,"Wi-Fi and power changes require confirmation; system policy applies.",c.muted);
        c.text(38,402,"Logs and addresses remain local. Missing utilities are reported.",c.muted);
        c.text(38,419,"Firewall inspection only; no automatic security policy changes.",c.muted);
        c.text(38,442,"Last command output (bounded; PgUp/PgDn): "+std::to_string(ui.outputPage+1),c.ink,true);
        for(std::size_t i=0;i<5&&ui.outputPage*5+i<ui.output.size();++i)c.text(38,460+static_cast<int>(i)*14,fitText(c,ui.output[ui.outputPage*5+i],width-76),c.muted);
    }else if(ui.toolsPage){
        c.panel(24,153,width-48,390);c.text(38,176,"System repair, storage and memory",c.ink,true);
        const int mw=(width-100)/3;
        const char* labels[]={"Check dependencies","Configure pending","Preview upgrades","Disk inventory","Preview root TRIM","TRIM root filesystem","Failed system services","Enable zswap","Disable zswap","Reclaim own RAM","Refresh evidence","Apply upgrades","Preview repair","Apply repair","Network / power"};
        for(int i=0;i<15;++i){const auto now=std::chrono::steady_clock::now();const bool enabled=i==5?ui.trimPreview&&now<=ui.trimExpires:i==11?ui.packagePreview==1&&now<=ui.packageExpires:i==13?ui.packagePreview==2&&now<=ui.packageExpires:true;button(c,38+(i%3)*(mw+8),188+(i/3)*37,mw,labels[i],false,enabled);}
        c.text(38,385,std::string("Runtime zswap: ")+(ui.compression.state==ZswapState::Enabled?"enabled":ui.compression.state==ZswapState::Disabled?"disabled":"unknown"),c.ink);
        c.text(38,402,"Changes require review. Root TRIM only; no filesystem repair.",c.muted);
        c.text(38,419,"APT rechecks the reviewed plan; no removals. Transaction timing can still change.",c.muted);
        c.text(38,442,"Last command output (bounded; PgUp/PgDn): "+std::to_string(ui.outputPage+1),c.ink,true);
        for(std::size_t i=0;i<5&&ui.outputPage*5+i<ui.output.size();++i)c.text(38,460+static_cast<int>(i)*14,fitText(c,ui.output[ui.outputPage*5+i],width-76),c.muted);
    }else if(ui.maintenancePage){
        c.panel(24,153,width-48,390);c.text(38,176,ui.systemServices?"Package maintenance / system services":"Package maintenance / user services",c.ink,true);
        const int mw=(width-100)/3;
        button(c,38,188,mw,"Package audit");button(c,46+mw,188,mw,"Preview autoclean");button(c,54+2*mw,188,mw,"Refresh APT lists");
        const bool serviceReady=ui.serviceListFresh&&ui.selectedService&&std::chrono::steady_clock::now()<=ui.serviceExpires;button(c,38,225,mw,"Clean obsolete cache",false,ui.cleanPreview&&std::chrono::steady_clock::now()<=ui.previewExpires);button(c,46+mw,225,mw,ui.systemServices?"Start system service":"Start user service",false,serviceReady);button(c,54+2*mw,225,mw,ui.systemServices?"Stop system service":"Stop user service",false,serviceReady);
        c.text(38,274,ui.systemServices?"System services: select a row; admin authorization required":"User services: select a row (refresh to reload)",c.muted);
        for(std::size_t i=0;i<5&&ui.page*5+i<ui.services.size();++i){const auto& v=ui.services[ui.page*5+i];int y=282+static_cast<int>(i)*22;if(ui.selectedService&&ui.selectedService->name==v.name)c.rounded(32,y,width-64,22,c.selected,4);c.text(38,y+15,fitText(c,v.name,width-(width>=760?210:76)),c.ink);if(width>=760)c.text(width-150,y+15,fitText(c,v.state,112),c.muted);c.registerButton(32,y,width-64,22,!c.blocked);}
        button(c,38,395,100,"Previous");button(c,150,395,100,"Next");button(c,262,395,128,ui.systemServices?"User services":"System services");button(c,402,395,112,"Restart service",false,serviceReady);c.text(524,414,"Page "+std::to_string(ui.page+1),c.muted);
        c.text(38,442,"Last command output (bounded; PgUp/PgDn): "+std::to_string(ui.outputPage+1),c.ink,true);
        for(std::size_t i=0;i<5&&ui.outputPage*5+i<ui.output.size();++i)c.text(38,460+static_cast<int>(i)*14,fitText(c,ui.output[ui.outputPage*5+i],width-76),c.muted);
        if(height>=700){c.panel(24,555,width-48,height-635);c.text(38,580,"Selected service",c.ink,true);if(ui.selectedService){c.text(38,604,fitText(c,ui.selectedService->name+"  |  "+ui.selectedService->state,width-76),c.ink);if(height>=730)c.text(38,627,ui.systemServices?"System scope: desktop authorization is required.":"User session scope: reviewed start, stop and restart.",c.muted);}else c.text(38,604,"Select a current service to inspect and review its actions.",c.muted);}
    }else if(ui.startupPage){
        c.panel(24,153,width-48,360);c.text(38,176,"User startup files / reversible local toggle",c.ink,true);
        c.text(38,196,"Package database: "+(ui.packages.complete?std::to_string(ui.packages.installed)+" installed / "+std::to_string(ui.packages.incomplete)+" incomplete":"unavailable"),c.muted);
        for(std::size_t i=0;i<10&&ui.page*10+i<ui.startupList.entries.size();++i){const auto& row=ui.startupList.entries[ui.page*10+i];int y=208+static_cast<int>(i)*22;if(ui.startupSelected&&ui.startupSelected->name==row.name)c.rounded(30,y,width-60,22,c.selected,4);c.text(38,y+15,fitText(c,row.name,248),c.ink);c.text(300,y+15,row.disabled?"Moved aside":"Present",c.muted);c.registerButton(30,y,width-60,22,!c.blocked);}
        button(c,40,437,100,"Previous");button(c,150,437,100,"Next");c.text(270,456,"Page "+std::to_string(ui.page+1),c.muted);
        c.text(38,486,"Select a file, then Review toggle. System entries unchanged.",c.muted);
    }else if(ui.tasks){
        c.panel(24,153,width-48,390);
        const char* sorts[]={"PID","CPU","Memory","Name"};button(c,38,164,122,std::string("Sort: ")+sorts[static_cast<unsigned>(ui.taskSort)]);button(c,170,164,100,"Search",ui.taskSearch);c.text(280,183,fitText(c,ui.taskQuery.empty()?"All processes":ui.taskQuery,width-325),c.ink);
        const int processWidth=std::max(110,width-545),stateX=110+processWidth,parentX=stateX+62,cpuX=parentX+78,rssX=cpuX+95;
        c.rounded(30,199,width-60,25,c.soft,4);c.text(38,217,"PID",c.muted);c.text(105,217,"PROCESS",c.muted);c.text(stateX,217,"STATE",c.muted);c.text(parentX,217,"PARENT",c.muted);c.text(cpuX,217,"CPU %*",c.muted);c.text(rssX,217,"RSS MiB",c.muted);
        for(std::size_t i=0;i<10&&ui.page*10+i<ui.taskRows.size();++i){const auto& entry=ui.taskRows[ui.page*10+i];const auto& row=entry.process;int y=226+static_cast<int>(i)*22;if(i%2)c.rect(30,y,width-60,22,c.soft);if(ui.selected&&ui.selected->pid==row.pid&&ui.selected->startTicks==row.startTicks)c.rounded(30,y,width-60,22,c.selected,4);c.text(38,y+15,std::to_string(row.pid),c.ink);c.text(105,y+15,fitText(c,safeName(row.name),processWidth-12),c.ink);c.text(stateX,y+15,std::string(1,row.state),c.ink);c.text(parentX,y+15,std::to_string(row.parent),c.ink);c.text(cpuX,y+15,fitText(c,percent(entry.cpuPercent),90),c.ink);char rss[32];if(entry.rssBytes)std::snprintf(rss,sizeof rss,"%.1f",static_cast<double>(*entry.rssBytes)/1048576.0);else std::snprintf(rss,sizeof rss,"unknown");c.text(rssX,y+15,rss,c.ink);c.registerButton(30,y,width-60,22,!c.blocked);}
        button(c,40,457,100,"Previous");button(c,150,457,100,"Next");c.text(270,476,"Page "+std::to_string(ui.page+1)+" | "+std::to_string(ui.taskRows.size())+" rows"+(ui.taskModel.partial?" (partial)":""),c.muted);
        c.text(38,508,fitText(c,"*CPU: percent of one logical CPU. Select process, then Review end task.",width-76),c.muted);
        if(height>=700){c.panel(24,555,width-48,height-635);c.text(38,580,"Selected process",c.ink,true);if(ui.selected){c.text(38,604,fitText(c,"PID "+std::to_string(ui.selected->pid)+"  |  "+safeName(ui.selected->name)+"  |  Parent "+std::to_string(ui.selected->parent)+"  |  State "+std::string(1,ui.selected->state),width-76),c.ink);if(height>=730)c.text(38,627,"End task requests graceful termination only after identity review.",c.muted);}else c.text(38,604,"Select a process to inspect its identity and review an action.",c.muted);}
    }else{
    const int gap=12,columns=metricColumns(width),cardWidth=(width-48-(columns-1)*gap)/columns;
    const auto* disk=displayDevice(ui,DeviceKind::Disk);const auto* net=displayDevice(ui,DeviceKind::Network);
    const std::string labels[]={"CPU usage","Memory used",disk?"Disk / "+disk->name:"Disk activity",net?"Network / "+net->name:"Network activity"};
    const std::string values[]={percent(rates?rates->cpu:std::nullopt),memoryLabel(health),disk?throughput(disk->readBytesPerSecond+disk->writeBytesPerSecond):"Unavailable",net?throughput(net->readBytesPerSecond+net->writeBytesPerSecond):"Unavailable"};
    for(int i=0;i<4;++i){const int x=24+i%columns*(cardWidth+gap),y=153+i/columns*88;c.panel(x,y,cardWidth,76);const unsigned long tone=i==0?c.green:i==1?c.accent:i==2?c.blue:c.purple;c.rect(x+12,y+11,3,14,tone);c.text(x+22,y+23,fitText(c,labels[i],cardWidth-30),c.muted);c.text(x+12,y+55,fitText(c,values[i],cardWidth-24,true),tone,true);}
    const int top=metricsBottom(width);
    if(ui.performancePage){
        c.panel(24,top,width-48,height-top-80);c.text(38,top+26,"Live per-core CPU and device rates",c.ink,true);
        const int inner=width-76,half=(inner-24)/2,rx=38+half+24,rows=performanceRows(width,height);
        c.text(38,top+50,"LOGICAL CORES",c.muted);c.text(rx,top+50,"DEVICE / READ + WRITE",c.muted);
        if(rates){std::size_t n=0;for(const auto& item:rates->cores){if(n>=ui.page*static_cast<std::size_t>(rows)&&n<(ui.page+1)*static_cast<std::size_t>(rows)){const int y=top+64+static_cast<int>(n-ui.page*rows)*27;const int bar=std::max(20,half-125);c.text(38,y+15,fitText(c,item.first,48),c.ink);c.rounded(90,y+5,bar,10,c.soft,5);c.rounded(90,y+5,static_cast<int>(bar*std::clamp(item.second,0.0,100.0)/100.0),10,c.green,5);c.text(94+bar,y+15,percent(item.second),c.ink);}++n;}}else c.text(38,top+79,"Waiting for valid samples",c.muted);
        for(int i=0;i<rows&&ui.page*static_cast<std::size_t>(rows)+static_cast<std::size_t>(i)<ui.deviceRows.size();++i){const auto& r=ui.deviceRows[ui.page*rows+i];const int y=top+64+i*27;c.text(rx,y+15,fitText(c,r.name,half/2-8),c.ink);c.text(rx+half/2,y+15,fitText(c,throughput(r.readBytesPerSecond+r.writeBytesPerSecond),half/2-4),r.kind==DeviceKind::Disk?c.blue:c.purple);}
        if(ui.deviceRows.empty())c.text(rx,top+79,fitText(c,"Waiting / unavailable",half),c.muted);
        button(c,38,height-135,100,"Previous",false,ui.page>0);const auto available=std::max(rates?rates->cores.size():0,ui.deviceRows.size());button(c,150,height-135,100,"Next",false,(ui.page+1)*static_cast<std::size_t>(rows)<available);c.text(262,height-115,"Page "+std::to_string(ui.page+1),c.muted);
        c.text(38,height-88,fitText(c,"Individual devices; loop/RAM hidden. No aggregate across stacked disks.",width-76),c.muted);
    }else{
    c.panel(24,top,width-48,150);c.text(40,top+24,"Health findings",c.ink,true);c.text(40,top+139,fitText(c,"Root: "+capacity(health.rootBytes)+"  |  "+std::to_string(inventory.processes.size())+" readable process records",width-80),c.muted);
    const char* names[]={"Memory","Root storage","Swap"};int i=0;
    for(auto f:assessHealth(health)){const int y=top+50+i*30;c.text(40,y,names[i],c.ink);c.text(154,y,severity(f.severity),f.severity==triage::Severity::Ok?c.green:c.accent);c.text(252,y,fitText(c,f.advice,width-290),c.muted);++i;}
    const int chartTop=top+158;const int chartHeight=height-76-chartTop;
    if(chartHeight>=48){const int half=(width-60)/2;c.panel(24,chartTop,width-48,chartHeight);
        auto graph=[&](int x,int w,const char* title,bool cpu){c.text(x+8,chartTop+15,fitText(c,std::string(title)+" | "+std::to_string(ui.history.rows().size())+" samples",w-16),c.muted);if(chartHeight<48)return;const auto& rows=ui.history.rows();const int y0=chartTop+28,h=chartHeight-42;for(int line=0;line<=4;++line){c.drawLine(x+44,y0+line*h/4,x+w-12,y0+line*h/4,c.line);if(chartHeight>=120)c.text(x+8,y0+line*h/4+4,std::to_string(100-25*line),c.muted);}int px=0,py=0;bool previous=false;for(std::size_t i=0;i<rows.size();++i){const auto v=cpu?rows[i].cpu:rows[i].memoryUsed;if(!v||!std::isfinite(*v)||*v<0||*v>100){previous=false;continue;}int xx=x+44+static_cast<int>(rows.size()==1?1:i)*std::max(1,w-56)/std::max(1,static_cast<int>(rows.size())-1),yy=y0+h-static_cast<int>(*v*h/100.0);if(previous)c.drawLine(px,py,xx,yy,cpu?c.green:c.accent);else c.rect(xx,yy,2,2,cpu?c.green:c.accent);px=xx;py=yy;previous=true;}};
        graph(24,half,"CPU history (0-100%)",true);graph(36+half,half,"Memory history (0-100%)",false);
    }
    button(c,width-204,top+4,160,"Backup / restore");
    }
    }
    c.panel(24,height-68,width-48,60);
    if(!ui.pending){c.text(36,height-46,fitText(c,ui.message,width-72),c.ink);c.text(36,height-20,ui.busy()?"Operation running. Keep this window open.":"Ready | F1-F10: modules   Tab: controls   Esc: close",c.muted);}
    else {
        c.targets.clear();c.disabled.clear();c.blocked=false;
        const int modalY=height-208;const auto lines=reviewLines(c,ui.message,width-72);
        c.rounded(24,modalY,width-48,200,c.accent,10);c.rounded(26,modalY+2,width-52,196,c.card,9);
        c.text(36,modalY+29,"Review required",c.accent,true);
        if(!lines.empty()&&lines.size()<=6){for(std::size_t i=0;i<lines.size();++i)c.text(36,modalY+52+static_cast<int>(i)*18,lines[i],c.ink);}
        else c.text(36,modalY+55,"Review does not fit. Widen the window or cancel.",c.accent);
        button(c,36,height-39,230,ui.pending==1?"Confirm cache cleanup":ui.pending==3?"Confirm startup toggle":ui.pending==4?"Confirm maintenance":ui.pending==5?"Confirm filesystem action":ui.pending==6?"Confirm verified copy":"Confirm SIGTERM",true,!lines.empty()&&lines.size()<=6);
        button(c,280,height-39,110,"Cancel");
    }
    c.origin=0;
    if(oldTargets!=c.targets)c.focus=-1;
    if(c.focus>=0&&static_cast<std::size_t>(c.focus)<c.targets.size()){const auto& r=c.targets[static_cast<std::size_t>(c.focus)];c.stroke(r.x-2,r.y-2,r.w+4,r.h+4,c.blue);}
    c.frameReady=true;
    XFlush(c.d);
}
}
int main(int argc,char** argv){
    const bool smoke=argc==2&&std::strcmp(argv[1],"--smoke")==0;
    if(argc>1&&!smoke)return 2;
    Display* d=XOpenDisplay(nullptr);if(!d){std::cerr<<"X11 display unavailable\n";return 1;}
    const int s=DefaultScreen(d);int width=std::max(700,std::min(1366,DisplayWidth(d,s)-40)),height=std::max(620,std::min(760,DisplayHeight(d,s)-60));
    Window w=XCreateSimpleWindow(d,RootWindow(d,s),30,30,width,height,0,BlackPixel(d,s),WhitePixel(d,s));
    XStoreName(d,w,"SysPulse Native - Linux Administration Build");XSizeHints hints{};hints.flags=PMinSize;hints.min_width=700;hints.min_height=620;XSetWMNormalHints(d,w,&hints);
    XSelectInput(d,w,ExposureMask|StructureNotifyMask|KeyPressMask|ButtonPressMask);Atom close=XInternAtom(d,"WM_DELETE_WINDOW",False);XSetWMProtocols(d,w,&close,1);
    GC gc=XCreateGC(d,w,0,nullptr);auto* normal=XLoadQueryFont(d,"-*-helvetica-medium-r-normal--14-*-*-*-*-*-iso8859-1");if(!normal)normal=XLoadQueryFont(d,"fixed");auto* heading=XLoadQueryFont(d,"-*-helvetica-bold-r-normal--18-*-*-*-*-*-iso8859-1");if(!heading)heading=XLoadQueryFont(d,"9x15bold");
    Canvas c{d,w,gc,s,normal,heading};c.titleFont=XLoadQueryFont(d,"-*-helvetica-bold-r-normal--24-*-*-*-*-*-iso8859-1");
    c.background=c.color("#f7f8fa");c.card=c.color("#ffffff");c.ink=c.color("#202124");c.muted=c.color("#667085");c.accent=c.color("#c8102e");c.line=c.color("#d5dae3");c.green=c.color("#147d43");c.soft=c.color("#f3f5f8");c.selected=c.color("#fbe9ed");c.blue=c.color("#2469bf");c.purple=c.color("#8244ba");c.disabledInk=c.color("#747b86");
    Pixmap buffer=0;int bufferWidth=0,bufferHeight=0;
    XMapWindow(d,w);XSync(d,False);
    auto previous=captureSample();auto health=collectHealth();auto inventory=inspect(256);std::optional<Rates> rates; UI ui;
    auto lastTask=Sample::Clock::now(),lastDevice=Sample::Clock::now();const auto positive=[](long n){return n>0?static_cast<std::uint64_t>(n):0;};
    auto refreshTasks=[&](bool reset){const auto now=Sample::Clock::now();auto current=inspect(1024);const Inspection empty;ui.taskModel=buildTaskView(reset?empty:inventory,current,std::chrono::duration<double>(now-lastTask).count(),positive(sysconf(_SC_CLK_TCK)),positive(sysconf(_SC_PAGESIZE)));inventory=std::move(current);lastTask=now;ui.selected=retainTaskSelection(ui.taskModel,ui.selected);ui.taskRows=filterTasks(ui.taskModel,ui.taskQuery,ui.taskSort);if(ui.page*10>=ui.taskRows.size())ui.page=0;};
    bool running=true,mapped=true,dirty=true;int draws=0;bool narrowRequested=false;auto next=Sample::Clock::now()+std::chrono::seconds(1);const auto deadline=Sample::Clock::now()+std::chrono::seconds(8);
    auto launch=[&](Operation op,const std::string& unit=std::string{}){auto cmd=commandFor(op,unit);ui.operation=op;ui.output.clear();ui.outputPage=0;ui.cleanPreview=false;ui.trimPreview=false;ui.packagePreview=0;if(!cmd||!ui.job.start(*cmd)){ui.message="Action unavailable or refused (required OS utility missing).";return false;}ui.message="Operation running. Wait for the result.";return true;};
    auto review=[&](Operation op,const std::string& unit=std::string{}){ui.cancel();auto cmd=commandFor(op,(op==Operation::ApplyUpgrade||op==Operation::ApplyRepair)?ui.packageToken:unit);if(op!=Operation::Clean)ui.cleanPreview=false;
    const int required=op==Operation::ApplyUpgrade?1:op==Operation::ApplyRepair?2:0;
    if(!required)ui.packagePreview=0;
    if(required&&(ui.packagePreview!=required||std::chrono::steady_clock::now()>ui.packageExpires)){ui.packagePreview=0;ui.message="Run the matching successful package simulation first.";return;}if(op!=Operation::TrimRoot)ui.trimPreview=false;if(op==Operation::TrimRoot&&(!ui.trimPreview||std::chrono::steady_clock::now()>ui.trimExpires)){ui.message="Run a successful root TRIM preview first.";return;}if((op==Operation::EnableCompression||op==Operation::DisableCompression)&&!compressionChangeAllowed(compressionEvidence(),op==Operation::EnableCompression)){ui.message="Current swap/compression evidence is ineligible or unavailable.";return;}if(op==Operation::Clean&&(!ui.cleanPreview||std::chrono::steady_clock::now()>ui.previewExpires)){ui.message="Run Preview autoclean successfully before cleaning.";return;}if(unit.size()>40){ui.message="Service name too long for exact confirmation; refused.";return;}if(!cmd){ui.message="Action refused by scope or protected-service policy.";return;}if(getuid()==0||getuid()!=geteuid()){ui.message="Use an ordinary desktop account. Review refused.";return;}ui.operation=op;ui.planned=std::move(cmd);ui.expires=std::chrono::steady_clock::now()+std::chrono::seconds(30);ui.pending=4;ui.message=op==Operation::CheckPackages?"Check package dependencies with administrator authorization?":op==Operation::ConfigurePackages?"Configure pending packages? May restart services; save work.":op==Operation::PreviewUpgrade?"Simulate package upgrades only? No installation.":op==Operation::ApplyUpgrade?"Upgrade installed packages (no removals)? Rechecks plan; save work.":op==Operation::PreviewRepair?"Simulate dependency repair with no removals?":op==Operation::ApplyRepair?"Repair dependencies (may install; no removals)? Rechecks plan; save work.":op==Operation::PreviewTrim?"Authorize root filesystem TRIM dry-run?":op==Operation::TrimRoot?"Discard unused blocks on root filesystem only?":op==Operation::EnableCompression?"Enable runtime zswap? Uses RAM and CPU for compression.":op==Operation::DisableCompression?"Disable new runtime zswap compression?":op==Operation::PreviewClean?"Authorize an APT cleanup simulation only (no changes)?":op==Operation::Refresh?"Refresh APT package lists with administrator authorization?":op==Operation::Clean?"Delete obsolete downloaded APT archives? This cannot be undone." :op==Operation::WifiEnable?"Enable Wi-Fi radio?":op==Operation::WifiDisable?"Disable Wi-Fi? This may disconnect your network.":op==Operation::PowerBalanced?"Set the balanced power profile?":op==Operation::PowerSaver?"Set the power-saver profile?":op==Operation::PowerPerformance?"Set performance profile? Battery use and heat may increase.":op==Operation::FirewallStatus?"Authorize read-only firewall status inspection?":(op==Operation::StartSystemService?"Start system service ":op==Operation::StopSystemService?"Stop system service (may interrupt apps) ":op==Operation::RestartSystemService?"Restart system service (may interrupt apps) ":op==Operation::RestartUserService?"Restart user service (may interrupt apps) ":op==Operation::StartService?"Start user service ":"Stop user service (may interrupt apps) ")+unit+"?";};
    auto navigate=[&](GuiPage page){
        ui.cancel();ui.hubPage=ui.backupPage=ui.scanPage=ui.performancePage=ui.tasks=ui.startupPage=ui.maintenancePage=ui.toolsPage=ui.advancedPage=ui.storagePage=false;
        ui.offlineTarget.reset();ui.offlineChecked.reset();ui.offlineSelected.reset();ui.packagePreview=0;ui.cleanPreview=ui.trimPreview=false;ui.serviceListFresh=false;ui.selectedService.reset();ui.selected.reset();ui.startupSelected.reset();ui.page=ui.outputPage=0;ui.taskSearch=false;ui.output.clear();c.focus=-1;
        switch(page){
        case GuiPage::Overview:ui.message="Overview. Choose a reviewed action.";break;
        case GuiPage::Performance:ui.performancePage=true;ui.message="Live resource samples; individual disk/interface rates.";break;
        case GuiPage::Processes:ui.tasks=true;ui.taskQuery.clear();refreshTasks(true);ui.message="Select a process to review.";break;
        case GuiPage::Storage:ui.maintenancePage=ui.storagePage=true;ui.offlineList.clear();launch(Operation::OfflineInventory);break;
        case GuiPage::Startup:ui.startupPage=true;ui.startupList=ui.startup.scan();ui.packages=auditPackages();ui.message=ui.startupList.status;break;
        case GuiPage::Services:ui.maintenancePage=true;ui.services.clear();launch(ui.systemServices?Operation::ListSystemServices:Operation::ListServices);break;
        case GuiPage::SystemTools:ui.maintenancePage=ui.toolsPage=true;ui.compression=compressionEvidence();ui.message="System tools: select a reviewed action.";break;
        case GuiPage::Network:ui.maintenancePage=ui.advancedPage=true;ui.message="Network, power and diagnostics.";break;
        case GuiPage::Recovery:ui.backupPage=true;ui.message="Enter the source and a new destination path.";break;
        case GuiPage::Reports:ui.hubPage=true;for(std::size_t i=0;i<ui.available.size();++i)ui.available[i]=desktopAvailable(i);ui.report=healthReport(collectHealth(),collectPressure(),inspect(256));ui.message="Desktop tools and local report. Nothing runs automatically.";break;
        }
    };
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
        while(XPending(d)){XEvent ev;XNextEvent(d,&ev);switch(ev.type){case Expose:dirty=true;break;case ConfigureNotify:width=ev.xconfigure.width;height=ev.xconfigure.height;c.focus=-1;c.frameReady=false;c.targets.clear();c.disabled.clear();if(ui.pending){ui.cancel();ui.message="Review cancelled after resize. Review the action again.";}dirty=true;break;case UnmapNotify:mapped=false;c.frameReady=false;c.focus=-1;ui.deviceBaseline.reset();ui.deviceRows.clear();if(!ui.copying.valid())ui.cancel();ui.serviceListFresh=false;ui.selectedService.reset();ui.offlineTarget.reset();ui.offlineChecked.reset();ui.offlineRevalidate=false;ui.packagePreview=0;ui.trimPreview=false;ui.cleanPreview=false;if(!ui.copying.valid())ui.message="Review cancelled when hidden.";break;case MapNotify:mapped=true;previous=captureSample();rates.reset();ui.history.push({});next=Sample::Clock::now()+std::chrono::seconds(1);dirty=true;break;case ClientMessage:if(static_cast<Atom>(ev.xclient.data.l[0])==close&&!ui.busy())running=false;break;case ButtonPress:{
            const GuiLayout layout(width);const int realX=ev.xbutton.x,y=ev.xbutton.y,x=realX-layout.sidebar,width=layout.content,bw=(width-80-30)/4;
            if(ev.xbutton.button!=Button1||ui.busy()||!c.frameReady)break;
            c.focus=-1;
            if(ui.pending&&(y<height-39||y>=height-9)){dirty=true;break;}
            if(std::any_of(c.disabled.begin(),c.disabled.end(),[&](const auto& r){return r.contains(realX,y);})){ui.message=ui.pending?ui.message:"Control unavailable until its prerequisite is satisfied.";dirty=true;break;}
            if(realX<layout.sidebar){if(!ui.pending){if(auto page=layout.navHit(realX,y)){c.frameReady=false;navigate(*page);}}dirty=true;break;}
            c.frameReady=false;dirty=true;
            if(ui.scanPage&&!ui.pending){if(y>=188&&y<218&&x>=38&&x<198){ui.scanRows.clear();ui.outputPage=0;ui.scanning=std::async(std::launch::async,[]{std::atomic_bool cancel{false};return scanAccountHome({},cancel);});ui.message="Scanning home storage...";}else if(y>=188&&y<218&&x>=208&&x<368){ui.scanPage=false;ui.message="Storage recovery.";}else if(y>=508&&y<538&&x>=38&&x<138&&ui.outputPage)--ui.outputPage;else if(y>=508&&y<538&&x>=150&&x<250&&(ui.outputPage+1)*15<ui.scanRows.size())++ui.outputPage;dirty=true;break;}
            if(y>=height-39&&y<height-9&&ui.pending){
                if(x>=36&&x<266){const auto lines=reviewLines(c,ui.message,width-72);if(lines.empty()||lines.size()>6){dirty=true;break;}if(ui.pending==1){auto result=ui.cache.apply();const auto status=!result.completed||result.skipped?LearningResult::Failure:result.removed?LearningResult::Applied:LearningResult::NoEffect;const bool recorded=rememberOutcome({ui.cacheContext,ui.cacheAction,status,status==LearningResult::Applied?result.bytes:0,result.availableBefore,result.availableAfter});ui.message="Removed "+std::to_string(result.removed)+", skipped "+std::to_string(result.skipped)+", "+std::to_string(result.bytes)+" logical bytes. "+result.status+(recorded?"":" History not saved.");}
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
            if(ui.pending)break;
            if(y>=18&&y<48&&x>=width-220&&x<width-24){ui.cancel();ui.hubPage=!ui.hubPage;ui.performancePage=ui.backupPage=ui.tasks=ui.startupPage=ui.maintenancePage=ui.toolsPage=ui.advancedPage=ui.storagePage=false;ui.packagePreview=0;ui.cleanPreview=ui.trimPreview=false;ui.offlineChecked.reset();ui.offlineTarget.reset();ui.serviceListFresh=false;ui.outputPage=0;
                if(ui.hubPage){for(std::size_t i=0;i<ui.available.size();++i)ui.available[i]=desktopAvailable(i);ui.report=healthReport(collectHealth(),collectPressure(),inspect(256));ui.message="Desktop tools and local report. Nothing runs automatically.";}else ui.message="Overview.";dirty=true;break;}
            if(y>=108&&y<138){int which=-1;for(int i=0;i<4;++i)if(x>=40+i*(bw+10)&&x<40+i*(bw+10)+bw)which=i;if(which<0)break;if(pageOf(ui)==GuiPage::Overview){
                    if(which==0){navigate(GuiPage::Reports);ui.message="System health evidence refreshed. Review the local report.";}
                    else if(which==1){ui.cancel();ui.cacheAction=LearningAction::Thumbnails;ui.cacheContext=contextFor(health);ui.summary=ui.cache.scan();ui.pending=ui.summary.files?1:0;ui.message=std::to_string(ui.summary.files)+" files / "+std::to_string(ui.summary.bytes)+" bytes. "+ui.summary.status+learnedAdvice(ui.cacheContext,ui.cacheAction);}
                    else if(which==2){navigate(GuiPage::Processes);}else{navigate(GuiPage::SystemTools);}
                    dirty=true;break;}
                const bool wasHub=ui.hubPage||ui.performancePage;ui.hubPage=ui.performancePage=false;ui.cancel();ui.offlineTarget.reset();ui.offlineChecked.reset();ui.packagePreview=0;
                if(which==0){if(wasHub){navigate(GuiPage::Overview);}else if(ui.backupPage){ui.backupPage=false;ui.message="Overview.";}else if(ui.tasks||ui.startupPage||ui.maintenancePage){ui.tasks=false;ui.startupPage=false;ui.maintenancePage=false;ui.toolsPage=false;ui.advancedPage=false;ui.storagePage=false;ui.trimPreview=false;ui.message="Overview.";}else{ui.cacheAction=LearningAction::Thumbnails;ui.cacheContext=contextFor(health);ui.summary=ui.cache.scan();ui.pending=ui.summary.files?1:0;ui.message=std::to_string(ui.summary.files)+" files / "+std::to_string(ui.summary.bytes)+" bytes. "+ui.summary.status+learnedAdvice(ui.cacheContext,ui.cacheAction);}}
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
            if(ui.performancePage){if(y>=height-135&&y<height-105){const int rows=performanceRows(width,height);const auto available=std::max(rates?rates->cores.size():0,ui.deviceRows.size());if(x>=38&&x<138&&ui.page)--ui.page;else if(x>=150&&x<250&&(ui.page+1)*static_cast<std::size_t>(rows)<available)++ui.page;dirty=true;}break;}
            if(!ui.tasks&&!ui.startupPage&&!ui.maintenancePage){const int top=metricsBottom(width);if(x>=width-204&&x<width-44&&y>=top+4&&y<top+34){ui.cancel();ui.backupPage=true;ui.message="Enter the source and a new destination path.";dirty=true;break;}}
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
            case KeyPress:{auto key=XLookupKeysym(&ev.xkey,0);if(ui.busy())break;
            if(!ui.pending&&!ui.taskSearch&&key>=XK_F1&&key<=XK_F10){c.frameReady=false;navigate(guiPages[static_cast<std::size_t>(key-XK_F1)].page);dirty=true;break;}
            if(c.frameReady&&(ui.pending||(!ui.backupPage&&!ui.taskSearch))&&(key==XK_Tab||key==XK_ISO_Left_Tab)){if(!c.targets.empty()){const int n=static_cast<int>(c.targets.size());c.focus=(ev.xkey.state&ShiftMask)?(c.focus<0?n-1:(c.focus+n-1)%n):(c.focus+1)%n;}dirty=true;break;}
            if(c.frameReady&&(ui.pending||(!ui.backupPage&&!ui.taskSearch))&&(key==XK_Return||key==XK_space)&&c.focus>=0&&static_cast<std::size_t>(c.focus)<c.targets.size()){const auto r=c.targets[static_cast<std::size_t>(c.focus)];XEvent click{};click.xbutton.type=ButtonPress;click.xbutton.display=d;click.xbutton.window=w;click.xbutton.button=Button1;click.xbutton.x=r.x+r.w/2;click.xbutton.y=r.y+r.h/2;XPutBackEvent(d,&click);break;}
            if(ui.pending&&key!=XK_Escape)break;
            c.focus=-1;
            if(ui.tasks&&ui.taskSearch&&!ui.pending){if(key==XK_Return||key==XK_Escape)ui.taskSearch=false;else if(key==XK_BackSpace){if(!ui.taskQuery.empty())ui.taskQuery.pop_back();}else{char typed[16];KeySym actual;int n=XLookupString(&ev.xkey,typed,sizeof typed,&actual,nullptr);for(int i=0;i<n&&ui.taskQuery.size()<128;++i)if(static_cast<unsigned char>(typed[i])>=32&&static_cast<unsigned char>(typed[i])<127)ui.taskQuery+=typed[i];}ui.selected.reset();ui.page=0;ui.taskRows=filterTasks(ui.taskModel,ui.taskQuery,ui.taskSort);dirty=true;break;}if(ui.backupPage&&key!=XK_Escape){ui.cancel();if(key==XK_Tab)ui.backupField=1-ui.backupField;else{auto& field=ui.backupField?ui.backupDestination:ui.backupSource;if(key==XK_BackSpace){if(!field.empty())field.pop_back();}else{char typed[16];KeySym actual;const int n=XLookupString(&ev.xkey,typed,sizeof typed,&actual,nullptr);for(int i=0;i<n&&field.size()<240;++i)if(static_cast<unsigned char>(typed[i])>=32&&static_cast<unsigned char>(typed[i])<127)field+=typed[i];}}ui.message="Edit paths, then Review copy.";dirty=true;break;}if(ui.hubPage&&key==XK_Page_Down){if((ui.outputPage+1)*6<static_cast<std::size_t>(std::count(ui.report.begin(),ui.report.end(),'\n')))++ui.outputPage;dirty=true;break;}if(ui.hubPage&&key==XK_Page_Up){if(ui.outputPage)--ui.outputPage;dirty=true;break;}if(ui.maintenancePage&&key==XK_Page_Down){if((ui.outputPage+1)*5<ui.output.size())++ui.outputPage;dirty=true;break;}if(ui.maintenancePage&&key==XK_Page_Up){if(ui.outputPage)--ui.outputPage;dirty=true;break;}if(key==XK_Escape&&ui.pending){c.frameReady=false;ui.cancel();ui.offlineTarget.reset();ui.offlineChecked.reset();ui.packagePreview=0;ui.trimPreview=false;ui.cleanPreview=false;ui.message="Cancelled. No changes made.";dirty=true;}else if(key==XK_q||key==XK_Escape)running=false;else if(key==XK_r){ui.cancel();ui.offlineTarget.reset();ui.offlineChecked.reset();ui.packagePreview=0;ui.trimPreview=false;ui.cleanPreview=false;if(ui.startupPage){ui.startupList=ui.startup.scan();ui.packages=auditPackages();ui.startupSelected.reset();}ui.selected.reset();ui.page=0;health=collectHealth();if(ui.tasks)refreshTasks(true);else inventory=inspect(256);ui.message="Evidence refreshed. Pending action cancelled.";dirty=true;}break;}default:break;}}
        const auto now=Sample::Clock::now();
        if(mapped&&(pageOf(ui)==GuiPage::Overview||pageOf(ui)==GuiPage::Performance)&&!ui.busy()&&!ui.pending){if(now-lastDevice>=std::chrono::seconds(2)){auto sample=captureDevices();if(ui.deviceBaseline){auto measured=deviceRates(*ui.deviceBaseline,sample);ui.deviceRows=measured?std::move(*measured):std::vector<DeviceRate>{};
            ui.deviceRows.erase(std::remove_if(ui.deviceRows.begin(),ui.deviceRows.end(),[](const DeviceRate& r){return r.kind==DeviceKind::Disk&&(r.name.rfind("loop",0)==0||r.name.rfind("ram",0)==0);}),ui.deviceRows.end());
            std::sort(ui.deviceRows.begin(),ui.deviceRows.end(),[](const DeviceRate& a,const DeviceRate& b){if(a.kind!=b.kind)return a.kind<b.kind;return a.name<b.name;});}ui.deviceBaseline=std::move(sample);lastDevice=now;dirty=true;}}
        else{ui.deviceBaseline.reset();ui.deviceRows.clear();}
        if(mapped&&ui.tasks&&!ui.pending&&!ui.busy()&&now-lastTask>=std::chrono::seconds(2)) {refreshTasks(false);dirty=true;}
        if(mapped&&now>=next){auto current=captureSample();rates=sampleRates(previous,current);if(rates)ui.history.push(*rates);else ui.history.push({});previous=std::move(current);health.memory=previous.system.memory;next=now+std::chrono::seconds(1);dirty=true;}
        if(mapped&&dirty){if(!buffer||bufferWidth!=width||bufferHeight!=height){if(buffer){c.endFrame();XFreePixmap(d,buffer);}buffer=XCreatePixmap(d,w,width,height,DefaultDepth(d,s));bufferWidth=width;bufferHeight=height;}c.beginFrame(buffer,width,height);render(c,width,height,rates,health,inventory,ui);c.flushFrame();XCopyArea(d,buffer,w,gc,0,0,width,height,0,0);XFlush(d);dirty=false;++draws;}
        if(smoke&&draws>=2&&rates&&rates->cpu){
            XSync(d,False);auto* image=XGetImage(d,w,GuiLayout(width).sidebar+10,2,1,1,AllPlanes,ZPixmap);const bool ok=image&&XGetPixel(image,0,0)==c.accent;if(image)XDestroyImage(image);if(!ok)return 3;
            if(!narrowRequested){narrowRequested=true;XResizeWindow(d,w,700,620);XFlush(d);}
            else if(width==700&&height==620){auto* cardPixel=XGetImage(d,w,GuiLayout(width).sidebar+35,248,1,1,AllPlanes,ZPixmap);const bool wrapped=cardPixel&&XGetPixel(cardPixel,0,0)==c.card;if(cardPixel)XDestroyImage(cardPixel);if(!wrapped)return 5;std::cout<<"X11 wide/narrow rendering and live sampling smoke passed\n";break;}
        }
        if(smoke&&now>=deadline)return 4;
        pollfd fd{ConnectionNumber(d),POLLIN,0};const auto wait=std::chrono::duration_cast<std::chrono::milliseconds>(next-Sample::Clock::now()).count();poll(&fd,1,(smoke||ui.busy())?100:mapped?static_cast<int>(std::clamp<long long>(wait,1,1000)):-1);
    }
    c.endFrame();
    if(buffer)XFreePixmap(d,buffer);
    if(c.titleFont)XFreeFont(d,c.titleFont);
    if(normal)XFreeFont(d,normal);
    if(heading)XFreeFont(d,heading);
    XFreeGC(d,gc);XDestroyWindow(d,w);XCloseDisplay(d);return 0;
}
