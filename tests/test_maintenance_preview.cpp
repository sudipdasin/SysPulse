// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/maintenance_preview.hpp"
#include <iostream>
#include <unistd.h>
int main(){
    using namespace syspulse::preview;namespace fs=std::filesystem;
    auto parse=[](const char* s){std::istringstream in(s);return swap(in);};
    auto v=parse("SwapTotal: 100 kB\nSwapFree: 25 kB\n");if(!v||v->total-v->free!=75)return 1;
    if(!parse("SwapTotal: 0 kB\nSwapFree: 0 kB\n"))return 2;
    if(parse("SwapTotal: 1 kB\nSwapFree: 2 kB\n"))return 3;
    if(parse("SwapTotal: 1 kB\n"))return 4;
    if(parse("SwapTotal: 1 kB\nSwapTotal: 1 kB\nSwapFree: 0 kB\n"))return 5;
    if(parse("SwapTotal: -1 kB\nSwapFree: 0 kB\n"))return 6;
    if(parse("SwapTotal: 1 MB\nSwapFree: 0 kB\n"))return 7;
    char pattern[]="/tmp/syspulse-preview-XXXXXX";auto* temp=::mkdtemp(pattern);if(!temp)return 8;
    struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{temp};
    auto dir=cleanup.p/"archives";fs::create_directory(dir);
    auto empty=archives(dir);if(!empty.complete||empty.files||empty.bytes)return 9;
    std::ofstream(dir/"a.deb")<<"1234";std::ofstream(dir/"lock")<<"ignore";
    fs::create_directory(dir/"nested.deb");std::ofstream(dir/"nested.deb"/"b.deb")<<"ignored";
    fs::create_symlink(dir/"a.deb",dir/"link.deb");
    auto full=archives(dir);if(!full.complete||full.files!=1||full.bytes!=4)return 10;
    if(archives(dir,1).complete)return 11;
    if(archives(dir/"missing").complete)return 12;
    fs::create_directory_symlink(dir,cleanup.p/"alias");if(archives(cleanup.p/"alias").complete)return 13;
    if(!fs::exists(dir/"a.deb"))return 14;
    std::cout<<"14 maintenance preview checks passed\n";
}
