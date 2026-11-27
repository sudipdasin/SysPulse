#include "syspulse/volume_inspection.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    namespace c=syspulse::cleaner;
    c::Result r;r.resolvedRoot="/";r.visited=5;r.files=1;r.complete=true;r.logical=123;r.categories[c::Other]=123;
    c::Entry e;e.path="/test";e.bytes=123;e.removable=true;r.largest.push_back(e);
    c::Result decoded;
    if(!c::decodeInspection(c::encodeInspection(r),"/",decoded)||decoded.largest.size()!=1||decoded.largest[0].removable||!decoded.readOnly||!decoded.candidates.empty()||decoded.logical!=123)return 1;
    if(c::decodeInspection(c::encodeInspection(r),"/other",decoded))return 2;
    r.largest[0].path="/../outside";if(c::decodeInspection(c::encodeInspection(r),"/",decoded))return 3;
    if(c::mountedVolume("/proc")||c::mountedVolume("/sys")||c::mountedVolume("/not-a-real-mount"))return 4;
    if(!c::mountedVolume("/"))return 5;
    std::cout<<"Read-only inspection encoding, path guards and mounted root selection passed\n";
}
