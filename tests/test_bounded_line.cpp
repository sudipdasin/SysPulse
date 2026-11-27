// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/bounded_line.hpp"
#include "syspulse/pressure.hpp"
#include "syspulse/swap_activity.hpp"
#include "syspulse/system_memory.hpp"
#include <iostream>
#include <sstream>
using namespace syspulse;
int main(){
    std::string line;
    std::istringstream normal("abc\n\nlast");
    if(boundedLine<4>(normal,line)!=LineState::Line||line!="abc")return 1;
    if(boundedLine<4>(normal,line)!=LineState::Line||!line.empty())return 2;
    if(boundedLine<4>(normal,line)!=LineState::Line||line!="last")return 3;
    if(boundedLine<4>(normal,line)!=LineState::End)return 4;
    std::istringstream exact("1234\nnext\n");
    if(boundedLine<4>(exact,line)!=LineState::Line||line!="1234")return 5;
    if(boundedLine<4>(exact,line)!=LineState::Line||line!="next")return 6;
    std::istringstream huge(std::string(8*1024*1024,'x')+"\n");
    if(boundedLine<512>(huge,line)!=LineState::Invalid||!line.empty()||huge.gcount()>513)return 7;
    std::istringstream binary(std::string("a\0b\n",4));
    if(boundedLine<4>(binary,line)!=LineState::Invalid)return 8;
    std::istringstream failed("a");failed.setstate(std::ios::badbit);
    if(boundedLine<4>(failed,line)!=LineState::Invalid)return 9;
    const std::string h="Filename Type Size Used Priority\n";
    std::istringstream psi(std::string(4096,'x')+"\nsome avg10=0 avg60=0 avg300=0 total=1\n");
    if(pressure::parse(psi).some||psi.gcount()>513)return 10;
    std::istringstream swap("pswpin 0\npswpout 0\n"+std::string(8192,'x'));
    if(swap_activity::parse(swap)||swap.gcount()>4097)return 11;
    std::istringstream memory(h+std::string(8192,'x'));
    if(system_memory::activeSwap(memory)||memory.gcount()>4097)return 12;
    std::istringstream valid(h+"/swapfile file 1024 0 -2");
    if(system_memory::activeSwap(valid)!=true)return 13;
    std::cout<<"13 bounded-line and native parser integration checks passed\n";
}
