// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/health.hpp"
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
    void rect(int x,int y,int width,int height,unsigned long pixel){if(width<=0||height<=0)return;XSetForeground(d,gc,pixel);XFillRectangle(d,w,gc,x,y,width,height);}
    void text(int x,int y,const std::string& value,unsigned long pixel,bool large=false){auto* font=large&&heading?heading:normal;if(font)XSetFont(d,gc,font->fid);XSetForeground(d,gc,pixel);XDrawString(d,w,gc,x,y,value.c_str(),static_cast<int>(value.size()));}
};
std::string percent(const std::optional<double>& v){if(!v)return "Unavailable";char b[32];std::snprintf(b,sizeof b,"%.1f%%",*v);return b;}
std::string capacity(const std::optional<triage::Counters>& v){if(!v||!triage::valid(v))return "Unavailable";char b[64];std::snprintf(b,sizeof b,"%.1f GiB free",static_cast<double>(v->available)/1073741824.0);return b;}
const char* severity(triage::Severity s){switch(s){case triage::Severity::Unknown:return "UNKNOWN";case triage::Severity::Ok:return "OK";case triage::Severity::Info:return "INFO";case triage::Severity::Warning:return "REVIEW";case triage::Severity::Critical:return "CRITICAL";}return "UNKNOWN";}
void render(Canvas& c,int width,int height,const std::optional<Rates>& rates,const HealthEvidence& health,std::size_t processCount){
    c.rect(0,0,width,height,c.background);c.rect(0,0,width,6,c.accent);
    c.text(24,36,"SysPulse Native",c.ink,true);c.text(24,58,"SYSTEM OVERVIEW  /  READ-ONLY PREVIEW",c.muted);
    c.rect(24,76,width-48,63,c.card);c.rect(24,76,4,63,c.accent);
    c.text(40,100,"System care and live evidence",c.ink,true);c.text(40,123,"R: refresh evidence     Q: close     No system changes",c.muted);
    const int gap=12,columns=width>=960?4:2,cardWidth=(width-48-(columns-1)*gap)/columns;
    const std::string labels[]={"CPU usage","Memory used","Root storage","Readable processes"};
    const std::string values[]={percent(rates?rates->cpu:std::nullopt),percent(rates?rates->memoryUsed:std::nullopt),capacity(health.rootBytes),std::to_string(processCount)};
    for(int i=0;i<4;++i){const int x=24+i%columns*(cardWidth+gap),y=153+i/columns*88;c.rect(x,y,cardWidth,76,c.card);c.rect(x,y,cardWidth,2,c.line);c.text(x+12,y+25,labels[i],c.muted);c.text(x+12,y+54,values[i],i==0?c.green:c.ink,true);}
    const int top=153+4/columns*88;
    c.rect(24,top,width-48,150,c.card);c.text(40,top+24,"Health findings",c.ink,true);
    const char* names[]={"Memory","Root storage","Swap"};int i=0;
    for(auto f:assessHealth(health)){const int y=top+50+i*30;c.text(40,y,names[i],c.ink);c.text(154,y,severity(f.severity),f.severity==triage::Severity::Ok?c.green:c.accent);if(width>=840)c.text(252,y,f.advice,c.muted);++i;}
    c.text(24,height-18,"Prototype: no Boost, process control, installer or privilege broker.",c.muted);
    XFlush(c.d);
}
}
int main(int argc,char** argv){
    const bool smoke=argc==2&&std::strcmp(argv[1],"--smoke")==0;
    if(argc>1&&!smoke)return 2;
    Display* d=XOpenDisplay(nullptr);if(!d){std::cerr<<"X11 display unavailable\n";return 1;}
    const int s=DefaultScreen(d);int width=1100,height=600;
    Window w=XCreateSimpleWindow(d,RootWindow(d,s),30,30,width,height,0,BlackPixel(d,s),WhitePixel(d,s));
    XStoreName(d,w,"SysPulse Native - R4 Preview");XSizeHints hints{};hints.flags=PMinSize;hints.min_width=640;hints.min_height=540;XSetWMNormalHints(d,w,&hints);
    XSelectInput(d,w,ExposureMask|StructureNotifyMask|KeyPressMask);Atom close=XInternAtom(d,"WM_DELETE_WINDOW",False);XSetWMProtocols(d,w,&close,1);
    GC gc=XCreateGC(d,w,0,nullptr);auto* normal=XLoadQueryFont(d,"fixed");auto* heading=XLoadQueryFont(d,"9x15bold");
    Canvas c{d,w,gc,s,normal,heading,0,0,0,0,0,0,0};c.background=c.color("#edf2f8");c.card=c.color("#ffffff");c.ink=c.color("#142336");c.muted=c.color("#506079");c.accent=c.color("#b51235");c.line=c.color("#cbd6e2");c.green=c.color("#148344");
    XMapWindow(d,w);XSync(d,False);
    auto previous=captureSample();auto health=collectHealth();auto inventory=inspect(256);std::optional<Rates> rates;
    bool running=true,mapped=true,dirty=true;int draws=0;bool narrowRequested=false;auto next=Sample::Clock::now()+std::chrono::seconds(1);const auto deadline=Sample::Clock::now()+std::chrono::seconds(8);
    while(running){
        while(XPending(d)){XEvent ev;XNextEvent(d,&ev);switch(ev.type){case Expose:dirty=true;break;case ConfigureNotify:width=ev.xconfigure.width;height=ev.xconfigure.height;dirty=true;break;case UnmapNotify:mapped=false;break;case MapNotify:mapped=true;previous=captureSample();rates.reset();next=Sample::Clock::now()+std::chrono::seconds(1);dirty=true;break;case ClientMessage:if(static_cast<Atom>(ev.xclient.data.l[0])==close)running=false;break;case KeyPress:{auto key=XLookupKeysym(&ev.xkey,0);if(key==XK_q||key==XK_Escape)running=false;else if(key==XK_r){health=collectHealth();inventory=inspect(256);dirty=true;}break;}default:break;}}
        const auto now=Sample::Clock::now();if(mapped&&now>=next){auto current=captureSample();rates=sampleRates(previous,current);previous=std::move(current);health.memory=previous.system.memory;next=now+std::chrono::seconds(1);dirty=true;}
        if(mapped&&dirty){render(c,width,height,rates,health,inventory.processes.size());dirty=false;++draws;}
        if(smoke&&draws>=2&&rates&&rates->cpu){
            XSync(d,False);auto* image=XGetImage(d,w,10,2,1,1,AllPlanes,ZPixmap);const bool ok=image&&XGetPixel(image,0,0)==c.accent;if(image)XDestroyImage(image);if(!ok)return 3;
            if(!narrowRequested){narrowRequested=true;XResizeWindow(d,w,640,540);XFlush(d);}
            else if(width==640&&height==540){auto* cardPixel=XGetImage(d,w,30,245,1,1,AllPlanes,ZPixmap);const bool wrapped=cardPixel&&XGetPixel(cardPixel,0,0)==c.card;if(cardPixel)XDestroyImage(cardPixel);if(!wrapped)return 5;std::cout<<"X11 wide/narrow rendering and live sampling smoke passed\n";break;}
        }
        if(smoke&&now>=deadline)return 4;
        pollfd fd{ConnectionNumber(d),POLLIN,0};const auto wait=std::chrono::duration_cast<std::chrono::milliseconds>(next-Sample::Clock::now()).count();poll(&fd,1,smoke?100:mapped?static_cast<int>(std::clamp<long long>(wait,1,1000)):-1);
    }
    if(normal)XFreeFont(d,normal);
    if(heading)XFreeFont(d,heading);
    XFreeGC(d,gc);XDestroyWindow(d,w);XCloseDisplay(d);return 0;
}
