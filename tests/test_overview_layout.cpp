// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/overview_layout.hpp"
#include <array>
#include <iostream>
#include <limits>

int main() {
    struct Case {int available,button,gap,expected;};
    const std::array<Case,14> cases{{
        {1240,200,8,6},{1239,200,8,3},{616,200,8,3},{615,200,8,2},
        {408,200,8,2},{407,200,8,1},{180,200,8,1},{0,200,8,1},
        {1000,0,8,1},{1000,100,-1,1},{600,100,0,6},{599,100,0,3},
        {std::numeric_limits<int>::max(),std::numeric_limits<int>::max(),8,1},
        {900,280,10,3}
    }};
    for(const auto& c:cases) {
        if(syspulse::overviewActionColumns(c.available,c.button,c.gap)!=c.expected) {
            std::cerr<<"Incorrect overview breakpoint at "<<c.available<<" pixels\n";return 1;
        }
    }
    // Check all pixel widths around real desktop breakpoints; never select an
    // incomplete row or a multi-column layout wider than its container.
    int previous=1;
    for(int width=1;width<=3000;++width) {
        const int count=syspulse::overviewActionColumns(width,200,8);
        if(6%count||count<previous||(count>1&&count*200+(count-1)*8>width))return 1;
        previous=count;
    }
    std::cout<<"14 boundary cases and 3000 width invariants passed\n";
}
