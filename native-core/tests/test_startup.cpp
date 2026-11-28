// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/startup.hpp"
#include <iostream>
#if defined(__linux__)
#include <unistd.h>
#endif
using namespace syspulse::native;
#ifdef SYSPULSE_STARTUP_FIXTURE
namespace syspulse::native {int startupFixture();}
#endif
int main(){StartupReview s;if(s.apply()||s.prepare({"../bad.desktop",false}))return 1;
#if defined(__linux__)
if(getuid()==0&&(s.scan().complete||s.prepare({"fixture.desktop",false})))return 2;
#endif
std::string a="Package: foo\nArchitecture: amd64\nStatus: install ok installed\n\n";auto p=parsePackageStatus(a+"Package: bar\nArchitecture: all\nStatus: install ok unpacked\n\nPackage: old\nArchitecture: all\nStatus: deinstall ok config-files\n");if(!p.complete||p.installed!=1||p.incomplete!=1||p.residual!=1)return 3;if(parsePackageStatus(a+a).complete||parsePackageStatus("Package: broken\n").complete)return 4;if(!parsePackageStatus(a+"Package: foo\nArchitecture: i386\nStatus: hold ok installed\n").complete)return 5;
#ifdef SYSPULSE_STARTUP_FIXTURE
if(int r=startupFixture()){std::cerr<<"fixture failed "<<r<<'\n';return 6;}std::cout<<"Actual user startup disable and restore, collision and changed identity refusal passed\n";
#endif
std::cout<<"Package audit fixtures and startup gates passed\n";
}
