// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/cleaner.hpp"
#include <QStorageInfo>
#include <QSet>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
namespace syspulse::cleaner {
inline bool localVolume(const QStorageInfo& v){
    static const QSet<QByteArray> local={"ext2","ext3","ext4","btrfs","xfs","f2fs","jfs","reiserfs","zfs","vfat","exfat","ntfs","ntfs3","fuseblk","overlay","udf","iso9660"};
    return v.isValid()&&v.isReady()&&local.contains(v.fileSystemType())&&!v.rootPath().isEmpty();
}
inline bool mountedVolume(const QString& path){
    if(path.isEmpty()||!path.startsWith('/')||path.contains(QChar::Null))return false;
    const QStorageInfo selected(path);
    return localVolume(selected)&&selected.rootPath()==path;
}
inline QString num(std::uint64_t n){return QString::number(static_cast<qulonglong>(n));}
inline QJsonArray encodeEntries(const std::vector<Entry>& entries){
    QJsonArray out;for(const auto& e:entries){const auto raw=e.path.native();out.append(QJsonObject{{"path",QString::fromLatin1(QByteArray(raw.data(),static_cast<int>(raw.size())).toBase64())},{"size",num(e.bytes)}});}return out;
}
inline QByteArray encodeInspection(const Result& r){
    QJsonArray cats;for(auto v:r.categories)cats.append(num(v));
    const auto raw=r.resolvedRoot.native();
    return QJsonDocument(QJsonObject{{"schema",1},{"root",QString::fromLatin1(QByteArray(raw.data(),static_cast<int>(raw.size())).toBase64())},{"categories",cats},{"largest",encodeEntries(r.largest)},{"review",encodeEntries(r.review)},
       {"visited",num(r.visited)},{"files",num(r.files)},{"permission",num(r.permissionDenied)},{"other",num(r.otherSkipped)},{"excluded",num(r.mountExcluded)},
       {"logical",num(r.logical)},{"allocated",num(r.allocated)},{"aliases",num(r.hardlinkAliases)},{"used",num(r.volume.used)},{"available",num(r.volume.available)},{"reserved",num(r.volume.reserved)},{"known",r.volume.known},{"complete",r.complete},{"limited",r.limited},{"overflow",r.accountingOverflow}}).toJson(QJsonDocument::Compact);
}
inline bool decodeInspection(const QByteArray& bytes,const fs::path& root,Result& output){
    if(bytes.size()>4*1024*1024)return false;
    QJsonParseError err;const auto doc=QJsonDocument::fromJson(bytes,&err);
    if(err.error!=QJsonParseError::NoError||!doc.isObject())return false;
    const auto obj=doc.object();
    const auto pathOf=[](const QJsonValue& v){const auto raw=QByteArray::fromBase64(v.toString().toLatin1());return raw.contains('\0')?fs::path{}:fs::path(std::string(raw.constData(),static_cast<size_t>(raw.size())));};
    if(obj.value("schema").toInt()!=1||pathOf(obj.value("root"))!=root)return false;
    Result r;r.resolvedRoot=root;r.readOnly=true;
    const auto n=[&](const QJsonValue& value,std::uint64_t& dest){bool ok=false;const auto parsed=value.toString().toULongLong(&ok);if(ok)dest=parsed;return ok;};
    const auto small=[&](const char* key,unsigned& dest){std::uint64_t v=0;if(!n(obj.value(key),v)||v>3000000)return false;dest=static_cast<unsigned>(v);return true;};
    if(!small("visited",r.visited)||!small("files",r.files)||!small("permission",r.permissionDenied)||!small("other",r.otherSkipped)||!small("excluded",r.mountExcluded)||!small("aliases",r.hardlinkAliases)||r.files>r.visited)return false;
    r.skipped=r.permissionDenied+r.otherSkipped+r.mountExcluded;
    if(!n(obj.value("logical"),r.logical)||!n(obj.value("allocated"),r.allocated)||!n(obj.value("used"),r.volume.used)||!n(obj.value("available"),r.volume.available)||!n(obj.value("reserved"),r.volume.reserved))return false;
    const auto cats=obj.value("categories").toArray();if(cats.size()!=Count)return false;for(int i=0;i<Count;++i)if(!n(cats[i],r.categories[i]))return false;
    const auto entries=[&](const char* key,std::vector<Entry>& dest){const auto arr=obj.value(key).toArray();if(!obj.value(key).isArray()||arr.size()>250)return false;
        for(const auto& item:arr){const auto row=item.toObject();Entry e;e.path=pathOf(row.value("path"));if(!inside(e.path,root)||e.path.lexically_normal()!=e.path||!n(row.value("size"),e.bytes))return false;dest.push_back(std::move(e));}return true;};
    if(!entries("largest",r.largest)||!entries("review",r.review))return false;
    for(const auto& key:{"known","complete","limited","overflow"})if(!obj.value(key).isBool())return false;
    r.volume.known=obj.value("known").toBool();r.limited=obj.value("limited").toBool();r.accountingOverflow=obj.value("overflow").toBool();r.complete=obj.value("complete").toBool()&&!r.limited&&!r.permissionDenied&&!r.otherSkipped&&!r.accountingOverflow;
    output=std::move(r);return true;
}
}
