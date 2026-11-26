// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/swap_activity.hpp"
#include <iostream>
#include <limits>
int main(){
    using namespace syspulse::swap_activity;
    const auto p=[](const std::string& s){std::istringstream in(s);return parse(in);};
    auto zero=p("nr_free_pages 5\npswpin 0\npswpout 0\n");if(!zero||zero->in||zero->out)return 1;
    auto a=p("pswpout 20\npswpin 10\n");if(!a||a->in!=10||a->out!=20)return 2;
    if(p("pswpin 1\n")||p("pswpout 1\n"))return 3;
    if(p("pswpin 1\npswpin 2\npswpout 0\n"))return 4;
    if(p("pswpin -1\npswpout 0\n")||p("pswpin 18446744073709551616\npswpout 0\n"))return 5;
    if(p("pswpin 1 junk\npswpout 0\n"))return 6;
    if(p(std::string(4097,'x')+"\npswpin 1\npswpout 0\n"))return 7;
    std::string longInput;for(int i=0;i<4097;++i)longInput+="other 0\n";
    if(p(longInput+"pswpin 0\npswpout 0\n"))return 8;
    auto r=rates({10,20},{15,30},2,4096);if(!r||r->inKiB!=10||r->outKiB!=20)return 9;
    r=rates({10,20},{10,20},2,4096);if(!r||r->inKiB||r->outKiB)return 10;
    if(rates({10,20},{9,20},2,4096)||rates({10,20},{10,19},2,4096))return 11;
    if(rates({0,0},{1,1},0,4096)||rates({0,0},{1,1},0.01,4096)||rates({0,0},{1,1},3601,4096))return 12;
    if(rates({0,0},{1,1},std::numeric_limits<double>::quiet_NaN(),4096)||rates({0,0},{1,1},2,-1)||rates({0,0},{1,1},2,4097))return 13;
    r=rates({0,0},{1,2},1,65536);if(!r||r->inKiB!=64||r->outKiB!=128)return 14;
    const auto live=read();if(live){r=rates(*live,*live,1,pageSize());if(!r||r->inKiB||r->outKiB)return 15;}
    std::cout<<"15 swap activity checks passed; live counters "<<(live?"available":"unavailable")<<"\n";
}
