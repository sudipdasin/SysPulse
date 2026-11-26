// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/cleaner_page.hpp"
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTabWidget>
#include <QWidget>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
int main(int argc,char** argv){
    QApplication app(argc,argv);QTemporaryDir fixture;if(!fixture.isValid())return 1;
    const auto root=std::filesystem::path(fixture.path().toStdString());
    std::ofstream(root/"largest.iso")<<std::string(4096,'x');std::ofstream(root/"old.log")<<"123";
    std::filesystem::create_symlink(root/"largest.iso",root/"link");
    QString routed;std::unique_ptr<QWidget> page(syspulse::cleanerPage([&](const QString& id){routed=id;}));page->resize(1024,700);page->show();
    auto* mode=page->findChild<QComboBox*>("cleanerMode");auto* path=page->findChild<QLineEdit*>("cleanerRoot");auto* scan=page->findChild<QPushButton*>("cleanerScan");auto* cancel=page->findChild<QPushButton*>("cleanerCancel");auto* status=page->findChild<QLabel*>("cleanerStatus");auto* largest=page->findChild<QTableWidget*>("cleanerLargest");auto* leftovers=page->findChild<QTableWidget*>("cleanerLeftovers");auto* candidates=page->findChild<QTableWidget*>("cleanerCandidates");auto* remove=page->findChild<QPushButton*>("cleanerDelete");auto* tabs=page->findChild<QTabWidget*>("cleanerTabs");auto* minimum=page->findChild<QSpinBox*>("cleanerMinimum");
    if(!mode||!path||!scan||!cancel||!status||!largest||!leftovers||!candidates||!remove||!tabs||!minimum||mode->count()!=4||tabs->count()!=6||remove->isEnabled()||cancel->isEnabled())return 2;
    const auto await=[&](QPushButton* b){QElapsedTimer time;time.start();while(!b->isEnabled()&&time.elapsed()<18000){app.processEvents();QThread::msleep(5);}return b->isEnabled();};
    mode->setCurrentIndex(3);path->setText(fixture.path());scan->click();
    if(scan->isEnabled()||!cancel->isEnabled()||!await(scan))return 3;
    if(!status->text().contains("Scan complete")||largest->rowCount()!=2||largest->item(0,1)->text()!="largest.iso"||largest->item(0,2)->text()!=fixture.path()||leftovers->rowCount()!=1||candidates->rowCount()!=0||remove->isEnabled())return 4;
    if(page->findChild<QWidget*>("cleanerLargestChart")->accessibleName().isEmpty()||page->findChild<QWidget*>("cleanerDiskChart")->accessibleName().isEmpty())return 5;
    auto* opt=page->findChild<QCheckBox*>("cleanerDuplicateOpt");auto* dup=page->findChild<QTableWidget*>("cleanerDuplicates");auto* dupStatus=page->findChild<QLabel*>("cleanerDuplicateStatus");
    if(!opt||!dup||!dupStatus||opt->isChecked())return 13;
    std::ofstream(root/"copy1")<<std::string(1048576,'x');std::ofstream(root/"copy2")<<std::string(1048576,'x');opt->setChecked(true);scan->click();if(!await(scan)||dup->rowCount()!=2||!dupStatus->text().contains("1 byte-matched groups"))return 14;
    opt->setChecked(false);std::filesystem::remove(root/"copy1");std::filesystem::remove(root/"copy2");
    minimum->setValue(1);scan->click();if(!await(scan)||largest->rowCount()!=0)return 6;
    path->setText("relative");scan->click();if(!status->text().contains("absolute"))return 7;
    path->setText(fixture.path()+"/missing");scan->click();if(!await(scan)||!status->text().contains("Cannot open"))return 8;
    for(auto* b:page->findChildren<QPushButton*>())if(b->text()=="Preview unused dependencies")b->click();
    if(routed!="apt-preview")return 9;
    auto* inventory=page->findChild<QPushButton*>("cleanerPackageScan");auto* packageStatus=page->findChild<QLabel*>("cleanerPackageStatus");auto* packages=page->findChild<QTableWidget*>("cleanerPackages");
    if(!inventory||!packageStatus||!packages)return 10;
    inventory->click();if(!await(inventory)||packages->rowCount()==0||!packageStatus->text().contains("installed packages listed"))return 11;
    if(!std::filesystem::exists(root/"largest.iso")||!std::filesystem::exists(root/"old.log"))return 12;
    std::cout<<"Cleaner UI: four modes, async scan, three charts, paths, size filtering, failures, review routing and live package inventory passed\n";
}
