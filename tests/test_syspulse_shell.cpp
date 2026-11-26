// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/window.hpp"
#include "syspulse/performance.hpp"
#include "tux/core.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QAction>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QTableView>
#include <QElapsedTimer>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void require(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
void events(int ms) {QElapsedTimer clock;clock.start();while(clock.elapsed()<ms){qApp->processEvents();QThread::msleep(5);}}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setApplicationName("SysPulse");app.setApplicationVersion(tux::Version);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    syspulse::applyPrecisionTheme();
    QTemporaryDir preferences;
    if(!preferences.isValid())return 1;
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,preferences.path());
    try {
        syspulse::Window window(false);window.resize(1366,768);window.show();events(50);
        require(window.navigationCount()==13,"existing navigation plus Control Panel");
        require(window.currentPageId()=="overview","overview is the initial destination");
        const auto idleTimers = [&window] {
            int active = 0;
            for (const auto* timer : window.findChildren<QTimer*>()) if (timer->isActive()) ++active;
            return active;
        };
        require(idleTimers()==0,"sampling-disabled idle shell has no recurring admission or other polling timer");
        auto* reportTable=window.findChild<QTableWidget*>("sessionReports");
        require(reportTable&&reportTable->rowCount()==0,"opening the app does not manufacture diagnostic results");
        const QStringList destinations={"overview","performance","storage","control","management","network","desktop","security","schedules","recovery","reports","experimental","settings"};
        for(const auto& id:destinations) {
            require(window.navigate(id),"navigation target available");events(25);
            require(window.currentPageId()==id,"navigation updates actual workspace");
        }
        require(!window.navigate("missing")&&window.currentPageId()=="settings","invalid navigation preserves current context");
        require(idleTimers()==0,"returning to an idle non-monitoring workspace leaves no recurring shell polling timer");
        require(reportTable->rowCount()==0,"page inspection never records fictitious changes");
        auto* tabs=window.findChild<QTabWidget*>("syspulsePerformanceTabs");
        require(tabs&&tabs->count()==6,"summary and four resource pages plus preserved processes");
        require(tabs->tabText(5)=="Processes","process manager is attached to performance");
        auto* search=window.findChild<QLineEdit*>("globalToolSearch");
        require(search,"global tool search is accessible");search->setText("Package health");QMetaObject::invokeMethod(search,"returnPressed");events(30);
        require(window.currentPageId()=="storage","global tool search routes a real catalog tool");
        auto* storageTabs=window.findChild<QTabWidget*>("storageWorkspaceTabs");
        auto* securityTabs=window.findChild<QTabWidget*>("securityWorkspaceTabs");
        require(storageTabs&&storageTabs->count()==2&&storageTabs->tabText(1)=="Disk tools","storage tools extend the existing workspace");
        require(securityTabs&&securityTabs->count()==2,"security tools extend the existing workspace");
        storageTabs->setCurrentIndex(1);search->setText("Package health");QMetaObject::invokeMethod(search,"returnPressed");
        require(storageTabs->currentIndex()==0,"existing search restores the diagnostic tab");
        int integrationCount=0;
        for(auto* b:window.findChildren<QPushButton*>())if(b->property("systemToolCommand").isValid())++integrationCount;
        require(integrationCount==63,"existing and Control Panel optional tool integrations are present");
        search->setText("Storage & cleanup / Disk management");
        QMetaObject::invokeMethod(search,"returnPressed");events(20);
        require(window.currentPageId()=="storage"&&storageTabs->currentIndex()==1,"qualified search reveals disk tool tab without launching");
        search->setText("Security & privacy / Firewall");
        QMetaObject::invokeMethod(search,"returnPressed");events(20);
        require(window.currentPageId()=="security"&&securityTabs->currentIndex()==1,"qualified search distinguishes duplicate launcher names");
        require(reportTable->rowCount()==0,"launcher search never executes a diagnostic or records a mutation");
        require(window.navigate("control"),"Control Panel accessible");
        auto* controlTabs=window.findChild<QTabWidget*>("controlPanelTabs");
        require(controlTabs&&controlTabs->count()==11,"all Control Panel categories, index, care and maintenance present");
        auto* care=window.findChild<QPushButton*>("openSystemCare");
        auto* scan=window.findChild<QPushButton*>("runCareScan");
        auto* findings=window.findChild<QTableWidget*>("careFindings");
        auto* careReview=window.findChild<QPushButton*>("reviewCareFinding");
        require(care&&scan&&findings&&careReview&&findings->rowCount()==0&&!careReview->isEnabled(),"care center starts unscanned and requires selection");
        window.navigate("overview");care->click();require(controlTabs->tabText(controlTabs->currentIndex())=="System care","overview opens care center");
        scan->click();require(findings->rowCount()==6&&!careReview->isEnabled(),"read-only scan populates six measured findings");
        int ramRow=-1;for(int i=0;i<findings->rowCount();++i)if(findings->item(i,0)->text()=="Available RAM")ramRow=i;
        require(ramRow>=0,"RAM finding present");findings->selectRow(ramRow);
        require(careReview->isEnabled(),"review enabled only after selection");careReview->click();
        require(controlTabs->currentIndex()==0,"care routes to existing memory optimizer");
        require(reportTable->rowCount()==0,"care scan and navigation never manufacture diagnostic reports");
        auto* maintenance=window.findChild<QPushButton*>("openMaintenanceCenter");
        auto* baseline=window.findChild<QPushButton*>("maintenanceBaseline");
        auto* recheck=window.findChild<QPushButton*>("maintenanceRecheck");
        auto* comparison=window.findChild<QTableWidget*>("maintenanceComparison");
        require(maintenance&&baseline&&recheck&&comparison&&!recheck->isEnabled(),"maintenance requires baseline");
        window.navigate("overview");maintenance->click();
        require(controlTabs->tabText(controlTabs->currentIndex())=="Maintenance","overview opens maintenance");
        baseline->click();require(recheck->isEnabled()&&comparison->item(0,1)->text()!="Not measured","baseline measured");
        recheck->click();require(comparison->item(1,2)->text()!="Not measured","latest swap reading or unknown");
        auto* cleanup=window.findChild<QPushButton*>("maintenanceAptClean");
        require(cleanup,"review package cleanup link");cleanup->click();
        require(window.currentPageId()=="experimental","cleanup routes to reviewed change");
        auto* activePage=window.findChild<QWidget*>("sysPulsePages");
        const auto selectedTools=activePage->findChildren<QWidget*>("toolboxPage");
        bool exact=false;for(auto* page:selectedTools)if(page->isVisible())if(auto* title=page->findChild<QLabel*>("toolboxTitle"))exact=title->text()=="Clean downloaded packages";
        require(exact,"exact catalog action selected");
        require(reportTable->rowCount()==0,"measurement and locating cleanup do not execute mutations");
        window.navigate("control");
        auto* index=window.findChild<QTableWidget*>("settingsIndexTable");
        auto* indexSearch=window.findChild<QLineEdit*>("settingsIndexSearch");
        auto* locate=window.findChild<QPushButton*>("locateIndexedSetting");
        require(index&&indexSearch&&locate&&index->rowCount()==37,"all 37 native Control Panel settings indexed");
        controlTabs->setCurrentIndex(8);indexSearch->setText("display resolution");
        int matching=-1,visible=0;for(int i=0;i<index->rowCount();++i)if(!index->isRowHidden(i)){matching=i;++visible;}
        require(visible==1&&matching>=0&&!locate->isEnabled(),"index filters and requires selection");
        index->selectRow(matching);require(locate->isEnabled(),"locate enabled for selected setting");locate->click();
        require(controlTabs->tabText(controlTabs->currentIndex())=="Desktop","index locates desktop setting without launching");
        indexSearch->setText("no-such-setting-xyz");require(!locate->isEnabled(),"filter clears stale selection");
        indexSearch->clear();controlTabs->setCurrentIndex(0);
        auto* systemBoost=window.findChild<QPushButton*>("optimizeSystemMemory");
        auto* systemStatus=window.findChild<QLabel*>("systemMemoryStatus");
        auto* undoCompression=window.findChild<QPushButton*>("disableSystemMemoryCompression");
        require(systemBoost&&systemStatus&&undoCompression&&systemStatus->text().contains("System-wide zswap:"),"system-wide compression controls and detected state");
        systemStatus->setText("stale fixture");
        window.navigate("overview");window.navigate("control");
        require(systemStatus->text().contains("System-wide zswap:"),"return to Control Panel refreshes kernel state");
        systemStatus->setText("stale fixture");
        controlTabs->setCurrentIndex(1);controlTabs->setCurrentIndex(0);
        require(systemStatus->text().contains("System-wide zswap:"),"return to performance tab refreshes kernel state");
        auto* sample=window.findChild<QPushButton*>("measureSwapActivity");
        auto* activity=window.findChild<QLabel*>("swapActivityStatus");
        require(sample&&activity&&sample->isEnabled(),"system-wide swap activity available on demand");
        sample->click();
        if(!sample->isEnabled()){require(activity->text().contains("Measuring"),"measurement is asynchronous");QElapsedTimer deadline;deadline.start();while(!sample->isEnabled()&&deadline.elapsed()<4500)events(25);}
        require(sample->isEnabled()&&(activity->text().contains("KiB/s")||activity->text().contains("unavailable")),"measured rate or honest unavailable state");
        require(idleTimers()==0,"swap measurement leaves no recurring timers");
        auto* optimize=window.findChild<QPushButton*>("optimizeMemory");
        auto* memoryStatus=window.findChild<QLabel*>("memoryOptimizerStatus");
        require(optimize&&memoryStatus,"one-click process-local memory relief available");
        optimize->click();require(memoryStatus->text().contains("SysPulse RSS before:"),"memory action reports measured scope");
        require(memoryStatus->text().contains("System available RAM before:"),"real system measurement displayed without attribution");
        auto* review=window.findChild<QPushButton*>("reviewMemoryConsumers");
        require(review,"memory-heavy process review available");review->click();events(25);
        auto* processTable=window.findChild<QTableView*>("processTable");
        require(window.currentPageId()=="performance"&&tabs->currentIndex()==5&&processTable,"review opens actual processes");
        require(processTable->horizontalHeader()->sortIndicatorSection()==2&&processTable->horizontalHeader()->sortIndicatorOrder()==Qt::DescendingOrder,"processes sorted by memory");
        require(!processTable->selectionModel()->hasSelection(),"review does not select a process for termination");
        window.navigate("control");
        auto* pause=window.findChild<QPushButton*>("pauseOptimizerSampling");auto* resume=window.findChild<QPushButton*>("resumeOptimizerSampling");
        auto* monitor=static_cast<syspulse::PerformancePage*>(window.findChild<QWidget*>("syspulsePerformancePage"));
        require(pause&&resume&&monitor,"explicit low overhead controls");pause->click();require(monitor->isPaused(),"pause stops sampling");resume->click();require(!monitor->isPaused(),"resume restores sampling preference");
        require(reportTable->rowCount()==0,"memory relief does not fabricate diagnostic records");
        auto* helpUpdate=window.findChild<QAction*>("helpUpdateApplication");require(helpUpdate,"Help menu exposes update application action");helpUpdate->trigger();require(window.currentPageId()=="settings","Help update action opens settings");
        require(window.findChild<QPushButton*>("checkApplicationUpdate")&&window.findChild<QLabel*>("applicationUpdateStatus"),"update controls and honest initial state present");
        auto* autoCheck=window.findChild<QCheckBox*>("automaticCachedUpdateCheck");
        auto* updateTimer=window.findChild<QTimer*>("cachedUpdateTimer");
        require(autoCheck&&updateTimer&&!autoCheck->isChecked()&&!updateTimer->isActive(),"cached update checks off by default");
        autoCheck->setChecked(true);require(updateTimer->isActive()&&updateTimer->interval()==1800000,"explicit session opt-in starts bounded polling");
        {
            QSettings settings(QSettings::IniFormat,QSettings::UserScope,"SysPulse","SysPulse");
            require(settings.value("updates/automaticCachedCheck").toBool(),"opt-in saved to isolated user preferences");
            syspulse::Window restarted(false);
            auto* savedCheck=restarted.findChild<QCheckBox*>("automaticCachedUpdateCheck");
            auto* savedTimer=restarted.findChild<QTimer*>("cachedUpdateTimer");
            require(savedCheck&&savedTimer&&savedCheck->isChecked()&&savedTimer->isActive(),"opt-in restored on next launch");
        }
        autoCheck->setChecked(false);require(!updateTimer->isActive(),"disable stops polling");
        {QSettings settings(QSettings::IniFormat,QSettings::UserScope,"SysPulse","SysPulse");require(!settings.value("updates/automaticCachedCheck").toBool(),"opt-out saved");}
        search->setText("Experimental");QMetaObject::invokeMethod(search,"returnPressed");
        require(window.currentPageId()=="experimental","search also navigates directly to module names");
        window.navigate("overview");
        auto* analyze=window.findChild<QPushButton*>("analyzeSystem");require(analyze,"analysis action exists");
        bool reviewed=false;QTimer::singleShot(30,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());reviewed=dialog&&dialog->objectName()=="healthCheckDialog";if(dialog)dialog->reject();});
        analyze->click();require(reviewed,"analysis opens the reviewed plan before executing");require(reportTable->rowCount()==0,"cancelling analysis leaves reports empty");
        syspulse::PerformancePage performance;performance.setSamplingEnabled(false);performance.setPaused(true);performance.refresh();
        require(performance.isPaused(),"manual refresh does not silently resume paused monitoring");
        performance.setPaused(false);performance.setSamplingEnabled(true);performance.refresh();events(150);performance.refresh();
        require(performance.memoryValue().contains("GiB")&&performance.cpuValue().contains('%'),"real VM procfs metrics supply sampled summary values");
        performance.setSamplingEnabled(false);
        if(argc>1) {
            const QString prefix=QString::fromLocal8Bit(argv[1]);
            search->clear();
            auto* embedded=static_cast<syspulse::PerformancePage*>(window.findChild<QWidget*>("syspulsePerformancePage"));
            require(embedded,"real monitoring workspace is embedded");
            window.navigate("overview");embedded->setSamplingEnabled(true);
            const int warmup=qBound(0,qEnvironmentVariableIntValue("SYSPULSE_PREVIEW_WARMUP_SECONDS"),120);
            for(int i=0;i<6;++i){events(150);embedded->refresh();}
            if(warmup)events(warmup*1000);
            require(window.grab().save(prefix+"-overview.png"),"render actual overview");
            window.navigate("performance");embedded->setSamplingEnabled(true);events(1200);
            require(window.grab().save(prefix+"-performance.png"),"render actual performance workspace");
            window.navigate("storage");events(40);
            require(window.grab().save(prefix+"-storage.png"),"render actual reviewed-tool workspace");
            window.navigate("experimental");events(40);
            require(window.grab().save(prefix+"-experimental.png"),"render separate experimental workspace");
        }
        std::cout<<checks<<" SysPulse shell/navigation/search/review/privacy checks passed\n";
    } catch(const std::exception& e) {std::cerr<<"SysPulse shell failure: "<<e.what()<<'\n';return 1;}
    return 0;
}
