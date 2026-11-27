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
    const auto cacheDir=root/"xdg-cache";std::filesystem::create_directory(cacheDir);std::ofstream(cacheDir/"fixture.cache")<<"abc";
    qputenv("XDG_CACHE_HOME",QByteArray::fromStdString(cacheDir.string()));
    QString routed;std::unique_ptr<QWidget> page(syspulse::cleanerPage([&](const QString& id){routed=id;}));page->resize(1024,700);page->show();
    auto* mode=page->findChild<QComboBox*>("cleanerMode");auto* path=page->findChild<QLineEdit*>("cleanerRoot");auto* scan=page->findChild<QPushButton*>("cleanerScan");auto* cancel=page->findChild<QPushButton*>("cleanerCancel");auto* status=page->findChild<QLabel*>("cleanerStatus");auto* largest=page->findChild<QTableWidget*>("cleanerLargest");auto* leftovers=page->findChild<QTableWidget*>("cleanerLeftovers");auto* candidates=page->findChild<QTableWidget*>("cleanerCandidates");auto* remove=page->findChild<QPushButton*>("cleanerDelete");auto* tabs=page->findChild<QTabWidget*>("cleanerTabs");auto* minimum=page->findChild<QSpinBox*>("cleanerMinimum");
    if(!mode||!path||!scan||!cancel||!status||!largest||!leftovers||!candidates||!remove||!tabs||!minimum||mode->count()!=4||tabs->count()!=6||remove->isEnabled()||cancel->isEnabled())return 2;
    auto* quick=page->findChild<QPushButton*>("cleanerQuickReview");auto* batch=page->findChild<QPushButton*>("cleanerSelectBatch");auto* clear=page->findChild<QPushButton*>("cleanerClearBatch");auto* trash=page->findChild<QPushButton*>("cleanerTrashLargest");auto* trim=page->findChild<QPushButton*>("cleanerTrim");auto* extended=page->findChild<QCheckBox*>("cleanerExtendedScan");
    if(!quick||!batch||!clear||!trash||!trim||!extended||batch->isEnabled()||clear->isEnabled()||trash->isEnabled()||extended->isChecked())return 30;
    quick->click();if(tabs->tabText(tabs->currentIndex())!="Cleanup candidates")return 31;
    trim->click();if(routed!="trim")return 32;
    tabs->setCurrentIndex(0);
    const auto await=[&](QPushButton* b){QElapsedTimer time;time.start();while(!b->isEnabled()&&time.elapsed()<18000){app.processEvents();QThread::msleep(5);}return b->isEnabled();};
    if(path->text()!=QString::fromStdString(cacheDir.string()))return 15;
    auto* volumes=page->findChild<QComboBox*>("cleanerVolume");auto* budget=page->findChild<QComboBox*>("cleanerVolumeBudget");auto* browse=page->findChild<QPushButton*>("cleanerBrowse");
    if(!volumes||!budget||!browse)return 27;
    mode->setCurrentIndex(2);if(!volumes->isVisible()||!budget->isVisible()||path->isEnabled()||browse->isEnabled()||volumes->count()<1||path->text()!=volumes->currentData().toString()||!path->text().startsWith("/"))return 28;
    mode->setCurrentIndex(3);if(!path->isEnabled()||!browse->isEnabled()||path->text().isEmpty()||volumes->isVisible())return 29;
    mode->setCurrentIndex(0);
    scan->click();if(!await(scan)||!status->text().contains("Scan complete")||largest->rowCount()!=1||remove->isEnabled())return 16;
    std::filesystem::remove_all(cacheDir);scan->click();if(!await(scan)||!status->text().contains("does not exist")||largest->rowCount()!=0||remove->isEnabled())return 17;
    std::filesystem::create_directory_symlink(root/"missing-target",cacheDir);scan->click();if(!await(scan)||!status->text().contains("Cannot open"))return 18;
    std::filesystem::remove(cacheDir);
    auto target=root/"cache-target";std::filesystem::create_directory(target);std::ofstream(target/"test.cache")<<"1234";std::filesystem::create_directory_symlink(target,cacheDir);
    scan->click();if(!await(scan)||!status->text().contains("resolved folder")||largest->rowCount()!=1||remove->isEnabled())return 19;
    std::filesystem::remove(cacheDir);std::filesystem::remove_all(target);
    mode->setCurrentIndex(3);path->setText(fixture.path());scan->click();
    if(scan->isEnabled()||!cancel->isEnabled()||!await(scan))return 3;
    if(!status->text().contains("Scan complete")||largest->rowCount()!=2||largest->item(0,1)->text()!="largest.iso"||largest->item(0,2)->text()!=fixture.path()||leftovers->rowCount()!=1||candidates->rowCount()!=0||remove->isEnabled())return 4;
    tabs->setCurrentIndex(1);largest->selectRow(0);app.processEvents();
    if(!trash->isEnabled()||batch->isEnabled()||remove->isEnabled())return 33;
    extended->setChecked(true);if(largest->rowCount()!=0||trash->isEnabled())return 34;
    extended->setChecked(false);scan->click();if(!await(scan)||largest->rowCount()!=2)return 35;
    if(page->findChild<QWidget*>("cleanerLargestChart")->accessibleName().isEmpty()||page->findChild<QWidget*>("cleanerDiskChart")->accessibleName().isEmpty())return 5;
    path->setText(fixture.path()+"/other");if(largest->rowCount()!=0||leftovers->rowCount()!=0||remove->isEnabled()||!status->text().contains("settings changed"))return 20;
    path->setText(fixture.path());scan->click();if(!await(scan)||largest->rowCount()!=2)return 21;
    minimum->setValue(1);if(largest->rowCount()!=0||!status->text().contains("settings changed"))return 22;
    minimum->setValue(0);scan->click();if(!await(scan)||largest->rowCount()!=2)return 23;
    mode->setCurrentIndex(0);if(largest->rowCount()!=0||remove->isEnabled())return 24;
    mode->setCurrentIndex(3);if(path->text()!=fixture.path())return 25;
    scan->click();if(!await(scan)||largest->rowCount()!=2)return 26;
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
