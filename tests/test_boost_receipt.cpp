#include "syspulse/boost_receipt.hpp"
#include <iostream>
int main(){
    using namespace syspulse::boostlog;
    Receipt r{"2026-11-27T09:30:00Z",{"One item removed","Some operations skipped"},true};
    auto data=encode(r);auto out=decode(data);
    if(!out||out->utc!=r.utc||out->lines!=r.lines||!out->needsReview)return 1;
    r.needsReview=false;if(decode(encode(r))->needsReview)return 2;
    for(const QByteArray bad:{"{}","[]","null","broken","{\"schema\":1,\"utc\":\"2026-11-27T09:30:00Z\",\"lines\":[3],\"needsReview\":false}","{\"schema\":1,\"utc\":\"2026-11-27T09:30:00Z\",\"lines\":[\"ok\"],\"needsReview\":\"false\"}"})if(decode(bad))return 3;
    r.utc="not a date";if(!encode(r).isEmpty())return 4;
    r.utc="2026-11-27T09:30:00Z";r.lines=QStringList{QString(1025,'x')};if(!encode(r).isEmpty())return 5;
    r.lines.clear();if(!encode(r).isEmpty())return 6;
    if(decode(QByteArray(32769,'x')))return 7;
    std::cout<<"Boost receipt roundtrip, type and size boundaries passed\n";
}
