// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/system_tools.hpp"
#include "syspulse/native/maintenance.hpp"
#include "syspulse/system_memory.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>
using namespace syspulse::native;
int n=0;void check(bool x,const char* message){++n;if(!x){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
    check(parseZswap("Y")==ZswapState::Enabled,"enabled");check(parseZswap("0")==ZswapState::Disabled,"disabled");check(parseZswap("garbage")==ZswapState::Unknown,"unknown");
    check(compressionChangeAllowed({ZswapState::Disabled,true,false},true),"eligible");check(!compressionChangeAllowed({ZswapState::Disabled,false,false},true),"no swap");check(!compressionChangeAllowed({ZswapState::Disabled,true,true},true),"zram conflict");check(!compressionChangeAllowed({ZswapState::Disabled,true,{}},true),"unknown zram");check(!compressionChangeAllowed({},true),"unknown");check(!compressionChangeAllowed({},false),"unknown disable");check(compressionChangeAllowed({ZswapState::Enabled,{},{}},false),"runtime disable");
    std::istringstream s("Filename Type Size Used Priority\n/dev/sda1 partition 100 10 -2\n");check(syspulse::system_memory::activeSwap(s)==true,"swap fixture");std::istringstream z("Filename Type Size Used Priority\n/dev/zram0 partition 100 10 -2\n");check(!syspulse::system_memory::activeSwap(z),"zram refused");
    auto trim=trimOwnMemory();check(trim.before&&trim.after,"actual own RSS snapshots");
    auto c=commandFor(Operation::ConfigurePackages);check(c&&c->admin&&c->args==std::vector<std::string>{"/usr/bin/dpkg","--configure","-a"},"fixed repair");c=commandFor(Operation::PreviewUpgrade);check(c&&c->args==std::vector<std::string>{"/usr/bin/apt-get","-s","--no-remove","upgrade"},"upgrade preview only");check(!commandFor(Operation::ApplyUpgrade),"upgrade requires reviewed token");c=commandFor(Operation::ApplyUpgrade,std::string(64,'a'));check(c&&c->args[0]=="/usr/bin/syspulse-native-package-helper"&&c->args[1]=="upgrade","fixed reviewed upgrade");c=commandFor(Operation::PreviewRepair);check(c&&c->args==std::vector<std::string>{"/usr/bin/apt-get","-s","--no-remove","--fix-broken","install"},"repair simulation");check(!commandFor(Operation::ApplyRepair,"invalid"),"repair invalid token");c=commandFor(Operation::ApplyRepair,std::string(64,'a'));check(c&&c->args[1]=="repair","fixed reviewed repair");c=commandFor(Operation::TrimRoot);check(c&&c->args.back()=="/","fixed root mount");check(!commandFor(Operation::TrimRoot,"/home"),"reject other mount");c=commandFor(Operation::EnableCompression);check(c&&c->args.back()=="enable","fixed helper");Job j;check(!j.start({"/usr/bin/pkexec",{"/bin/sh"},true}),"root shell rejected");check(!j.start({"/usr/bin/pkexec",{"/usr/bin/dpkg","--remove","foo"},true}),"unreviewed removal rejected");check(!j.start({"/usr/bin/lsblk",{"--help"},false}),"unreviewed args rejected");
    if(j.start(*commandFor(Operation::DiskInventory))){bool done=false;for(int i=0;i<500;++i){if(auto r=j.poll()){check(r->code==0&&!r->output.empty(),"live disk inventory");done=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}check(done,"disk inventory completes");}else check(false,"lsblk launch");
    std::cout<<n<<" system tools checks passed\n";
}
