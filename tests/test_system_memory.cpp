// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/system_memory.hpp"
#include <iostream>
int main(){
    using namespace syspulse::system_memory;
    if(parseEnabled("Y")!=State::Enabled||parseEnabled("0")!=State::Disabled||parseEnabled("yes")!=State::Unknown)return 1;
    const std::string h="Filename Type Size Used Priority\n";
    const auto parse=[](const std::string& s){std::istringstream in(s);return activeSwap(in);};
    if(parse(h)!=false)return 2;
    if(parse(h+"/swapfile file 1024 10 -2\n")!=true)return 3;
    if(parse(h+"/dev/sda2 partition 1024 0 -1\n")!=true)return 4;
    if(parse(h+"/dev/zram0 partition 1024 0 100\n"))return 5;
    if(parse(h+"/swapfile file 1 2 -2\n"))return 6;
    if(parse(h+"/swapfile file -1 0 -2\n"))return 7;
    if(parse(h+"/swapfile file 1 0 bad\n"))return 8;
    if(parse("invalid\n"))return 9;
    if(!canEnable(State::Disabled,true,false))return 10;
    if(canEnable(State::Disabled,false,false)||canEnable(State::Disabled,true,true))return 11;
    if(canEnable(State::Unknown,true,false)||canEnable(State::Enabled,true,false))return 12;
    if(canEnable(State::Disabled,{},false)||canEnable(State::Disabled,true,{}))return 13;
    std::cout<<"13 system memory eligibility checks passed\n";
}
