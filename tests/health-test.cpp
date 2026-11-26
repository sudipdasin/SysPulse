#include "health.hpp"
#include <iostream>
int main(){
    std::istringstream good("MemTotal: 4096000 kB\nMemAvailable: 2048000 kB\nSwapTotal: 1024000 kB\nSwapFree: 512000 kB\n");
    const auto m=sp::parseMemory(good);
    if(sp::memorySummary(m)!="RAM available: 2000 / 4000 MiB" || sp::swapSummary(m)!="Swap used: 500 / 1000 MiB") return 1;
    std::istringstream malformed("MemTotal: 100 kB\nMemAvailable: 101 kB\nSwapTotal: 0 kB\nSwapFree: 1 kB\n");
    const auto bad=sp::parseMemory(malformed);
    if(bad.available || bad.swapFree) return 2;
    std::istringstream missing("MemTotal: 400 kB\nMemAvailable: -1 kB\nSwapTotal: 0 kB\nSwapFree: 0 kB\n");
    const auto partial=sp::parseMemory(missing);
    if(partial.available || sp::swapSummary(partial)!="Swap used: 0 / 0 MiB") return 3;
    std::istringstream units("MemTotal: 400 MB\nMemAvailable: 2 kB trailing\n");
    const auto invalid=sp::parseMemory(units);
    if(invalid.total || invalid.available) return 4;
    std::cout<<"Memory parser fixtures passed\n";
}
