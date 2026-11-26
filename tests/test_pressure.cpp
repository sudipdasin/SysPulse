// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/pressure.hpp"
#include <iostream>
using namespace syspulse::pressure;
int main(){
    auto p=[](const std::string& s){std::istringstream in(s);return parse(in);};
    const std::string good="some avg10=12.50 avg60=0.20 avg300=100.00 total=18446744073709551615\n";
    auto a=p(good+"full avg10=0.00 avg60=0.00 avg300=0.00 total=0\n");
    if(!a.some||!a.full||a.some->avg10!=12.5||a.some->avg300!=100||a.some->total!=UINT64_MAX)return 1;
    if(p(good+good).some)return 2;
    if(p("some avg10=100.01 avg60=0 avg300=0 total=1\n").some)return 3;
    if(p("some avg10=-1 avg60=0 avg300=0 total=1\n").some)return 4;
    if(p("some avg10=nan avg60=0 avg300=0 total=1\n").some)return 5;
    if(p("some avg10=1e1 avg60=0 avg300=0 total=1\n").some)return 6;
    if(p("some avg10=0 avg60=0 avg300=0 total=18446744073709551616\n").some)return 7;
    if(p("some avg10=0 avg60=0 avg300=0 total=1 junk\n").some)return 8;
    if(p("some avg10=0 avg60=0 total=1\n").some)return 9;
    if(p(std::string(513,'x')+"\n"+good).some)return 10;
    if(p("").some||p("").full)return 11;
    if(!p(good).some||p(good).full)return 12;
    if(percentage("1.")||percentage(".5")||percentage("1.1234567"))return 13;
    for(const auto& sample:measure())for(auto row:{sample.some,sample.full})if(row&&(row->avg10<0||row->avg10>100))return 14;
    std::cout<<"14 pressure parser and live shape checks passed\n";
}
