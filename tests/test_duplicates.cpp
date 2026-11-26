// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/duplicates.hpp"
#include <fstream>
#include <iostream>
int main(){
    using namespace syspulse::cleaner;
    char temp[]="/tmp/syspulse-dupes-XXXXXX";if(!::mkdtemp(temp))return 1;
    struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{temp};
    auto root=cleanup.p;const std::string data(1048576,'A');
    std::ofstream(root/"a")<<data;std::ofstream(root/"b")<<data;std::ofstream(root/"c")<<std::string(1048576,'B');
    std::ofstream(root/"d")<<data;fs::create_hard_link(root/"d",root/"d-link");
    fs::create_symlink(root/"a",root/"link");std::ofstream(root/"small")<<"x";
    std::atomic_bool cancel{false};Limits lim;lim.collectDuplicates=true;
    auto s=scan(root,{},lim,cancel);
    if(!s.complete||s.files!=6||s.duplicateInput.size()!=3||s.hardlinkAliases!=1||s.logical!=4*1048576+1)return 2;
    auto r=duplicates(s.duplicateInput,cancel);
    if(r.groups.size()!=1||r.groups[0].files.size()!=2||r.logicalExtra!=1048576||r.limited||r.cancelled||r.bytesRead>256ULL*1024*1024)return 3;
    r=duplicates(s.duplicateInput,cancel,65536);if(!r.limited||!r.groups.empty())return 4;
    cancel=true;r=duplicates(s.duplicateInput,cancel);if(!r.cancelled||!r.groups.empty())return 5;cancel=false;
    std::ofstream(root/"b",std::ios::app)<<"changed";r=duplicates(s.duplicateInput,cancel);if(!r.groups.empty()||!r.unverified)return 6;
    fs::remove(root/"b");fs::create_symlink(root/"a",root/"b");r=duplicates(s.duplicateInput,cancel);if(!r.groups.empty())return 7;
    if(!fs::exists(root/"a")||!fs::exists(root/"d-link"))return 8;
    std::cout<<"8 duplicate/allocated accounting fixtures passed; full byte comparison, links, changed files, budget and cancellation\n";
}
