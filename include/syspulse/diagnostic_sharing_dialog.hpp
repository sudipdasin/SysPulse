// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/diagnostic_reporting.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QTimer>
#include <QVBoxLayout>
#ifndef SYSPULSE_REPORT_ENDPOINT
#define SYSPULSE_REPORT_ENDPOINT ""
#endif
namespace syspulse::diagnostic {
inline QUrl endpoint(){const QUrl u(QStringLiteral(SYSPULSE_REPORT_ENDPOINT));return u.isValid()&&u.scheme()=="https"&&!u.host().isEmpty()&&u.userInfo().isEmpty()&&!u.hasQuery()&&!u.hasFragment()?u:QUrl{};}
inline void reviewSharing(QWidget* parent,const QVector<tux::Result>& results){
    QDialog dialog(parent);dialog.setWindowTitle("Optional diagnostic sharing · SysPulse");dialog.resize(720,620);
    auto* layout=new QVBoxLayout(&dialog);
    auto* info=new QLabel("Choose what to disclose. Sharing is optional and off by default. No raw logs, personal paths, account, device IDs, IP addresses, timestamps or hardware fingerprints are included. A network provider can still observe connection metadata. A configured relay would publish the reviewed structured report to the SysPulse repository via a service account.",&dialog);
    info->setWordWrap(true);layout->addWidget(info);
    auto* level=new QComboBox(&dialog);level->setObjectName("diagnosticShareLevel");level->addItems({"Off — do not share","Minimum — aggregate outcome counts","Medium — counts and fixed catalog check outcomes","Maximum — medium plus allowlisted imported audit checks"});layout->addWidget(level);
    auto* import=new QPushButton("Import local SHARE-report.json (optional)",&dialog);layout->addWidget(import);
    auto* preview=new QPushButton("Generate exact report preview",&dialog);layout->addWidget(preview);
    auto* text=new QPlainTextEdit(&dialog);text->setReadOnly(true);text->setObjectName("diagnosticSharePreview");layout->addWidget(text,1);
    auto* status=new QLabel("No report collected or sent.",&dialog);status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);
    auto* consent=new QCheckBox("I reviewed this exact report and consent to sharing it with the public repository.",&dialog);consent->setObjectName("diagnosticShareConsent");layout->addWidget(consent);
    auto* save=new QPushButton("Save reviewed report locally…",&dialog);auto* send=new QPushButton("Send reviewed report to repository",&dialog);auto* close=new QPushButton("Close without sharing",&dialog);
    save->setEnabled(false);send->setEnabled(false);layout->addWidget(save);layout->addWidget(send);layout->addWidget(close);
    auto* manager=new QNetworkAccessManager(&dialog);const auto target=endpoint();QMap<QString,QString> imported;QByteArray body;bool inFlight=false;
    const auto refresh=[&]{const bool ready=!inFlight&&level->currentIndex()>0&&!body.isEmpty()&&consent->isChecked();save->setEnabled(ready);send->setEnabled(ready&&!target.isEmpty());};
    const auto clear=[&]{body.clear();text->clear();consent->setChecked(false);refresh();};
    QObject::connect(level,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&](int){clear();status->setText("Selection changed; generate a new preview.");});
    QObject::connect(import,&QPushButton::clicked,&dialog,[&]{
        const auto path=QFileDialog::getOpenFileName(&dialog,"Select local audit report",{},"JSON (*.json)");if(path.isEmpty())return;
        clear();imported.clear();QFile file(path);bool valid=false;
        if(file.open(QIODevice::ReadOnly))imported=importAudit(file.read(512*1024+1),valid);
        status->setText(valid?"Imported recognized check statuses only. All facts, output and unknown fields were discarded.":"Audit rejected: unsupported, oversized or malformed report. Nothing imported.");
    });
    QObject::connect(preview,&QPushButton::clicked,&dialog,[&]{
        clear();if(level->currentIndex()==0){status->setText("Sharing is off.");return;}
        const auto checks=snapshot(results,imported);
        body=QJsonDocument(payload(checks,level->currentIndex())).toJson(QJsonDocument::Compact);
        text->setPlainText(QString::fromUtf8(body));status->setText(advice(checks)+(target.isEmpty()?"\nRepository relay is not configured. Local save only.":"\nDestination: "+target.toString()));refresh();
    });
    QObject::connect(consent,&QCheckBox::toggled,&dialog,[&](bool){refresh();});
    QObject::connect(save,&QPushButton::clicked,&dialog,[&]{
        if(body.isEmpty()||!consent->isChecked()||inFlight)return;
        const auto path=QFileDialog::getSaveFileName(&dialog,"Save reviewed structured report","syspulse-share.json","JSON (*.json)");if(path.isEmpty())return;
        QSaveFile file(path);if(file.open(QIODevice::WriteOnly)&&file.write(body)==body.size()&&file.commit())status->setText("Report saved locally. Nothing transmitted.");else status->setText("Could not save report.");
    });
    QObject::connect(send,&QPushButton::clicked,&dialog,[&]{
        if(body.isEmpty()||!consent->isChecked()||inFlight||target.isEmpty())return;
        if(QMessageBox::question(&dialog,"Confirm repository submission","Send the exact reviewed report through the configured HTTPS relay? The report may be publicly visible and cannot be guaranteed anonymous at the network layer.",QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
        inFlight=true;refresh();level->setEnabled(false);import->setEnabled(false);preview->setEnabled(false);consent->setEnabled(false);
        QNetworkRequest request(target);request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::Manual);request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,QNetworkRequest::Manual);request.setHeader(QNetworkRequest::UserAgentHeader,"SysPulse-report/1");
        auto* reply=manager->post(request,body);reply->setReadBufferSize(4096);QTimer::singleShot(15000,reply,[reply]{if(!reply->isFinished())reply->abort();});
        QObject::connect(reply,&QNetworkReply::finished,&dialog,[&,reply]{
            const auto code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto response=QJsonDocument::fromJson(reply->read(4096)).object();
            const bool accepted=reply->error()==QNetworkReply::NoError&&code==201&&response.value("accepted").toBool();
            reply->deleteLater();inFlight=false;level->setEnabled(true);import->setEnabled(true);preview->setEnabled(true);consent->setEnabled(true);consent->setChecked(false);refresh();
            status->setText(accepted?"Repository relay confirmed receipt. No automatic system change will be applied.":"Submission not confirmed. Do not assume receipt; no automatic retry.");
        });
    });
    QObject::connect(close,&QPushButton::clicked,&dialog,&QDialog::reject);dialog.exec();
}
}
