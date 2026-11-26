#include "inventory.hpp"
#include <iostream>
int main() {
    sp::Browser b; std::istringstream c("brave\n"),s("Name: private\nVmRSS: 2048 kB\n"); sp::addBrowser(b,c,s);
    std::istringstream other("not-brave\n"),o("VmRSS: 8192 kB\n"); sp::addBrowser(b,other,o);
    std::istringstream truncated("brave-browser-s\n"),bad("VmRSS: -42 kB\n"); sp::addBrowser(b,truncated,bad);
    std::istringstream missing(""),empty(""); sp::addBrowser(b,missing,empty);
    if(b.count!=2 || b.rss!=2048 || b.unreadable!=2) return 1;
    for(const auto& row:sp::browserRows(b)) if(row.find("private")!=std::string::npos) return 2;
    if(!sp::browserSnapshot("/nonexistent-syspulse-test").partial) return 3;
    if(sp::capabilityRows().size()!=5 || sp::commandPresent("nonexistent-syspulse-command")) return 4;
    std::cout<<"Inventory fixtures passed\n";
}
