// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/volume_inspection.hpp"
#include <QCoreApplication>
#include <iostream>
#include <unistd.h>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(::geteuid()!=0){std::cerr<<"Administrator authorization required.\n";return 1;}
    if(argc!=3){std::cerr<<"Expected mounted volume and standard or extended budget.\n";return 1;}
    const auto path=QFile::decodeName(argv[1]);const auto budget=QString::fromLatin1(argv[2]);
    if((budget!="standard"&&budget!="extended")||!syspulse::cleaner::mountedVolume(path)){std::cerr<<"Not a supported mounted local filesystem.\n";return 1;}
    namespace c=syspulse::cleaner;
    c::Limits limits;limits.entries=budget=="extended"?3000000:1000000;limits.seconds=budget=="extended"?600:180;limits.allowRemoval=false;limits.collectDuplicates=false;
    const c::fs::path root=argv[1];std::error_code ec;if(c::fs::canonical(root,ec)!=root||ec){std::cerr<<"Noncanonical mount root refused.\n";return 1;}
    ::alarm(limits.seconds+15); // hard deadline for a read-only operation; no writes to roll back.
    std::atomic_bool cancel{false};
    try{const auto result=c::scan(root,{},limits,cancel);if(!result.error.empty())return 1;const auto payload=c::encodeInspection(result);if(payload.size()>4*1024*1024)return 1;std::cout<<payload.constData()<<std::endl;}
    catch(const std::exception&){return 1;}
    return 0;
}
