// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/desktop.hpp"
#include <iostream>
#include <cstdlib>
#include <set>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
using namespace syspulse::native;
int count=0;void check(bool ok,const char* label){++count;if(!ok){std::cerr<<label<<'\n';std::exit(1);}}
int main(){std::set<std::string> paths;for(auto t:desktopTools()){check(std::string(t.path).rfind("/usr/bin/",0)==0,"absolute allowlist");check(paths.insert(t.path).second,"unique tool");}check(!desktopAvailable(999),"unknown tool");DesktopLauncher launcher;check(!launcher.launch(999),"reject unknown launch");if(!getuid())check(!launcher.launch(0),"root launch blocked");
    auto report=healthReport({}, {}, {});check(report.find("RAM available KiB: unknown")!=report.npos,"unknown RAM");check(report.find("CPU core rows: unknown")!=report.npos,"unknown CPU");
    HealthEvidence e{Memory{100,4,2,10},syspulse::triage::Counters{100,2}};Inspection i;i.processAvailable=true;i.processes.push_back({42,1,"PRIVATE-NAME",'S',0,1,{}});i.processTruncated=true;
    report=healthReport(e,{},i);check(report.find("RAM available KiB: 4")!=report.npos,"known RAM");check(report.find("critical:")!=report.npos,"triage reused");check(report.find("PRIVATE-NAME")==report.npos,"no process names");check(report.find("Process inventory capped.")!=report.npos,"partial inventory disclosed");
    check(!exportHealthReport("").published,"empty report refused");check(!exportHealthReport(std::string(16385,'x')).published,"oversized report refused");
    auto result=exportHealthReport(report);if(getuid()){check(result.published&&result.durable,"actual private report saved");struct stat st{};check(!lstat(result.path.c_str(),&st)&&S_ISREG(st.st_mode)&&(st.st_mode&0777)==0600,"report permissions");std::ifstream f(result.path);std::string content((std::istreambuf_iterator<char>(f)),{});check(content==report,"saved contents exact");auto second=exportHealthReport(report);check(second.published&&second.path!=result.path,"distinct no-overwrite reports");unlink(result.path.c_str());unlink(second.path.c_str());}else check(!result.published,"root export blocked");
    std::cout<<count<<" desktop and report checks passed\n";
}
