// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/actions.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <thread>
using namespace syspulse::native;
#ifdef SYSPULSE_ACTION_FIXTURE
namespace syspulse::native { int cleanupFixture(); }
#endif
int main(){CacheReview cache;ProcessReview proc; if(proc.terminate()||proc.prepare({1,0,"init",'S',0,1,{}}))return 1;auto empty=cache.apply();if(empty.removed)return 2;
if(getuid()==0){if(cache.scan().files||proc.prepare({2,0,"root",'S',0,1,{}}))return 3;std::cout<<"root refused; run separately as ordinary user for mutation tests\n";return 0;}
#ifdef SYSPULSE_ACTION_FIXTURE
if(int result=cleanupFixture()){std::cerr<<"cache fixture failed: "<<result<<'\n';return 12;}
std::cout<<"real generated-cache and separately scoped failed-marker fixture unlink verified; fresh, altered and personal files rejected\n";
#endif
 auto pid=fork();if(pid<0)return 4;if(!pid){for(;;)pause();}struct Guard{pid_t p;~Guard(){if(p>0){kill(p,SIGKILL);waitpid(p,nullptr,0);}}} guard{pid};std::ifstream in("/proc/"+std::to_string(pid)+"/stat");std::string row;std::getline(in,row);auto child=parseProcess(row);if(!child)return 5;auto stale=*child;++stale.startTicks;if(proc.prepare(stale))return 6;if(!proc.prepare(*child))return 7;proc.cancel();if(proc.terminate())return 8;if(!proc.prepare(*child)||!proc.terminate()||proc.terminate())return 9;int status{};for(int i=0;i<100;++i){if(waitpid(pid,&status,WNOHANG)==pid){guard.p=-1;if(!WIFSIGNALED(status)||WTERMSIG(status)!=SIGTERM)return 10;std::cout<<"real child SIGTERM verified; stale, protected, cancelled and replay actions rejected\n";return 0;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}return 11;}
