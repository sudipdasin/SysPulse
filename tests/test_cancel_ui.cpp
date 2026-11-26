// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/window.hpp"
#include "tux/trusted_command.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <chrono>
#include <iostream>
#include <thread>
int main(int argc,char** argv) {
    QApplication app(argc,argv); QTemporaryDir temp; if(!temp.isValid()) return 1;
    const auto oldPath=qgetenv("PATH");
    auto fixture=[&](const QString& name,const QByteArray& body) {
        QFile file(temp.filePath(name));
        if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate) || file.write("#!/bin/sh\n"+body)<0) return false;
        file.close(); return file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    };
    if(!fixture("df","printf 'fixture diagnostic running\\n'\nexec /bin/sleep 30\n") ||
       !fixture("xfconf-query","printf 'fixture mutation running\\n'\n/bin/sleep 0.3\nprintf 'fixture mutation completed\\n'\n")) return 2;
    qputenv("PATH",temp.path().toUtf8()+":"+oldPath);
    tux::Window window; window.show(); app.processEvents();
    auto* status=window.findChild<QLabel*>("taskStatus");
    auto* output=window.findChild<QPlainTextEdit*>("sessionOutput");
    auto* stop=window.findChild<QPushButton*>("stopDiagnostic");
    auto wait=[&](auto condition,int ms) { QElapsedTimer timer; timer.start(); while(!condition() && timer.elapsed()<ms) {app.processEvents();std::this_thread::sleep_for(std::chrono::milliseconds(10));} return condition(); };
    auto review=[&](const QString& id) {
        QTimer::singleShot(0,[&]{for(auto* widget:QApplication::topLevelWidgets()) if(auto* box=qobject_cast<QMessageBox*>(widget)) box->done(QMessageBox::Ok);});
        window.findChild<QPushButton*>("tool_"+id)->click();
    };
    if(!stop || stop->isEnabled()) return 3;
    review("storage");
    if(!wait([&]{return output->toPlainText().contains("fixture diagnostic running");},3000) || !stop->isEnabled()) return 4;
    stop->click();
    if(!wait([&]{return status->text().startsWith("Cancelled");},3000) || stop->isEnabled() || window.findChild<QPushButton*>("sessionHistory")->text()!="History (1)") return 5;
    if(argc>1 && !window.grab().save(QString::fromLocal8Bit(argv[1]))) return 6;
    if(!fixture("df","printf 'fixture diagnostic completed\\n'\n")) return 7;
    review("storage");
    if(!wait([&]{return status->text().startsWith("Completed");},3000) || !output->toPlainText().contains("fixture diagnostic completed")) return 8;
    auto* mutation=window.findChild<QPushButton*>("tool_compositor-on");
    if(!mutation||stop->isEnabled())return 9;
    const bool trusted=!tux::trustedSystemExecutable("xfconf-query").isEmpty();
    if(mutation->isEnabled()!=trusted)return 10;
    if(!trusted)mutation->click(); // Disabled: the PATH fixture must never run.
    if(output->toPlainText().contains("fixture mutation running")||
       window.findChild<QPushButton*>("sessionHistory")->text()!="History (2)")return 11;
    // Actual mutation cancellation is exercised with dependency injection in syspulse-toolbox.
    // Never invoke the real XFCE configuration tool during the legacy UI test.
    window.close(); qputenv("PATH",oldPath);
    std::cout<<"Read-only stop, cancelled UI/history, next-task token reset and trusted mutation admission passed with isolated helpers\n";
}
