// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/health.hpp"
#include <cstdlib>
#include <iostream>
using namespace syspulse::native;
using syspulse::triage::Severity;
int count=0;void check(bool b,const char* label){++count;if(!b){std::cerr<<label<<'\n';std::exit(1);}}
int main(){
    auto unknown=assessHealth({});check(unknown.size()==3,"three checks");for(auto f:unknown)check(f.severity==Severity::Unknown,"missing evidence unknown");
    HealthEvidence e{Memory{100,4,{},0},syspulse::triage::Counters{100,2}};
    auto f=assessHealth(e);check(f[0].severity==Severity::Critical,"low RAM");check(f[1].severity==Severity::Critical,"low disk");check(f[2].severity==Severity::Info,"no swap");
    auto p=planReview(e);check(!p.incomplete&&p.checks.size()==2,"review critical");
    e.memory->availableKiB=5;e.rootBytes->available=10;f=assessHealth(e);check(f[0].severity==Severity::Warning,"RAM boundary");check(f[1].severity==Severity::Warning,"disk boundary");
    e.memory->availableKiB=15;e.rootBytes->available=11;f=assessHealth(e);check(f[0].severity==Severity::Ok&&f[1].severity==Severity::Ok,"healthy thresholds");check(planReview(e).checks.empty(),"no unnecessary review");
    e.memory->availableKiB=101;check(planReview(e).incomplete,"invalid memory");check(planReview({}).checks.size()==3,"unknown review");
    Process a{1,0,"init",'S',0,1,{}},b{2,1,"child",'S',0,2,{}},c{3,2,"child",'S',0,3,{}};
    auto tree=processForest({c,b,a});check(tree&&tree->size()==3&&!(*tree)[0].parent,"root");check((*tree)[2].parent==2,"parent");tree=processForest({c});check(tree&&!tree->front().parent,"missing parent");check(!processForest({a,a}),"duplicate");b.parent=2;check(!processForest({b}),"self cycle");b.parent=3;check(!processForest({b,c}),"two node cycle");check(processForest({})->empty(),"empty");check(!processForest(std::vector<Process>(4097,a)),"bounded");
#if defined(__linux__)
    auto live=collectHealth();check(live.memory.has_value(),"live memory");check(live.rootBytes&&live.rootBytes->available<=live.rootBytes->total,"live root");
#endif
    std::cout<<count<<" health and process tree checks passed\n";
}
