#include "tux/core.hpp"
#include "tux/trusted_command.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>
#include <QTemporaryDir>
#include <chrono>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
namespace {
int checks=0;
void require(bool ok,const char* text) { ++checks; if (!ok) throw std::runtime_error(text); }
void nap(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
int child(const QString& mode) {
    if (mode=="stream") { write(1,"first\n",6); nap(300); write(1,"last\n",5); }
    else if (mode=="error") { write(2,"failed\n",7); return 7; }
    else if (mode=="flood") { const QByteArray b(4096,'x'); for (int i=0;i<256;++i) write(1,b.data(),b.size()); }
    else if (mode=="timeout") { write(1,"before\n",7); nap(10000); }
    else if (mode=="hold") { if (fork()==0) { nap(10000); _exit(0); } write(1,"parent done\n",12); }
    else if (mode=="closed") { close(1); close(2); nap(10000); }
    else if (mode=="utf8") { write(1,"\xe2",1); nap(30); write(1,"\x82\xac",2); }
    else if (mode=="signal") { raise(SIGTERM); }
    return 0;
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if (QFileInfo(QString::fromLocal8Bit(argv[0])).fileName()=="pkexec") {
        for (int i=1;i<argc;++i) std::cout<<argv[i]<<'\n'; return 0;
    }
    if (argc>2 && QString(argv[1])=="--child") return child(argv[2]);
    if (argc>2 && QString(argv[1])=="--literal") { std::cout<<argv[2]; return 0; }
    const auto self=QCoreApplication::applicationFilePath();
    auto run=[&](const QString& mode,int ms=3000,tux::Output cb=tux::Output{}) {
        return tux::runCommand({self,"--child",mode},ms,std::move(cb));
    };
    try {
        require(tux::actions().size()==61,"58 existing tools plus three reviewed package and journal actions");
        const QStringList added={"cpu-topology","kernel-version","uptime-summary","systemd-state","boot-history","discard-support","disk-transport","package-holds","package-origins","native-architecture","pci-inventory","usb-inventory","display-modes","compositor-state","graphics-renderer","link-counters","socket-summary","network-manager-state","system-timer-files","user-timer-files"};
        for(const auto& id:added) {
            const auto& a=tux::action(id);
            require(!a.changes&&!a.admin,"new diagnostic is unprivileged and read only");
            require(!a.title.isEmpty()&&!a.description.isEmpty()&&!a.command.isEmpty(),"diagnostic metadata and fixed command are present");
            require(tux::matches(a,a.title),"diagnostic discoverable by full title");
            require(a.command.first()!="sh"&&a.command.first()!="bash","new probe never invokes shell");
        }
        require(tux::action("cpu-topology").command==QStringList{"lscpu"},"cpu-topology fixed argument contract");
        require(tux::action("kernel-version").command==QStringList{"uname","-srmo"},"kernel-version fixed argument contract");
        require(tux::action("uptime-summary").command==QStringList{"uptime","-p"},"uptime-summary fixed argument contract");
        require(tux::action("systemd-state").command==QStringList{"systemctl","is-system-running"},"systemd-state fixed argument contract");
        require(tux::action("boot-history").command==QStringList{"journalctl","--list-boots","--no-pager"},"boot-history fixed argument contract");
        require(tux::action("discard-support").command==QStringList{"lsblk","--discard"},"discard-support fixed argument contract");
        require(tux::action("disk-transport").command==QStringList{"lsblk","-o","NAME,TYPE,SIZE,ROTA,TRAN"},"disk-transport fixed argument contract");
        require(tux::action("package-holds").command==QStringList{"apt-mark","showhold"},"package-holds fixed argument contract");
        require(tux::action("package-origins").command==QStringList{"apt-cache","policy"},"package-origins fixed argument contract");
        require(tux::action("native-architecture").command==QStringList{"dpkg","--print-architecture"},"native-architecture fixed argument contract");
        require(tux::action("pci-inventory").command==QStringList{"lspci","-nn"},"pci-inventory fixed argument contract");
        require(tux::action("usb-inventory").command==QStringList{"lsusb"},"usb-inventory fixed argument contract");
        require(tux::action("display-modes").command==QStringList{"xrandr","--query"},"display-modes fixed argument contract");
        require(tux::action("compositor-state").command==QStringList{"xfconf-query","-c","xfwm4","-p","/general/use_compositing"},"compositor-state fixed argument contract");
        require(tux::action("graphics-renderer").command==QStringList{"glxinfo","-B"},"graphics-renderer fixed argument contract");
        require(tux::action("link-counters").command==QStringList{"ip","-s","link"},"link-counters fixed argument contract");
        require(tux::action("socket-summary").command==QStringList{"ss","-s"},"socket-summary fixed argument contract");
        require(tux::action("network-manager-state").command==QStringList{"nmcli","general","status"},"network-manager-state fixed argument contract");
        require(tux::action("system-timer-files").command==QStringList{"systemctl","list-unit-files","--type=timer","--no-pager"},"system-timer-files fixed argument contract");
        require(tux::action("user-timer-files").command==QStringList{"systemctl","--user","list-unit-files","--type=timer","--no-pager"},"user-timer-files fixed argument contract");
        require(tux::action("display-modes").x11&&tux::action("graphics-renderer").x11,"X11 queries gated");
        require(tux::action("zswap-enable").admin&&tux::action("zswap-enable").changes&&tux::action("zswap-disable").admin&&tux::action("zswap-disable").changes,"kernel controls require review and authorization");
        require(tux::action("trim-disable").admin&&tux::action("trim-disable").changes&&tux::action("trim-disable").command==QStringList{"systemctl","disable","--now","fstrim.timer"},"fixed reviewed timer disable action");
        require(tux::action("apt-refresh").command==QStringList{"apt-get","update"}&&tux::action("apt-refresh").admin&&tux::action("apt-refresh").changes,"fixed package-list refresh");
        require(tux::action("apt-autoclean").command==QStringList{"apt-get","autoclean"}&&tux::action("apt-autoclean").admin&&tux::action("apt-autoclean").changes,"fixed obsolete archive cleanup");
        require(tux::action("journal-cap").command==QStringList{"journalctl","--vacuum-size=100M"}&&tux::action("journal-cap").admin&&tux::action("journal-cap").changes,"fixed archived journal size policy");
        QSet<QString> ids;
        for (const auto& a:tux::actions()) {
            ids.insert(a.id);
            require(!a.admin || a.changes || a.id=="firewall-status","administrator tools are reviewed mutations or the explicit read-only firewall check");
            if (a.changes || a.admin) {
                bool rejected=false; try { tux::execute(a.id); } catch (const std::exception&) { rejected=true; }
                require(rejected,"mutation confirmation required");
            }
        }
        require(ids.size()==tux::actions().size(),"unique identifiers");
        require(tux::trustedSystemExecutable("../sh").isEmpty()&&tux::trustedSystemExecutable("sh\n").isEmpty(),"privileged executable names reject traversal and trailing newline");
        QTemporaryDir untrustedPath;
        require(untrustedPath.isValid(),"create isolated PATH fixture");
        const auto originalPath=qgetenv("PATH");qputenv("PATH",untrustedPath.path().toUtf8());
        const auto trustedShell=tux::trustedSystemExecutable("sh");
        if(originalPath.isNull())qunsetenv("PATH");else qputenv("PATH",originalPath);
        require(!trustedShell.isEmpty()&&!trustedShell.startsWith(untrustedPath.path()),"privileged command lookup is independent of user PATH");
        require(tux::matches(tux::action("storage"),"STORAGE usage"),"search ignores case");
        require(!tux::matches(tux::action("storage"),"wine"),"search excludes nonmatches");
        require(tux::matches(tux::action("storage"),"  "),"empty search");
        const auto session=qgetenv("XDG_SESSION_TYPE"); qputenv("XDG_SESSION_TYPE","wayland");
        require(tux::unavailable(tux::action("input")).contains("X11"),"Wayland gate");
        if (session.isNull()) qunsetenv("XDG_SESSION_TYPE"); else qputenv("XDG_SESSION_TYPE",session);
        auto r=run("error"); require(r.code==7 && r.output.contains("failed"),"stderr and exit code");
        r=run("flood"); require(r.truncated && r.output.size()==tux::OutputLimit,"bounded flood output");
        QVector<qint64> times; QElapsedTimer timer; timer.start();
        r=run("stream",3000,[&](const QByteArray&){times.append(timer.elapsed());});
        require(r.code==0 && times.size()>=2 && times.last()-times.first()>=200,"output streams before exit");
        r=run("timeout",150); require(r.code==124 && r.output.contains("before") && r.seconds<2,"partial output on timeout");
        r=run("hold",150); require(r.code==124 && r.seconds<2,"child holding pipe deadline");
        r=run("closed",150); require(r.code==124,"closed stdout deadline");
        r=run("utf8"); require(QString::fromUtf8(r.output)==QString::fromUtf8("€"),"split UTF8 retained");
        r=run("signal"); require(r.code==128+SIGTERM,"signal exit reported");
        bool caught=false;
        try { run("timeout",3000,[](const QByteArray&){throw std::runtime_error("callback");}); } catch (...) { caught=true; }
        require(caught,"callback exception propagated and child cleaned");
        r=tux::runCommand({self,"--literal","$(touch should-not-exist); literal"});
        require(r.output=="$(touch should-not-exist); literal","no shell expansion");
        std::atomic_bool preCancelled{true};
        const auto skipped=tux::runCommand({"/nonexistent-tux-command"},3000,{},&preCancelled);
        require(skipped.cancelled&&skipped.code==130,"pre-cancel does not spawn a command");
        caught=false; try { tux::runCommand({self},0); } catch (...) { caught=true; } require(caught,"positive timeout");
        caught=false; try { tux::runCommand({"/nonexistent-tux-command"}); } catch (...) { caught=true; } require(caught,"spawn failure");
        caught=false; try { tux::action("unknown"); } catch (...) { caught=true; } require(caught,"unknown action");
        auto report=r.json(); require(report.contains("code") && report.contains("command"),"JSON result schema");
        r=tux::execute("storage"); require(r.code==0 && r.output.contains("Filesystem"),"real df diagnostic");
        // The build container may not expose a physical block-device sysfs.
        // Do not equate command presence with successful hardware discovery.
        const auto blockProblem=tux::unavailable(tux::action("devices"));
        if(blockProblem.isEmpty()){
            const auto live=tux::execute("devices",false,10000);
            std::cout<<"Block inventory execution result: "<<live.code<<"; target hardware validation still required.\n";
            require(live.action==tux::action("devices").title&&!live.cancelled&&live.code!=124,"block inventory preserves the actual bounded execution result");
        }else std::cout<<"Block inventory unavailable in build container; target hardware validation required.\n";
        for(const auto& id:QStringList{"memory","kernel-version","native-architecture","package-audit","package-holds"}){
            const auto& a=tux::action(id);
            require(tux::unavailable(a).isEmpty(),"baseline container dependency required");
            auto live=tux::execute(id,false,10000);
            std::cout<<"Real diagnostic "<<id.toStdString()<<": exit "<<live.code<<std::endl;
            require(live.code==0&&!live.cancelled&&!live.truncated,"baseline diagnostic executes successfully");
        }
        if(qEnvironmentVariable("SYSPULSE_RUNTIME_X11")=="1"){
            for(const auto& id:QStringList{"display-modes","input","desktop-channels","compositor-state"}){
                require(tux::unavailable(tux::action(id)).isEmpty(),"virtual X11 dependency available");
                const auto live=tux::execute(id,false,10000);
                std::cout<<"X11 runtime diagnostic "<<id.toStdString()<<": exit "<<live.code<<std::endl;
                require(live.code==0&&!live.cancelled&&!live.truncated,"virtual X11 diagnostic actually executes");
            }
            for(const auto& id:QStringList{"compositor-off","compositor-on"}){
                const auto live=tux::execute(id,true,10000);
                const auto state=tux::execute("compositor-state",false,10000);
                require(live.code==0&&state.code==0&&state.output.trimmed()==(id=="compositor-off"?"false":"true"),"isolated XFCE setting mutation verified by readback");
                std::cout<<"X11 runtime "<<id.toStdString()<<": verified by readback"<<std::endl;
            }
        }
        const auto path=qgetenv("PATH");
        const auto protectedAuthorization=tux::trustedSystemExecutable("pkexec");
        const auto protectedApt=tux::trustedSystemExecutable("apt-get");
        QTemporaryDir temp; require(temp.isValid(),"temporary dependency sandbox");
        QFile::link(self,temp.filePath("pkexec")); QFile::link(self,temp.filePath("apt-get"));
        QFile::link(self,temp.filePath("xfconf-query"));
        qputenv("PATH",temp.path().toUtf8());
        require(tux::trustedSystemExecutable("pkexec")==protectedAuthorization&&
                tux::trustedSystemExecutable("apt-get")==protectedApt,
                "user PATH helpers cannot replace protected privileged executables");
        require(!protectedAuthorization.startsWith(temp.path())&&!protectedApt.startsWith(temp.path()),
                "privileged resolution rejects user-controlled helper locations without executing a mutation");
        if(tux::trustedSystemExecutable("xfconf-query").isEmpty())require(tux::unavailable(tux::action("compositor-off")).contains("Missing dependency"),"untrusted PATH cannot satisfy desktop mutation");
        require(tux::unavailable(tux::action("storage")).contains("Missing dependency"),"missing tool detected");
        qputenv("PATH",path);
        std::cout<<checks<<" checks passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr<<"FAILED: "<<e.what()<<'\n'; return 1; }
}
