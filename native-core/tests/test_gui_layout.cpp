// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/gui_layout.hpp"
#include <cstdlib>
#include <iostream>
using namespace syspulse::native;
int main(){unsigned checks=0;auto check=[&](bool pass){++checks;if(!pass){std::cerr<<"Layout assertion "<<checks<<" failed\n";std::exit(1);}};
for(int w=700;w<=2560;++w){GuiLayout l(w);check(l.content>=640&&l.sidebar+l.content==w);check(l.expanded()==(w>=1100));for(std::size_t i=0;i<guiPages.size();++i){const int y=l.navY(i);check(l.navHit(l.sidebar/2,y+18)==guiPages[i].page);check(!l.navHit(l.sidebar/2,y+36));check(!l.navHit(l.sidebar,y+18));check(y+36<=558);}}
for(int w=700;w<=2000;w+=5)for(int h=620;h<=1100;h+=10){const int content=GuiLayout(w).content;const int rows=performanceRows(content,h);check(rows>=1&&rows<=12);check(metricsBottom(content)+64+rows*27<=h-135);}
check(GuiLayout(640).sidebar==0);check(!GuiLayout(640).navHit(20,100));check(!GuiLayout(700).navHit(10,0));
std::cout<<checks<<" geometry and hit-target checks passed\n";}
