// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/apt_status.hpp"
#include <iostream>
int main(){
    using namespace syspulse::aptstatus;
    const auto p=parse("syspulse:\n  Installed: 3.0.0~alpha3~render1\n  Candidate: 3.0.0~alpha4\n  Version table:\n");
    if(!p||p->installed!="3.0.0~alpha3~render1"||p->candidate!="3.0.0~alpha4")return 1;
    if(!parse("syspulse:\n  Installed: (none)\n  Candidate: 3.0.0\n"))return 2;
    if(!parse("syspulse:\n  Installed: 3.0.0\n  Candidate: (none)\n"))return 3;
    if(parse("  Installed: 3.0.0\n  Candidate: 3.0.1\n"))return 4;
    if(parse("syspulse:\n  Installed: 3.0.0\n"))return 5;
    if(parse("syspulse:\n  Installed: 3.0.0\n  Candidate: 3.0.1\n  Candidate: 3.0.2\n"))return 6;
    if(parse("syspulse:\n  Installed: 3.0.0\n  Candidate: $(id)\n"))return 7;
    if(parse("syspulse:\n  Installed: 3.0.0\n  Candidate: 3.0.1 extra\n"))return 8;
    if(parse(std::string(17000,'a')))return 9;
    if(!validToken("1:3.0.0-1+deb12u1")||validToken("-3.0")||validToken("3.0\r"))return 10;
    if(parse("syspulse:\n  Installed: 3.0.0\n  Candidate: 3.0.1\nsyspulse:\n"))return 11;
    std::cout<<"11 APT metadata parser checks passed\n";
}
