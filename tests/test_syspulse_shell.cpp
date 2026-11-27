// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/window.hpp"
#include "syspulse/dependency_help.hpp"
#include "syspulse/performance.hpp"
#include "tux/core.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QAction>
#include <QMenu>
#include <QScrollArea>
#include "syspulse/fluent_visuals.hpp"
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
#include <QSet>
#include <QTemporaryDir>
#include <QMessageBox>
#include <cstdlib>
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
        struct StartupWindows final:QObject {
            int visibleTopLevels=0;
            bool eventFilter(QObject* o,QEvent* e) override {
                if(e->type()==QEvent::Show){if(auto* widget=qobject_cast<QWidget*>(o);widget&&widget->isWindow())++visibleTopLevels;}
                return false;
            }
        } startup;
        app.installEventFilter(&startup);
        syspulse::Window window(false);
        require(startup.visibleTopLevels==0,"no transient top-level widgets during construction");
        window.resize(1366,768);window.show();events(50);
        app.removeEventFilter(&startup);
        require(startup.visibleTopLevels==1,"only the main window is shown at startup");
        require(window.navigationCount()==13,"existing navigation plus Control Panel");
        int iconCount=0;
        for(auto* button:window.findChildren<QPushButton*>())if(button->objectName()=="navButton"){
            require(!button->icon().isNull()&&button->iconSize().width()==20,"every navigation item has a lightweight vector icon");
            require(!button->accessibleName().isEmpty()&&!button->toolTip().isEmpty(),"navigation retains accessible labels and hints");++iconCount;
        }
        require(iconCount==13,"all thirteen destinations decorated");
        require(window.findChild<QWidget*>("fluentBackdrop"),"cached local backdrop installed");
        auto* help=window.findChild<QPushButton*>("helpMenuButton");
        require(help&&help->menu()&&!help->icon().isNull(),"help menu has icon");
        for(auto* action:help->menu()->actions())require(!action->icon().isNull(),"help commands have distinct icon glyphs");
        for(auto* tabs:window.findChildren<QTabWidget*>())for(int i=0;i<tabs->count();++i)require(!tabs->tabIcon(i).isNull(),"workspace tabs have icons");
        require(window.currentPageId()=="overview","overview is the initial destination");
        auto* sharing=window.findChild<QPushButton*>("reviewDiagnosticSharing");require(sharing,"report consent entry present");
        bool consentChecks=false;
        QTimer::singleShot(0,&window,[&]{
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!dialog)return;
            auto* level=dialog->findChild<QComboBox*>("diagnosticShareLevel");
            auto* consent=dialog->findChild<QCheckBox*>("diagnosticShareConsent");
            auto* preview=dialog->findChild<QPushButton*>("diagnosticGeneratePreview");
            auto* output=dialog->findChild<QPlainTextEdit*>("diagnosticSharePreview");
            auto* send=dialog->findChild<QPushButton*>("diagnosticSend");
            auto* save=dialog->findChild<QPushButton*>("diagnosticSave");
            if(level&&consent&&preview&&output&&send&&save){
                const bool initial=level->currentIndex()==0&&!consent->isChecked()&&output->toPlainText().isEmpty()&&!send->isEnabled();
                level->setCurrentIndex(1);preview->click();
                const bool generated=!output->toPlainText().isEmpty()&&!save->isEnabled()&&!send->isEnabled();
                consent->setChecked(true);const bool reviewed=save->isEnabled()&&send->isEnabled()==send->property("relayConfigured").toBool();
                level->setCurrentIndex(3);const bool reset=!consent->isChecked()&&output->toPlainText().isEmpty()&&!save->isEnabled()&&!send->isEnabled();
                consentChecks=initial&&generated&&reviewed&&reset;
            }
            dialog->reject();
        });
        sharing->click();require(consentChecks,"diagnostic sharing defaults off, needs review and invalidates changed payload; send requires configured relay");
        auto* back=window.findChild<QPushButton*>("navigationBack");auto* forward=window.findChild<QPushButton*>("navigationForward");
        require(back&&forward&&!back->isEnabled()&&!forward->isEnabled(),"initial history empty except overview");
        window.navigate("performance");window.navigate("storage");back->click();
        require(window.currentPageId()=="performance"&&forward->isEnabled(),"back restores previous workspace");
        forward->click();require(window.currentPageId()=="storage"&&!forward->isEnabled(),"forward restores next workspace");
        back->click();window.navigate("overview");require(!forward->isEnabled(),"new navigation discards forward history");
        require(window.findChild<QScrollArea*>("navigationScroll"),"navigation scrolls on short screens");
        auto* wallpaper=window.findChild<QCheckBox*>("appearanceArtwork");
        auto* background=static_cast<syspulse::fluent::Backdrop*>(window.findChild<QWidget*>("fluentBackdrop"));
        require(wallpaper&&background&&wallpaper->isChecked()&&background->artworkEnabled(),"artwork default enabled");
        wallpaper->setChecked(false);require(!background->artworkEnabled(),"reduced artwork changes visible rendering state");
        { QSettings setting(QSettings::IniFormat,QSettings::UserScope,"SysPulse","SysPulse");require(!setting.value("appearance/artwork",true).toBool(),"appearance preference persists"); }
        wallpaper->setChecked(true);require(background->artworkEnabled(),"artwork can be restored");
        int tiles=0;
        for(auto* b:window.findChildren<QPushButton*>())if(b->property("systemToolCommand").isValid()){
            require(!b->icon().isNull()&&b->parentWidget()->objectName()=="systemToolTile","system launcher uses a rich tile");
            require(b->parentWidget()->findChild<QLabel*>("toolAvailability"),"tile reports actual capability status");++tiles;
        }
        require(tiles==63,"all existing launchers retained as tiles");
        require(syspulse::dependencyPackage("xfce4-display-settings")=="xfce4-settings"&&syspulse::dependencyPackage("xrandr")=="x11-xserver-utils","known dependency mappings");
        require(syspulse::dependencyPackage("syspulse-memory-control").isEmpty()&&syspulse::dependencyPackage("malicious; command").isEmpty(),"internal and arbitrary names never become installation targets");
        for(auto* b:window.findChildren<QPushButton*>())if(b->property("systemToolCommand").isValid()||b->property("desktopToolCommand").isValid()){
            auto* dep=b->parentWidget()->findChild<QPushButton*>("toolDependency");require(dep&&!dep->toolTip().isEmpty(),"native launcher has reviewed dependency route");
        }
        int desktopTiles=0;QSet<qint64> desktopIcons;
        for(auto* b:window.findChildren<QPushButton*>())if(b->property("desktopToolCommand").isValid()){
            require(b->parentWidget()->objectName()=="systemToolTile"&&b->iconSize()==QSize(40,40),"desktop settings use large graphical tiles");
            require(!b->toolTip().isEmpty()&&!b->accessibleDescription().isEmpty()&&b->parentWidget()->findChild<QLabel*>("toolDescription"),"desktop tiles expose hover descriptions");
            desktopIcons.insert(b->icon().cacheKey());++desktopTiles;
        }
        require(desktopTiles==8&&desktopIcons.size()==8,"all eight screenshot launchers have distinct icons");
        for(auto* tab:window.findChildren<QTabWidget*>())for(int i=0;i<tab->count();++i)require(!tab->tabToolTip(i).isEmpty(),"every workspace tab has hover help");
        for(auto* b:window.findChildren<QPushButton*>())if(!b->text().trimmed().isEmpty())require(!b->icon().isNull()&&!b->toolTip().isEmpty(),"labeled shell actions have icons and hover help");
        QDialog dialog;QPushButton delayed("Refresh results",&dialog);dialog.show();events(25);require(!delayed.icon().isNull()&&!delayed.toolTip().isEmpty(),"late-opened dialog actions decorated");dialog.hide();
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
        require(storageTabs&&storageTabs->count()==5&&storageTabs->tabText(4)=="HDD optimization"&&storageTabs->tabText(3)=="Filesystem repair"&&storageTabs->tabText(1)=="Disk tools"&&storageTabs->tabText(2)=="Cleaner","storage tools extend the existing workspace");
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
        search->setText("XFCE desktop / Display");QMetaObject::invokeMethod(search,"returnPressed");
        require(window.currentPageId()=="desktop"&&window.findChild<QTabWidget*>("desktopWorkspaceTabs")->currentIndex()==0,"desktop launchers are searchable without execution");
        for(const auto& id:QStringList{"cpu-topology","kernel-version","uptime-summary","systemd-state","boot-history","discard-support","disk-transport","package-holds","package-origins","native-architecture","pci-inventory","usb-inventory","display-modes","compositor-state","graphics-renderer","link-counters","socket-summary","network-manager-state","system-timer-files","user-timer-files"}) {
            search->setText(id);QMetaObject::invokeMethod(search,"returnPressed");
            bool selected=false;
            for(auto* table:window.findChildren<QTableWidget*>("toolboxTools"))if(table->isVisible()&&table->currentRow()>=0&&table->item(table->currentRow(),0)&&table->item(table->currentRow(),0)->text()==tux::action(id).title)selected=true;
            require(selected,"exact new diagnostic ID routes to selected tool without launching");
        }
        require(reportTable->rowCount()==0,"twenty new diagnostic routes never execute tasks");
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
        require(window.findChild<QPushButton*>("quickBoost")&&window.findChild<QPushButton*>("maintenanceQuickBoost"),"visible primary Boost controls");
        require(!window.findChild<QPushButton*>("quickBoost")->isEnabled()&&!window.findChild<QPushButton*>("maintenanceQuickBoost")->isEnabled(),"Boost locked without fresh analysis");
        require(window.findChild<QPushButton*>("diagnosticHistory"),"local learning history control present");
        require(window.findChild<QPushButton*>("quickDesktopProfiles")&&window.findChild<QPushButton*>("lastBoostResults")&&!window.findChild<QPushButton*>("lastBoostResults")->isEnabled(),"visible preset and saved results controls without fabricated receipt");
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
        auto* quickClean=window.findChild<QPushButton*>("sidebarCleaner");auto* quickUpdate=window.findChild<QPushButton*>("sidebarApplicationUpdates");auto* globalAdmin=window.findChild<QPushButton*>("globalAdminSession");auto* quickLock=window.findChild<QPushButton*>("globalAdminLock");
        require(quickClean&&quickUpdate&&globalAdmin&&quickLock&&!quickLock->isEnabled(),"visible action shortcuts and locked session status");
        quickClean->click();require(window.currentPageId()=="storage"&&storageTabs->currentIndex()==2,"direct cleaner navigation");
        quickUpdate->click();require(window.currentPageId()=="settings","direct signed update navigation");
        window.navigate("overview");globalAdmin->click();require(window.currentPageId()=="settings","administrator indicator opens session settings");
        require(window.findChild<QPushButton*>("offlineFilesystemRefresh")&&window.findChild<QPushButton*>("offlineFilesystemCheck")&&!window.findChild<QPushButton*>("offlineFilesystemRepair")->isEnabled(),"offline repair gated until reviewed check");
        require(window.findChild<QPushButton*>("applyRuntimeSwappiness")&&!window.findChild<QPushButton*>("applyRuntimeSwappiness")->isEnabled(),"runtime memory action gated until capture");
        require(window.findChild<QPushButton*>("launchBraveBounded")&&window.findChild<QComboBox*>("physicalInputDevices"),"browser and input action controls present");
        require(window.findChild<QComboBox*>("wifiRadioValue")&&window.findChild<QComboBox*>("powerProfileValue"),"native radio and power controls present");
        require(window.findChild<QPushButton*>("saveBraveProfile")&&window.findChild<QPushButton*>("captureDesktopSnapshot")&&window.findChild<QPushButton*>("restoreDesktopSnapshot"),"persistent Brave and scoped desktop recovery controls present");
        require(window.findChild<QComboBox*>("scheduleProfileChoice")&&window.findChild<QPushButton*>("scheduleProfileApply")&&!window.findChild<QPushButton*>("scheduleProfileApply")->isEnabled(),"schedule mutation gated by current timer state");
        require(window.findChild<QComboBox*>("hddProfileDevices")&&window.findChild<QPushButton*>("hddProfileApply")&&!window.findChild<QPushButton*>("hddProfileApply")->isEnabled(),"HDD tuning gated by device inventory");
        auto* helpUpdate=window.findChild<QAction*>("helpUpdateApplication");require(helpUpdate,"Help menu exposes update application action");helpUpdate->trigger();require(window.currentPageId()=="settings","Help update action opens settings");
        require(window.findChild<QPushButton*>("checkApplicationUpdate")&&window.findChild<QLabel*>("applicationUpdateStatus"),"update controls and honest initial state present");
        require(window.findChild<QComboBox*>("applicationUpdateChannel")->currentData()=="alpha"&&!window.findChild<QPushButton*>("installApplicationUpdate")->isEnabled(),"alpha build defaults to alpha channel and no unreviewed install");
        auto* rememberAdmin=window.findChild<QCheckBox*>("rememberAdminSession");auto* unlockAdmin=window.findChild<QPushButton*>("unlockAdminSession");auto* lockAdmin=window.findChild<QPushButton*>("lockAdminSession");
        require(rememberAdmin&&unlockAdmin&&lockAdmin&&!rememberAdmin->isChecked()&&!unlockAdmin->isEnabled()&&!lockAdmin->isEnabled(),"administrator session locked and opt-in only");
        rememberAdmin->setChecked(true);require(unlockAdmin->isEnabled(),"explicit opt-in enables desktop authentication");rememberAdmin->setChecked(false);require(!unlockAdmin->isEnabled(),"opt-out locks session");
        auto* autoCheck=window.findChild<QCheckBox*>("automaticCachedUpdateCheck");
        auto* updateTimer=window.findChild<QTimer*>("cachedUpdateTimer");
        require(autoCheck&&updateTimer&&!autoCheck->isChecked()&&!updateTimer->isActive(),"cached update checks off by default");
        autoCheck->setChecked(true);require(updateTimer->isActive()&&updateTimer->interval()==1800000,"explicit session opt-in starts bounded polling");
        {
            QSettings settings(QSettings::IniFormat,QSettings::UserScope,"SysPulse","SysPulse");
            require(settings.value("updates/automaticSignedCheck").toBool(),"opt-in saved to isolated user preferences");
            syspulse::Window restarted(false);
            auto* savedCheck=restarted.findChild<QCheckBox*>("automaticCachedUpdateCheck");
            auto* savedTimer=restarted.findChild<QTimer*>("cachedUpdateTimer");
            require(savedCheck&&savedTimer&&savedCheck->isChecked()&&savedTimer->isActive(),"opt-in restored on next launch");
        }
        autoCheck->setChecked(false);require(!updateTimer->isActive(),"disable stops polling");
        {QSettings settings(QSettings::IniFormat,QSettings::UserScope,"SysPulse","SysPulse");require(!settings.value("updates/automaticSignedCheck").toBool(),"opt-out saved");}
        search->setText("Experimental");QMetaObject::invokeMethod(search,"returnPressed");
        require(window.currentPageId()=="experimental","search also navigates directly to module names");
        window.navigate("overview");
        auto* analyze=window.findChild<QPushButton*>("analyzeSystem");require(analyze,"analysis action exists");
        bool reviewed=false;QTimer::singleShot(30,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());reviewed=dialog&&dialog->objectName()=="deepAnalysisReview";if(dialog)dialog->reject();});
        analyze->click();require(reviewed,"analysis opens the reviewed plan before executing");require(reportTable->rowCount()==0,"cancelling analysis leaves reports empty");require(!window.findChild<QPushButton*>("quickBoost")->isEnabled(),"cancelled analysis cannot authorize Boost");
        if(qEnvironmentVariableIntValue("SYSPULSE_DIAGNOSTICS_RUNTIME")==1){
            QTemporaryDir isolatedHome;require(isolatedHome.isValid(),"isolated diagnostics runtime home");
            const auto oldHome=qgetenv("HOME");qputenv("HOME",isolatedHome.path().toUtf8());
            bool analyzed=false,reviewedBoost=false,finishedBoost=false,errorDialog=false;
            QTimer watchdog;watchdog.setSingleShot(true);QObject::connect(&watchdog,&QTimer::timeout,&app,[]{std::cerr<<"diagnostics GUI watchdog expired\n";std::exit(2);});watchdog.start(175000);
            QTimer driver;driver.setInterval(50);
            QObject::connect(&driver,&QTimer::timeout,&app,[&]{
                auto* dlg=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dlg)return;
                if(dlg->objectName()=="deepAnalysisReview"){if(auto* b=dlg->findChild<QPushButton*>("runDeepAnalysis"))b->click();}
                else if(dlg->objectName()=="deepAnalysisResults"){analyzed=true;dlg->accept();}
                else if(dlg->objectName()=="adaptiveBoostReview"){
                    for(auto* check:dlg->findChildren<QCheckBox*>())check->setChecked(false);
                    auto* own=dlg->findChild<QCheckBox*>("boostOwnMemory");auto* apply=dlg->findChild<QPushButton*>("applyQuickBoost");
                    if(own&&own->isEnabled()&&apply){own->setChecked(true);reviewedBoost=true;apply->click();}
                    else{errorDialog=true;dlg->reject();}
                }else if(dlg->objectName()=="adaptiveBoostResults"){finishedBoost=true;dlg->accept();}
                else if(auto* msg=qobject_cast<QMessageBox*>(dlg)){errorDialog=true;msg->accept();}
            });
            driver.start();analyze->click();
            auto* boost=window.findChild<QPushButton*>("quickBoost");
            require(analyzed&&!errorDialog&&boost->isEnabled(),"actual read-only analysis unlocks Boost");
            boost->click();driver.stop();watchdog.stop();
            require(reviewedBoost&&finishedBoost&&!errorDialog&&!boost->isEnabled(),"actual GUI selected own-process memory action and consumed analysis");
            require(window.findChild<QPushButton*>("lastBoostResults")->isEnabled(),"actual Boost receipt persisted");
            qputenv("HOME",oldHome);
            std::cout<<"Actual installed GUI Analyze -> Boost (own-process memory only) -> receipt passed\n";
        }
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
