// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/care_plan.hpp"
#include <iostream>
#include <limits>
int main(){
    using namespace syspulse;using S=triage::Severity;using R=care::Route;
    triage::Snapshot snapshot;std::array<pressure::Snapshot,3> psi{};
    auto result=care::plan(snapshot,psi);
    if(result.size()!=6)return 1;
    for(const auto& row:result)if(row.severity!=S::Unknown)return 2;
    snapshot.ram=triage::Counters{100,4};snapshot.root=triage::Counters{100,2};snapshot.swap=0;
    psi[0].some=pressure::Sample{40,0,0,0};psi[1].some=pressure::Sample{1,0,0,0};psi[2].some=pressure::Sample{0,0,0,0};
    result=care::plan(snapshot,psi);
    if(result[0].severity!=S::Critical||result[0].route!=R::Memory)return 3;
    if(result[1].severity!=S::Critical||result[1].route!=R::Storage)return 4;
    if(result[2].severity!=S::Critical||result[2].route!=R::Cpu)return 5;
    if(result[3].severity!=S::Warning||result[3].route!=R::Memory)return 6;
    if(result[4].severity!=S::Info||result[4].route!=R::Swap)return 7;
    if(result[5].severity!=S::Ok||result[5].route!=R::Io)return 8;
    psi[0].some=pressure::Sample{std::numeric_limits<double>::quiet_NaN(),0,0,0};
    result=care::plan(snapshot,psi);
    if(result[3].severity!=S::Unknown||result[3].route!=R::Cpu)return 9;
    snapshot.ram=triage::Counters{100,100};snapshot.root=triage::Counters{100,100};psi[0].some=pressure::Sample{0,0,0,0};psi[1].some=pressure::Sample{0,0,0,0};
    result=care::plan(snapshot,psi);
    if(result[0].severity!=S::Info)return 10;
    for(unsigned i=1;i<result.size();++i)if(result[i].severity!=S::Ok)return 11;
    std::cout<<"11 prioritized care-plan checks passed\n";
}
