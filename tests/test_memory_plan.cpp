// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/memory_plan.hpp"
#include <iostream>
#include <limits>
int main(){
    using namespace syspulse::memoryplan;
    if(assess(100,4,0).level!=Level::High)return 1;
    if(assess(100,5,0).level!=Level::Elevated)return 2;
    if(assess(100,14,0).level!=Level::Elevated)return 3;
    if(assess(100,15,0).level!=Level::Normal)return 4;
    if(assess(100,80,10).level!=Level::High)return 5;
    if(assess(100,80,1).level!=Level::Elevated)return 6;
    if(assess(100,80,0.99).level!=Level::Normal)return 7;
    if(assess({}, {}, {}).level!=Level::Unknown)return 8;
    if(assess(0,0,0).ramKnown||assess(100,101,0).ramKnown)return 9;
    if(assess(100,80,std::numeric_limits<double>::quiet_NaN()).pressureKnown)return 10;
    if(assess(100,80,-1).level!=Level::Unknown||assess(100,80,101).pressureKnown)return 11;
    if(assess({}, {}, 11).level!=Level::High)return 12;
    if(assess(100,4,{}).level!=Level::High)return 13;
    std::cout<<"13 memory pressure planning checks passed\n";
}
