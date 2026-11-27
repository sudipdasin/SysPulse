// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QHash>
#include <QJsonArray>
#include <QRegularExpression>
namespace tux::admin {
inline bool supported(const QString& id){
    static const QSet<QString> ids={"trim","apt-clean","apt-refresh","apt-autoclean","journal-cap","journal-prune","trim-enable","trim-disable","zswap-enable","zswap-disable","firewall-status"};
    return ids.contains(id);
}
inline constexpr int MaxScanOutput=4*1024*1024;
inline constexpr int MaxReply=6*1024*1024;
inline constexpr int MaxRequest=16384;
// No command strings, caller environment, arbitrary paths to executables or shell.
inline QStringList typedCommand(const QStringList& fields){
    if(fields.size()!=3)return {};
    const auto& kind=fields[0];const auto& a=fields[1];const auto& b=fields[2];
    if(kind=="boost"){
        if(a!="archives"||b!="clean")return {};
        return {"apt-get","clean"};
    }
    if(kind=="update"){
        static const QRegularExpression version(R"(\A[0-9]{1,6}\.[0-9]{1,6}\.[0-9]{1,6}(?:~(?:alpha|beta)[0-9]{1,6})?(?:~render[0-9]{1,6})?\z)");
        if((a!="alpha"&&a!="beta"&&a!="stable")||!version.match(b).hasMatch())return {};
        return {"syspulse-apt-update","install",a,b}; // independently validates channel, trust and version
    }
    if(kind=="volume"){
        if(!a.startsWith('/')||a.size()>4096||a.contains(QChar::Null)||(b!="standard"&&b!="extended"))return {};
        return {"syspulse-volume-scan",a,b}; // helper independently validates exact canonical mounted local root
    }
    if(kind=="memory"){
        // Runtime-only, bounded kernel tuning. No caller-selected sysctl paths.
        static const QRegularExpression number(R"(\A(?:0|[1-9][0-9]{0,2})\z)");
        bool ok=false;const int value=b.toInt(&ok);
        if(a!="swappiness"||!number.match(b).hasMatch()||!ok||value>200)return {};
        return {"sysctl","-w","vm.swappiness="+b};
    }
    if(kind=="network"){
        if(a!="wifi"||(b!="on"&&b!="off"))return {};
        return {"nmcli","radio","wifi",b};
    }
    if(kind=="hdd"){
        static const QRegularExpression disk(R"(\A(?:sd|hd)[a-z]{1,8}\z)"),identity(R"(\A[0-9]+:[0-9]+:[0-9]+\z)"),number(R"(\A(?:0|[1-9][0-9]{0,3})\z)");
        const auto values=b.split(',');
        if(!disk.match(a).hasMatch()||values.size()!=3||!identity.match(values[0]).hasMatch()||!number.match(values[1]).hasMatch()||!number.match(values[2]).hasMatch()||values[1].toInt()>4096||values[2].toInt()>4096)return {};
        return {"syspulse-hdd-profile","apply",a,values[0],values[1],values[2]};
    }
    if(kind=="schedule"){
        static const QSet<QString> profiles={"off","daily","weekly","monthly"};
        static const QRegularExpression token(R"(\A[01]{6}\z)");
        if(!profiles.contains(a)||!token.match(b).hasMatch())return {};
        return {"syspulse-schedule-profile","apply",a,b};
    }
    if(kind=="power"){
        if(a!="profile"||(b!="balanced"&&b!="power-saver"&&b!="performance"))return {};
        return {"powerprofilesctl","set",b};
    }
    if(kind=="packages"){
        // Explicit fixed distribution package groups; no user-entered package names,
        // third-party repositories, removal, downgrade or unauthenticated packages.
        if(b!="install")return {};
        static const QHash<QString,QStringList> groups={
            {"wine",{"wine","winetricks","cabextract"}},
            {"lutris",{"lutris"}},
            {"graphics",{"vulkan-tools","mesa-utils","mesa-vulkan-drivers","gamemode"}},
            {"power",{"tlp"}},
            {"preload",{"preload"}},
            {"disk",{"gnome-disk-utility","gparted","smartmontools"}},
            {"backup",{"timeshift","deja-dup"}}
        };
        QStringList command={"apt-get","--no-remove","--assume-yes","-o","Acquire::AllowInsecureRepositories=false","-o","APT::Get::AllowUnauthenticated=false","-o","Dpkg::Options::=--force-confdef","-o","Dpkg::Options::=--force-confold"};
        if(a=="repair"){command<<"--fix-broken"<<"install";return command;}
        if(!groups.contains(a))return {};
        command<<"install";command.append(groups.value(a));return command;
    }
    if(kind=="service"){
        static const QSet<QString> verbs={"start","stop","restart","enable","disable"};
        static const QRegularExpression unit(R"(\A[A-Za-z0-9_@:.\\-]+\.service\z)");
        if(!verbs.contains(a)||b.size()>255||b.startsWith('-')||!unit.match(b).hasMatch())return {};
        return {"systemctl","--system","--no-pager","--no-ask-password",a,"--",b};
    }
    return {};
}
inline QByteArray encodeRequest(const QStringList& fields){
    if(typedCommand(fields).isEmpty())return {};
    auto bytes=QJsonDocument(QJsonArray::fromStringList(fields)).toJson(QJsonDocument::Compact);
    return bytes.size()<=MaxRequest?bytes:QByteArray();
}
inline QStringList decodeRequest(const QByteArray& bytes){
    if(bytes.size()>MaxRequest)return {};
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isArray()||doc.array().size()!=3)return {};
    QStringList result;for(const auto v:doc.array()){if(!v.isString())return {};result<<v.toString();}
    return typedCommand(result).isEmpty()?QStringList():result;
}
inline QByteArray encode(const Result& r){
    return QJsonDocument(QJsonObject{{"code",r.code},{"output",QString::fromLatin1(r.output.toBase64())},{"truncated",r.truncated},{"seconds",r.seconds}}).toJson(QJsonDocument::Compact);
}
inline bool decode(const QByteArray& bytes,Result& r,int limit=OutputLimit){
    if(bytes.size()>(limit<=OutputLimit?400000:MaxReply))return false;
    QJsonParseError error;auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return false;
    auto o=doc.object();if(!o.value("code").isDouble()||!o.value("output").isString()||!o.value("truncated").isBool()||!o.value("seconds").isDouble())return false;
    const int code=o.value("code").toInt(-1);if(code<0||code>255)return false;
    auto out=QByteArray::fromBase64(o.value("output").toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(out.isNull()&&!o.value("output").toString().isEmpty())return false;
    if(out.size()>limit)return false;
    r.code=code;r.output=out;r.truncated=o.value("truncated").toBool();r.seconds=o.value("seconds").toDouble();return true;
}
}
