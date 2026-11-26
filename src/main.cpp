#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/statvfs.h>
#include "health.hpp"
#include "inventory.hpp"
#include <fstream>
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
constexpr const char* version = "0.3.0";
std::vector<std::string> diagnostics() {
    std::vector<std::string> rows;
    std::ifstream mem("/proc/meminfo");
    const auto m=sp::parseMemory(mem);
    rows.push_back(sp::memorySummary(m));
    rows.push_back(sp::swapSummary(m));
    struct statvfs fs{};
    if(statvfs("/",&fs)==0 && fs.f_blocks) rows.push_back("Root filesystem available: "+std::to_string(static_cast<unsigned long long>(fs.f_bavail)*fs.f_frsize/1048576)+" MiB");
    else rows.push_back("Storage query failed");
    rows.push_back("Logical CPUs: "+std::to_string(sysconf(_SC_NPROCESSORS_ONLN)));
    return rows;
}
std::string helper() {
    std::array<char,4096> b{}; auto n=readlink("/proc/self/exe",b.data(),b.size()-1);
    if(n<=0 || static_cast<size_t>(n)>=b.size()-1) return {};
    std::string s(b.data(),static_cast<size_t>(n)); return s.substr(0,s.rfind('/')+1)+"syspulse-update";
}
int main(int argc,char** argv) {
    if(argc==2 && std::string(argv[1])=="--version"){std::cout<<version<<'\n';return 0;}
    if(argc==2 && std::string(argv[1])=="--diagnostics"){for(auto& s:diagnostics()) std::cout<<s<<'\n';return 0;}
    if(argc==2 && std::string(argv[1])=="--capabilities"){for(auto& s:sp::capabilityRows()) std::cout<<s<<'\n';return 0;}
    if(argc==2 && std::string(argv[1])=="--browser-summary"){for(auto& s:sp::browserRows(sp::browserSnapshot())) std::cout<<s<<'\n';return 0;}
    if(argc!=1){std::cerr<<"Usage: syspulse [--version|--diagnostics|--capabilities|--browser-summary]\n";return 2;}
    if(geteuid()==0){std::cerr<<"Run the GUI as a normal user.\n";return 1;}
    Display* d=XOpenDisplay(nullptr); if(!d){std::cerr<<"X11 display unavailable.\n";return 1;}
    int screen=DefaultScreen(d); auto win=XCreateSimpleWindow(d,RootWindow(d,screen),40,40,920,430,1,BlackPixel(d,screen),WhitePixel(d,screen));
    XStoreName(d,win,"SysPulse Community - Native Linux Preview");
    auto gc=XCreateGC(d,win,0,nullptr); XSelectInput(d,win,ExposureMask|ButtonPressMask|KeyPressMask|StructureNotifyMask);
    Atom close=XInternAtom(d,"WM_DELETE_WINDOW",False); XSetWMProtocols(d,win,&close,1); XMapWindow(d,win);
    int page=0; auto rows=diagnostics(); std::string status="Ready. Read-only diagnostics; updates require a trusted signing key."; pid_t child=-1; bool confirm=false, running=true;
    auto draw=[&]{XClearWindow(d,win); auto line=[&](int x,int y,const std::string& s){XDrawString(d,win,gc,x,y,s.c_str(),static_cast<int>(s.size()));};
        line(24,32,"SysPulse Community 0.3.0 | Linux X11 preview"); line(24,62,page==0?"SYSTEM HEALTH - local snapshot":page==1?"INSTALLED COMMANDS - standard paths only":"BROWSER - aggregate local process snapshot"); int y=86; for(auto& s:rows){line(24,y,s);y+=24;}
        XDrawRectangle(d,win,gc,24,200,170,38);line(36,224,"Refresh page");
        XDrawRectangle(d,win,gc,210,200,170,38);line(222,224,"Check signed update");
        XDrawRectangle(d,win,gc,396,200,190,38);line(408,224,confirm?"Confirm installation":"Install signed update");
        XDrawRectangle(d,win,gc,24,248,170,30);line(36,268,"System health");
        XDrawRectangle(d,win,gc,210,248,170,30);line(222,268,"Capabilities");
        XDrawRectangle(d,win,gc,396,248,190,30);line(408,268,"Browser summary");
        line(24,306,status);line(24,342,"Updates support a per-user Linux x86_64 installation only.");line(24,366,"Restart manually after an update. Esc cancels confirmation.");XFlush(d);};
    auto start=[&](const char* mode){auto path=helper(); if(path.empty()||access(path.c_str(),X_OK)!=0){status="Update helper not installed beside this executable.";return;} child=fork(); if(child==0){execl(path.c_str(),path.c_str(),mode,version,static_cast<char*>(nullptr));_exit(127);} if(child<0){status="Could not start updater.";return;} status="Update operation running; see terminal for details.";};
    while(running){while(XPending(d)){XEvent ev; XNextEvent(d,&ev); if(ev.type==Expose) draw();
        else if(ev.type==ClientMessage && static_cast<Atom>(ev.xclient.data.l[0])==close){if(child<0)running=false;else{status="Update active; close after completion.";draw();}}
        else if(ev.type==KeyPress && XLookupKeysym(&ev.xkey,0)==XK_Escape){confirm=false;draw();}
        else if(ev.type==ButtonPress && child<0 && ev.xbutton.y>=248 && ev.xbutton.y<=278){int x=ev.xbutton.x; if(x>=24&&x<=194)page=0;else if(x>=210&&x<=380)page=1;else if(x>=396&&x<=586)page=2;rows=page==0?diagnostics():page==1?sp::capabilityRows():sp::browserRows(sp::browserSnapshot());confirm=false;draw();}
        else if(ev.type==ButtonPress && child<0 && ev.xbutton.y>=200 && ev.xbutton.y<=238){int x=ev.xbutton.x; if(x>=24&&x<=194){rows=page==0?diagnostics():page==1?sp::capabilityRows():sp::browserRows(sp::browserSnapshot());confirm=false;}else if(x>=210&&x<=380){confirm=false;start("check");}else if(x>=396&&x<=586){if(confirm){confirm=false;start("install");}else{confirm=true;status="Click Confirm installation to verify and replace the user binary.";}}draw();}}
        if(child>0){int code=0; auto r=waitpid(child,&code,WNOHANG); if(r==child || (r<0&&errno!=EINTR)){child=-1;status=(r>0&&WIFEXITED(code)&&WEXITSTATUS(code)==0)?"Verified update operation completed; see terminal for details.":"Update unavailable, rejected, or failed. No unsigned update is installed.";draw();}}
        usleep(40000);
    } XFreeGC(d,gc); XDestroyWindow(d,win);XCloseDisplay(d);return 0;
}
