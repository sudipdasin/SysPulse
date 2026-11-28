// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/apt_plan.hpp"
#include "syspulse/native/maintenance.hpp"
#include <cstdlib>
#include <iostream>
using namespace syspulse::native;
int n=0;void check(bool x,const char* label){++n;if(!x){std::cerr<<label<<'\n';std::exit(1);}}
int main(){const std::string empty="0 upgraded, 0 newly installed, 0 to remove and 0 not upgraded.\n",one="1 upgraded, 0 newly installed, 0 to remove and 2 not upgraded.\nInst demo [1] (2 stable [amd64])\nConf demo (2 stable [amd64])\n";
    auto token=aptPlanToken("upgrade",empty);check(token&&validPlanToken(*token),"real empty plan format");auto plan=aptPlanToken("upgrade",one);check(plan&&*plan!=*token,"actual actions change token");check(aptPlanToken("upgrade","Reading package lists...\n"+one)==plan,"progress ignored");check(aptPlanToken("repair",one)!=plan,"operation binding");check(!aptPlanToken("upgrade",empty+empty),"duplicate summary");check(!aptPlanToken("upgrade","1 upgraded, 0 newly installed, 1 to remove and 0 not upgraded.\n"),"removal refused");check(!aptPlanToken("upgrade",empty+"Remv package [1]\n"),"removal action refused");check(!aptPlanToken("upgrade",empty+"Purg package [1]\n"),"purge refused");check(!aptPlanToken("upgrade","1 upgraded, 0 newly installed, 0 to remove and 0 not upgraded.\n"),"missing install action");check(!aptPlanToken("upgrade","Inst demo [1] (2 stable)\n"),"missing summary");check(!aptPlanToken("upgrade",std::string(65537,'x')),"bounded output");check(!aptPlanToken("invalid",empty),"fixed operation");check(!validPlanToken(std::string(64,'Z')),"hex validation");
    Job job;check(!job.start({"/usr/bin/pkexec",{"/usr/bin/apt-get","--no-remove","--assume-yes","upgrade"},true}),"direct upgrade bypass refused");check(!commandFor(Operation::ApplyUpgrade,"bad"),"invalid digest refused");check(commandFor(Operation::ApplyUpgrade,*plan).has_value(),"approved token command");std::cout<<n<<" package plan checks passed\n";
}
