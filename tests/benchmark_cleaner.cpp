// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/cleaner.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
int main(){
    using namespace syspulse::cleaner;
    if(::geteuid()==0){std::cerr<<"Benchmark must run as ordinary user\n";return 1;}
    char tmp[]="/tmp/syspulse-benchmark-XXXXXX";
    if(!::mkdtemp(tmp))return 1;
    struct Cleanup {fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{tmp};
    const auto root=cleanup.p,cache=root/".cache";
    fs::create_directory(cache);
    std::atomic_bool cancel{false};
    Limits limits;limits.entries=300000;limits.seconds=90;limits.top=250;limits.allowRemoval=false;
    using Clock=std::chrono::steady_clock;
    std::cout<<std::fixed<<std::setprecision(6);
    unsigned previous=0;
    for(unsigned count:{10000u,50000u,250000u}){
        for(unsigned i=previous;i<count;++i){
            const auto dir=cache/std::to_string(i/100);
            if(i%100==0)fs::create_directory(dir);
            std::ofstream out(dir/(std::to_string(i)+".bin"),std::ios::binary);
            out<<"0123456789abcdef";
            if(!out)return 2;
        }
        previous=count;
        for(unsigned rep=0;rep<3;++rep){
            const auto start=Clock::now();
            const auto result=scan(root,cache,limits,cancel);
            const double elapsed=std::chrono::duration<double>(Clock::now()-start).count();
            if(!result.complete||result.files!=count||result.logical!=16ull*count||
               !result.candidates.empty()||elapsed<=0||result.permissionDenied||result.otherSkipped||
               result.mountExcluded||result.accountingOverflow)return 3;
            std::cout<<"{\"files\":"<<count<<",\"repeat\":"<<rep+1<<",\"seconds\":"<<elapsed
                     <<",\"files_per_second\":"<<count/elapsed<<",\"visited\":"<<result.visited
                     <<",\"logical_bytes\":"<<result.logical<<",\"complete\":true}\n"<<std::flush;
        }
    }
    limits.entries=25000;
    auto bounded=scan(root,cache,limits,cancel);
    if(bounded.complete||!bounded.limited||bounded.visited>limits.entries)return 4;
    std::cout<<"{\"guard\":\"entry_limit\",\"visited\":"<<bounded.visited<<",\"passed\":true}\n";
    cancel=true;auto stopped=scan(root,cache,limits,cancel);
    if(stopped.complete||!stopped.cancelled)return 5;
    std::cout<<"{\"guard\":\"pre_cancel\",\"passed\":true}\n";
    return 0;
}
