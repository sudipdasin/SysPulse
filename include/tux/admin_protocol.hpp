// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
namespace tux::admin {
inline bool supported(const QString& id){
    static const QSet<QString> ids={"trim","apt-clean","journal-prune","trim-enable","zswap-enable","zswap-disable","firewall-status"};
    return ids.contains(id);
}
inline QByteArray encode(const Result& r){
    return QJsonDocument(QJsonObject{{"code",r.code},{"output",QString::fromLatin1(r.output.toBase64())},{"truncated",r.truncated},{"seconds",r.seconds}}).toJson(QJsonDocument::Compact);
}
inline bool decode(const QByteArray& bytes,Result& r){
    if(bytes.size()>400000)return false;
    QJsonParseError error;auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return false;
    auto o=doc.object();if(!o.value("code").isDouble()||!o.value("output").isString()||!o.value("truncated").isBool()||!o.value("seconds").isDouble())return false;
    const int code=o.value("code").toInt(-1);if(code<0||code>255)return false;
    auto out=QByteArray::fromBase64(o.value("output").toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(out.isNull()&&!o.value("output").toString().isEmpty())return false;
    if(out.size()>OutputLimit)return false;
    r.code=code;r.output=out;r.truncated=o.value("truncated").toBool();r.seconds=o.value("seconds").toDouble();return true;
}
}
