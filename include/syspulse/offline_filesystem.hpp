// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVector>
namespace syspulse::offline {
struct Device {
    QString path, uuid, type, majorMinor, size;
    bool operator==(const Device& b)const{return path==b.path&&uuid==b.uuid&&type==b.type&&majorMinor==b.majorMinor&&size==b.size;}
};
inline QVector<Device> candidates(const QByteArray& data) {
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(data,&error);
    QVector<Device> out;
    if(error.error!=QJsonParseError::NoError||!doc.isObject()||!doc.object().value("blockdevices").isArray())return out;
    static const QRegularExpression path(R"(\A/dev/[A-Za-z0-9][A-Za-z0-9._+-]*\z)"), uuid(R"(\A[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\z)"), devno(R"(\A[0-9]+:[0-9]+\z)");
    for(const auto& v:doc.object().value("blockdevices").toArray()){
        if(!v.isObject())continue;const auto o=v.toObject();
        const auto name=o.value("path").toString(), fs=o.value("fstype").toString(), id=o.value("uuid").toString(), dev=o.value("maj:min").toString(), kind=o.value("type").toString();
        if(!path.match(name).hasMatch()||!uuid.match(id).hasMatch()||!devno.match(dev).hasMatch()||(fs!="ext2"&&fs!="ext3"&&fs!="ext4")||(kind!="part"&&kind!="loop"&&kind!="disk"))continue;
        if(!o.value("ro").isBool()||o.value("ro").toBool()||!o.value("mountpoints").isArray())continue;
        bool mounted=false;for(const auto& m:o.value("mountpoints").toArray())if(!m.isNull()){mounted=true;break;}
        if(mounted||o.value("size").toString().isEmpty())continue;
        Device d{name,id,fs,dev,o.value("size").toString()};if(!out.contains(d))out.append(d);
    }
    // A duplicate UUID makes selection ambiguous (e.g. attached cloned disk).
    QVector<Device> unique;for(const auto& d:out){int matches=0;for(const auto& a:out)if(a.uuid.compare(d.uuid,Qt::CaseInsensitive)==0)++matches;if(matches==1)unique.append(d);}
    return unique;
}
inline QString objectPath(const QByteArray& output){
    const auto first=QString::fromUtf8(output).section('\n',0,0).trimmed();
    static const QRegularExpression rx(R"(\A(/org/freedesktop/UDisks2/block_devices/[A-Za-z0-9_]+):?\z)");
    const auto match=rx.match(first);return match.hasMatch()?match.captured(1):QString{};
}
inline QStringList arguments(const QString& path,bool repair){
    static const QRegularExpression rx(R"(\A/org/freedesktop/UDisks2/block_devices/[A-Za-z0-9_]+\z)");
    if(!rx.match(path).hasMatch())return {};
    return {"call","--system","--dest","org.freedesktop.UDisks2","--object-path",path,"--method",repair?"org.freedesktop.UDisks2.Filesystem.Repair":"org.freedesktop.UDisks2.Filesystem.Check","--timeout","1800","{}"};
}
inline int boolReply(const QByteArray& data){const auto v=data.trimmed();return v=="(true,)"?1:v=="(false,)"?0:-1;}
}
