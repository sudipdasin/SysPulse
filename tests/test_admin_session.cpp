#include "tux/admin_session.hpp"
#include "tux/admin_protocol.hpp"
#include <QCoreApplication>
#include <iostream>
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 int count=0;for(const auto& a:tux::actions())if(tux::admin::supported(a.id)){if(!a.admin)return 1;++count;}
 if(count!=13||!tux::admin::supported("weekly-clean-enable")||!tux::admin::supported("weekly-clean-disable")||!tux::admin::supported("apt-refresh")||!tux::admin::supported("apt-autoclean")||!tux::admin::supported("journal-cap")||!tux::admin::supported("trim-disable")||tux::admin::supported("sh")||tux::admin::supported("apt-clean\ntrim")||tux::admin::supported("storage"))return 2;
 tux::Result r;r.code=0;r.output="test\nbinary";r.seconds=0.1;tux::Result decoded;
 if(!tux::admin::decode(tux::admin::encode(r),decoded)||decoded.output!=r.output||decoded.code!=0)return 3;
 if(tux::admin::decode("{\"code\":0}",decoded)||tux::admin::decode(QByteArray(400001,'x'),decoded))return 4;
 if(tux::admin::active()||tux::admin::busy()||tux::admin::execute("apt-clean").has_value()||!tux::admin::lock())return 5;
 using namespace tux::admin;
 const QStringList update={"update","alpha","3.0.0~alpha26~render1"},volume={"volume","/","standard"},service={"service","restart","test.service"};
 const QStringList memory={"memory","swappiness","120"},packages={"packages","wine","install"};
 const QStringList wifi={"network","wifi","on"},power={"power","profile","balanced"};
 for(const auto& request:{update,volume,service,memory,packages,wifi,power})if(decodeRequest(encodeRequest(request))!=request||typedCommand(request).isEmpty())return 6;
 for(const auto& invalid:QVector<QStringList>{{"update","alpha","--bad"},{"update","unknown","3.0.0"},{"volume","relative","extended"},{"volume","/","delete"},{"service","edit","x.service"},{"service","start","--evil.service"},{"service","start","x.service;sh"},{"service","start","*.service"},{"sh","-c","id"},{"memory","swappiness","201"},{"memory","swappiness","-1"},{"memory","vm.other","60"},{"memory","swappiness","060"},{"packages","--bad","install"},{"packages","wine","remove"},{"network","wifi","reset"},{"power","profile","turbo"}})if(!typedCommand(invalid).isEmpty()||!encodeRequest(invalid).isEmpty())return 7;
 if(typedCommand(memory)!=QStringList{"sysctl","-w","vm.swappiness=120"}||!typedCommand(packages).contains("--no-remove")||!typedCommand(packages).contains("wine"))return 10;
 r.output=QByteArray(1024*1024,'a');if(!decode(encode(r),decoded,MaxScanOutput)||decoded.output!=r.output||decode(encode(r),decoded))return 8;
 if(!decodeRequest("[\"volume\",123,\"standard\"]").isEmpty()||!decodeRequest(QByteArray(MaxRequest+1,'x')).isEmpty())return 9;
 std::cout<<"Session allowlist, typed operation restrictions, bounded response validation and locked default passed\n";
}
