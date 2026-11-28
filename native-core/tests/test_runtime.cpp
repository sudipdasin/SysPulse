// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/runtime.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>
using namespace syspulse::native;
int count=0;void check(bool ok,const char* name){++count;if(!ok){std::cerr<<name<<'\n';std::exit(1);}}
int main(){
    const std::string some="some avg10=12.50 avg60=1.20 avg300=0.00 total=123\n";
    auto p=parsePressure(some);check(p&&p->some.avg10==12.5&&p->some.totalUs==123&&!p->full,"some only");
    p=parsePressure(some+"full avg10=0.00 avg60=0.00 avg300=0.00 total=0\n");check(p&&p->full&&p->full->totalUs==0,"full present");
    check(!parsePressure(some+some),"duplicate");check(!parsePressure("full avg10=1 avg60=1 avg300=1 total=1\n"),"missing some");
    check(!parsePressure("some avg10=101 avg60=0 avg300=0 total=1"),"range");check(!parsePressure("some avg10=NaN avg60=0 avg300=0 total=1"),"NaN");
    check(!parsePressure("some avg10=1 avg60=0 avg300=0 total=-1"),"negative total");check(!parsePressure("some avg10=1 avg60=0 avg300=0 total=18446744073709551616"),"overflow");
    check(!parsePressure(some+"unknown avg10=0 avg60=0 avg300=0 total=0\n"),"unknown row");check(!parsePressure(std::string(4097,'x')),"bounded pressure");
    Process a{42,1,"fixture",'S',10,200,{}},b=a;b.cpuTicks=30;
    check(processTicksPerSecond(a,b,2)==10,"process rate");b.startTicks++;check(!processTicksPerSecond(a,b,2),"PID reuse");b=a;b.pid++;check(!processTicksPerSecond(a,b,2),"PID mismatch");b=a;b.cpuTicks=1;check(!processTicksPerSecond(a,b,2),"reset");
    check(reviewProcess({42,200},{a},99)==IdentityReview::Match,"identity");check(reviewProcess({42,201},{a},99)==IdentityReview::Changed,"changed");check(reviewProcess({42,200},{},99)==IdentityReview::Missing,"missing");check(reviewProcess({42,200},{a,a},99)==IdentityReview::Ambiguous,"duplicate PID");check(reviewProcess({1,200},{},99)==IdentityReview::Protected,"init");check(reviewProcess({42,200},{a},42)==IdentityReview::Protected,"self");check(reviewProcess({42,0},{a},99)==IdentityReview::Protected,"unknown identity");
    Sample x,y;x.time=Sample::Clock::now();y.time=x.time+std::chrono::seconds(2);x.system.cpu=CpuTicks{100,50};y.system.cpu=CpuTicks{200,70};y.system.memory=Memory{100,25,{},{}};x.cores={{"cpu0",{100,50}}};y.cores={{"cpu0",{200,70}},{"cpu1",{100,20}}};
    auto r=sampleRates(x,y);check(r&&r->seconds==2&&r->cpu==80&&r->memoryUsed==75,"sample rates");check(r->cores.size()==1&&r->cores.at("cpu0")==80,"hotplug baseline");check(!sampleRates(y,x),"reverse clock");
    y.cores.push_back(y.cores.front());check(!sampleRates(x,y),"duplicate cores");y.cores.pop_back();y.time=x.time+std::chrono::seconds(31);check(!sampleRates(x,y),"stale sample");
    History history(3);for(int i=0;i<9;++i){Rates row;row.seconds=i;history.push(row);}check(history.rows().size()==3&&history.rows().front().seconds==6,"bounded rolling history");Rates detail;detail.cores.emplace("cpu0",10);history.push(detail);check(history.rows().back().cores.empty(),"aggregate history memory bound");History minimum(0);minimum.push({});minimum.push({});check(minimum.rows().size()==1,"minimum cap");History maximum(10000);for(int i=0;i<650;++i)maximum.push({});check(maximum.rows().size()==600,"maximum cap");
#if defined(__linux__)
    auto first=captureSample();std::this_thread::sleep_for(std::chrono::milliseconds(100));auto second=captureSample();auto live=sampleRates(first,second);check(live&&live->memoryUsed&&live->cpu,"live timed sample");check(!first.cores.empty(),"live core sample");
#endif
    auto pressure=collectPressure();check(!pressure.cpu||pressure.cpu->some.avg10<=100,"live PSI or unavailable");std::cout<<count<<" runtime checks passed\n";
}
