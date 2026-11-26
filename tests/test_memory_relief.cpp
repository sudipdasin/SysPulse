// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/memory_relief.hpp"
#include <iostream>
#include <sstream>
int main(){
    using namespace syspulse::relief;
    auto parse=[](const char* text,std::uint64_t size){std::istringstream in(text);return residentKiB(in,size);};
    if(parse("100 25 5",4096)!=100)return 1;
    if(parse("100 0",4096)!=0)return 2;
    if(parse("10 11",4096))return 3;
    if(parse("10 -1",4096))return 4;
    if(parse("10 1x",4096))return 5;
    if(parse("10 1",0))return 6;
    if(parse("18446744073709551615 18446744073709551615",4096))return 7;
    if(parse("100",4096))return 8;
    if(parse("18446744073709551616 1",4096))return 9;
    const auto result=trimSelf();
    if(!result.before || !result.after || *result.before==0 || *result.after==0)return 10;
    if(!result.supported && result.released)return 11;
    std::cout<<"11 memory relief fixture and live checks passed\n";
}
