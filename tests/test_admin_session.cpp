#include "tux/admin_session.hpp"
#include "tux/admin_protocol.hpp"
#include <QCoreApplication>
#include <iostream>
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 int count=0;for(const auto& a:tux::actions())if(tux::admin::supported(a.id)){if(!a.admin)return 1;++count;}
 if(count!=7||tux::admin::supported("sh")||tux::admin::supported("apt-clean\ntrim")||tux::admin::supported("storage"))return 2;
 tux::Result r;r.code=0;r.output="test\nbinary";r.seconds=0.1;tux::Result decoded;
 if(!tux::admin::decode(tux::admin::encode(r),decoded)||decoded.output!=r.output||decoded.code!=0)return 3;
 if(tux::admin::decode("{\"code\":0}",decoded)||tux::admin::decode(QByteArray(400001,'x'),decoded))return 4;
 if(tux::admin::active()||tux::admin::busy()||tux::admin::execute("apt-clean").has_value()||!tux::admin::lock())return 5;
 std::cout<<"Session allowlist, bounded response validation and locked default passed\n";
}
