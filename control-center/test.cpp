#include "catalog.hpp"
#include "diagnosis.hpp"
#include <set>
#include <iostream>
int main(){
    std::set<std::string> groups(sp::groups.begin(),sp::groups.end()), names;
    int privileged=0;
    for(auto& t:sp::tools){if(!groups.count(t.group)||!names.insert(std::string(t.group)+":"+t.name).second)return 1;
        if(std::string(t.commands)=="@apt")++privileged;
        else if(std::string(t.commands)!="@health" && std::string(t.commands).find('@')!=std::string::npos)return 2;}
    if(privileged!=1 || !sp::findCommand("../bin/sh|sh;id|nonexistent-syspulse-test").empty())return 3;
    auto f=sp::assess(99,4,0);if(f[0].level!="CRITICAL"||f[1].level!="CRITICAL"||f[2].level!="INFO")return 4;
    f=sp::assess(-1,-1,-1);for(auto& x:f)if(x.level!="UNKNOWN")return 5;
    f=sp::assess(90,14,1);if(f[0].level!="WARNING"||f[1].level!="WARNING")return 6;
    f=sp::assess(89,15,1);if(f[0].level!="OK"||f[1].level!="OK"||f[2].level!="OK")return 7;
    std::cout<<sp::tools.size()<<" catalog entries and diagnostic boundary fixtures passed\n";
}
