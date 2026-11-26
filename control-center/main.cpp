#include "catalog.hpp"
#include "diagnosis.hpp"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <sys/wait.h>
#include <iostream>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
int main(int argc,char** argv){
    if(argc==2 && std::string(argv[1])=="--version"){std::cout<<"0.4.0\n";return 0;}
    if(argc==2 && std::string(argv[1])=="--diagnose"){for(auto& f:sp::snapshot())std::cout<<f.level<<": "<<f.title<<" | "<<f.advice<<'\n';return 0;}
    if(argc!=1 || geteuid()==0){std::cerr<<"Start as a normal desktop user. Options: --version, --diagnose\n";return 1;}
    Display* d=XOpenDisplay(nullptr);if(!d){std::cerr<<"X11 display unavailable\n";return 1;}
    int screen=DefaultScreen(d);auto root=RootWindow(d,screen);auto w=XCreateSimpleWindow(d,root,20,20,1050,720,0,0,0);
    XStoreName(d,w,"SysPulse | System Control Center");auto gc=XCreateGC(d,w,0,nullptr);XSelectInput(d,w,ExposureMask|ButtonPressMask|KeyPressMask|StructureNotifyMask);
    XSizeHints hints{};hints.flags=PMinSize;hints.min_width=920;hints.min_height=660;XSetWMNormalHints(d,w,&hints);
    Atom close=XInternAtom(d,"WM_DELETE_WINDOW",False);XSetWMProtocols(d,w,&close,1);XMapWindow(d,w);
    auto color=[&](const char* name){XColor c{},exact{};return XAllocNamedColor(d,DefaultColormap(d,screen),name,&c,&exact)?c.pixel:BlackPixel(d,screen);};
    auto bg=color("#eef2f7"),sidebar=color("#15283c"),accent=color("#087f86"),white=color("#ffffff"),ink=color("#172c40"),muted=color("#52677b"),light=color("#bed0df"),red=color("#b42318"),amber=color("#946200");
    auto small=XLoadQueryFont(d,"fixed");auto large=XLoadQueryFont(d,"9x15bold");if(!large)large=small;
    int page=0,width=1050,height=720,selected=-1;bool confirm=false,running=true;pid_t admin=-1;std::vector<pid_t> children;std::string status="Choose a panel. System changes require explicit approval.";auto findings=sp::snapshot();
    std::array<char,4096> path{};auto n=readlink("/proc/self/exe",path.data(),path.size()-1);std::string sibling;
    if(n>0 && n<static_cast<ssize_t>(path.size()-1)){std::string p(path.data(),static_cast<size_t>(n));sibling=p.substr(0,p.rfind('/')+1)+"syspulse";}
    auto resolve=[&](const sp::Tool& t){std::string cmd=t.commands;if(cmd=="@health")return !sibling.empty()&&access(sibling.c_str(),X_OK)==0?sibling:std::string{};if(cmd=="@apt")return !sp::findCommand("apt-get").empty()?sp::findCommand("pkexec"):std::string{};return sp::findCommand(cmd);};
    auto launch=[&](const sp::Tool& t){auto exe=resolve(t);if(exe.empty()){status="Not installed or unsupported. Use your distribution package manager.";return;}bool elevated=std::string(t.commands)=="@apt";if(elevated && admin>0){status="An administrator action is already running.";return;}auto pid=fork();if(pid==0){if(elevated){auto apt=sp::findCommand("apt-get");execl(exe.c_str(),exe.c_str(),apt.c_str(),"update",static_cast<char*>(nullptr));}else execl(exe.c_str(),exe.c_str(),static_cast<char*>(nullptr));_exit(127);}if(pid<0){status="Could not start tool.";return;}if(elevated){admin=pid;status="Waiting for system authentication / APT refresh. Output is in the launching terminal.";}else{children.push_back(pid);status="Tool launch requested. Its own authorization and results are managed by that tool.";}};
    auto rect=[&](int x,int y,int a,int b,unsigned long c){if(a<=0||b<=0)return;XSetForeground(d,gc,c);XFillRectangle(d,w,gc,x,y,a,b);};
    auto line=[&](int x,int y,std::string s,unsigned long c,bool big=false){XSetForeground(d,gc,c);auto f=big?large:small;if(f)XSetFont(d,gc,f->fid);XDrawString(d,w,gc,x,y,s.c_str(),static_cast<int>(s.size()));};
    auto visible=[&]{std::vector<int> v;for(size_t i=0;i<sp::tools.size();++i)if(std::string(sp::tools[i].group)==sp::groups[page])v.push_back(static_cast<int>(i));return v;};
    auto draw=[&]{rect(0,0,width,height,bg);rect(0,0,204,height,sidebar);line(20,31,"SYSPULSE",white,true);line(20,51,"COMMUNITY CONTROL CENTER",light);
        for(size_t i=0;i<sp::groups.size();++i){int y=74+static_cast<int>(i)*40;if(static_cast<int>(i)==page)rect(8,y,188,34,accent);line(20,y+22,sp::groups[i],white,true);}
        line(228,35,sp::groups[page],ink,true);line(228,57,"Linux system tools | Native diagnostics and installed utility shortcuts",muted);
        if(page==0){int y=84;for(auto& f:findings){rect(228,y,width-248,88,white);auto c=f.level=="CRITICAL"?red:f.level=="WARNING"?amber:accent;line(244,y+22,f.level,c,true);line(244,y+44,f.title,ink,true);line(244,y+65,f.advice,muted);y+=100;}rect(228,393,210,36,accent);line(242,416,"Refresh diagnosis",white,true);line(228,457,"Checks cover storage pressure, RAM availability and swap configuration only.",muted);line(228,481,"No changes are made by diagnosis. Open the relevant panel to investigate.",muted);}
        else{int y=82;for(int i:visible()){auto& t=sp::tools[i];rect(228,y,width-248,54,i==selected?light:white);line(242,y+20,t.name,ink,true);line(242,y+40,t.detail,muted);line(std::max(700,width-160),y+20,resolve(t).empty()?"Unavailable":"Open / select",accent);y+=60;}}
        rect(204,height-76,width-204,76,white);if(confirm){line(228,height-52,"Confirm APT metadata refresh? No package upgrade. Press Enter or Esc.",red,true);}line(228,height-25,status,ink);XFlush(d);};
    draw();while(running){while(XPending(d)){XEvent e;XNextEvent(d,&e);if(e.type==Expose)draw();else if(e.type==ConfigureNotify){width=e.xconfigure.width;height=e.xconfigure.height;draw();}else if(e.type==ClientMessage&&static_cast<Atom>(e.xclient.data.l[0])==close){if(admin<=0)running=false;else{status="Administrator action active; finish or cancel authentication first.";draw();}}
        else if(e.type==KeyPress){auto k=XLookupKeysym(&e.xkey,0);if(k==XK_Escape){confirm=false;status="Selection cancelled.";}else if(k==XK_Return&&confirm&&selected>=0){confirm=false;launch(sp::tools[selected]);}draw();}
        else if(e.type==ButtonPress){int x=e.xbutton.x,y=e.xbutton.y;if(x<204&&y>=74&&y<74+40*static_cast<int>(sp::groups.size())){page=(y-74)/40;selected=-1;confirm=false;status="Choose a tool. Unavailable tools are not installed.";}else if(page==0&&x>=228&&y>=393&&y<=429){findings=sp::snapshot();status="Diagnostic snapshot refreshed.";}else if(page!=0&&x>=228&&y>=82&&y<height-76){auto v=visible();int row=(y-82)/60;if(row>=0&&row<static_cast<int>(v.size())){selected=v[row];auto& t=sp::tools[selected];confirm=std::string(t.commands)=="@apt";if(confirm)status="Review the action, then press Enter to authorize.";else launch(t);}}draw();}}
        if(admin>0){int c=0;auto r=waitpid(admin,&c,WNOHANG);if(r==admin||(r<0&&errno!=EINTR)){admin=-1;status=r>0&&WIFEXITED(c)&&WEXITSTATUS(c)==0?"APT metadata refresh exited successfully.":"APT refresh failed or authorization was cancelled.";draw();}}
        for(auto it=children.begin();it!=children.end();){int c=0;auto r=waitpid(*it,&c,WNOHANG);if(r==*it||(r<0&&errno!=EINTR)){if(r>0&&WIFEXITED(c)&&WEXITSTATUS(c)==127){status="Tool could not be executed.";draw();}it=children.erase(it);}else ++it;}
        usleep(40000);
    }XFreeGC(d,gc);if(large&&large!=small)XFreeFont(d,large);if(small)XFreeFont(d,small);XDestroyWindow(d,w);XCloseDisplay(d);return 0;
}
