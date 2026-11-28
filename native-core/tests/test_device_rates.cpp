// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/device_rates.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>
using namespace syspulse::native;
int count=0;void check(bool ok,const char* name){++count;if(!ok){std::cerr<<name<<'\n';std::exit(1);}}
int main(){DeviceSample a,b;a.time=Sample::Clock::now();b.time=a.time+std::chrono::seconds(2);a.counters={{DeviceKind::Disk,"sda","9",10,20},{DeviceKind::Network,"eth0","2:2",100,200}};b.counters={{DeviceKind::Disk,"sda","9",14,28},{DeviceKind::Network,"eth0","2:2",120,260}};
    auto r=deviceRates(a,b);check(r&&r->size()==2,"two devices");check((*r)[0].readBytesPerSecond==1024&&(*r)[0].writeBytesPerSecond==2048,"512-byte sectors");check((*r)[1].readBytesPerSecond==10&&(*r)[1].writeBytesPerSecond==30,"network bytes");
    b.counters[0].identity="10";r=deviceRates(a,b);check(r&&r->size()==1,"disk replacement");b.counters[1].identity="";check(deviceRates(a,b)->empty(),"missing identity");b.counters=a.counters;b.counters[0].read=0;check(deviceRates(a,b)->size()==1,"counter reset");b.counters=a.counters;b.counters.push_back(b.counters[0]);check(!deviceRates(a,b),"duplicate device");b.counters=a.counters;b.counters[0].name="..";check(!deviceRates(a,b),"invalid name");check(!deviceRates(a,a),"zero interval");b.time=a.time+std::chrono::seconds(31);check(!deviceRates(a,b),"stale interval");
#if defined(__linux__)
    auto first=captureDevices();std::this_thread::sleep_for(std::chrono::milliseconds(100));auto second=captureDevices();check(first.counters.size()<=256&&second.counters.size()<=256,"live cap");check(deviceRates(first,second).has_value(),"live rate calculation or unavailable rows");
#endif
    std::cout<<count<<" device rate checks passed\n";
}
