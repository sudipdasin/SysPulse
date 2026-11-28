// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/device_rates.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
int main(){using namespace syspulse::native;
    auto a=captureSample();auto devices=captureDevices();std::this_thread::sleep_for(std::chrono::seconds(1));auto b=captureSample();auto later=captureDevices();
    const auto rates=sampleRates(a,b);const auto io=deviceRates(devices,later);const auto psi=collectPressure();
    std::cout<<std::fixed<<std::setprecision(1)<<"SysPulse Native R3 - read-only sample\n";
    const auto print=[](const char* label,const std::optional<double>& v){std::cout<<label<<": ";if(v)std::cout<<*v<<"%";else std::cout<<"unavailable";std::cout<<'\n';};
    print("CPU",rates?rates->cpu:std::nullopt);print("Memory used",rates?rates->memoryUsed:std::nullopt);
    if(rates)std::cout<<"Matched CPU cores: "<<rates->cores.size()<<'\n';
    std::cout<<"Per-device rates (not summed; network identity best effort):\n";
    if(io)for(const auto& row:*io)std::cout<<(row.kind==DeviceKind::Disk?"Disk ":"Net ")<<row.name<<": "<<row.readBytesPerSecond/1024<<" / "<<row.writeBytesPerSecond/1024<<" KiB/s\n";
    if(!io||io->empty())std::cout<<"Unavailable\n";
    if(devices.truncated||later.truncated)std::cout<<"Device inventory capped\n";
    print("Memory stall avg10",psi.memory?std::optional<double>(psi.memory->some.avg10):std::nullopt);
    print("I/O stall avg10",psi.io?std::optional<double>(psi.io->some.avg10):std::nullopt);
    return rates?0:1;
}
