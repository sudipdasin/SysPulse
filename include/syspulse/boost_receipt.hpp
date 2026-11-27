// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <optional>
namespace syspulse::boostlog {
// Local display-only receipt. Never used as authorization, a plan or execution input.
struct Receipt {QString utc;QStringList lines;bool needsReview=false;};
inline bool valid(const Receipt& r){
    const auto stamp=QDateTime::fromString(r.utc,Qt::ISODate);
    if(!stamp.isValid()||!r.utc.endsWith('Z')||r.utc.size()>32||r.lines.isEmpty()||r.lines.size()>16)return false;
    int total=0;
    for(const auto& line:r.lines){
        if(line.isEmpty()||line.size()>1024||line.contains(QChar::Null))return false;
        total+=line.size();
    }
    return total<=8192;
}
inline QByteArray encode(const Receipt& r){
    if(!valid(r))return {};
    return QJsonDocument(QJsonObject{{"schema",1},{"utc",r.utc},{"lines",QJsonArray::fromStringList(r.lines)},{"needsReview",r.needsReview}}).toJson(QJsonDocument::Compact);
}
inline std::optional<Receipt> decode(const QByteArray& bytes){
    if(bytes.isEmpty()||bytes.size()>32768)return {};
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return {};
    const auto obj=doc.object();
    if(obj.size()!=4||obj.value("schema").toInt(-1)!=1||!obj.value("utc").isString()||!obj.value("lines").isArray()||!obj.value("needsReview").isBool())return {};
    Receipt r{obj.value("utc").toString(),{},obj.value("needsReview").toBool()};
    for(const auto& v:obj.value("lines").toArray()){
        if(!v.isString())return {};
        r.lines<<v.toString();
    }
    return valid(r)?std::optional<Receipt>(r):std::nullopt;
}
}
