#include "syspulse/diagnostic_reporting.hpp"
#include <QCoreApplication>
#include <QJsonDocument>
#include <iostream>
using namespace syspulse::diagnostic;
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 QMap<QString,QString> data{{"preflight/storage","PREFLIGHT_ONLY"},{"audit/catalog/devices","PASS"},{"audit/Cleaner/quick-cache","DISCREPANCY"},{"audit/launcher/Desktop/Desktop settings/xfce4-panel","MISSING"},{"run/memory","FAILED"},{"evil/private","PASS"},{"audit/catalog/memory","alice@example.test"}};
 if(!payload(data,0).isEmpty())return 1;
 const auto min=payload(data,1),med=payload(data,2),max=payload(data,3);
 if(min.contains("checks")||med.value("checks").toObject().size()!=3||max.value("checks").toObject().size()!=5)return 2;
 if(QJsonDocument(max).toJson().contains("alice")||QJsonDocument(max).toJson().contains("evil"))return 3;
 const auto fixture=QByteArray(R"JSON({"facts":{"reference_commit":"3beb151833b9faf2ec9266e67b0123f2cab91f5d","audit_version":"2.0.0","hostname":"secret-host"},"results":[{"section":"catalog","id":"storage","status":"PASS","detail":"secret /home/private"},{"section":"unknown","id":"serial-123","status":"PASS"}]})JSON");
 bool valid=false;auto imported=importAudit(fixture,valid);
 if(!valid||imported.size()!=1||imported.value("audit/catalog/storage")!="PASS")return 4;
 if(QJsonDocument(payload(imported,3)).toJson().contains("secret"))return 5;
 importAudit(QByteArray(512*1024+1,'x'),valid);if(valid)return 6;
 tux::Result r;r.code=0;if(sessionState(r,tux::action("apt-clean"))!="REVIEW")return 7;
 r.cancelled=true;if(sessionState(r,tux::action("storage"))!="CANCELLED")return 8;
 std::cout<<"Privacy allowlist, three levels, import filtering and outcomes passed\n";
 return 0;
}
