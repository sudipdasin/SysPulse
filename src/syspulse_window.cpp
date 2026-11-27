#include "syspulse/result_state.hpp"
#include "tux/admin_session.hpp"
#include "tux/admin_session_async.hpp"
#include "tux/admin_protocol.hpp"
#include <QFile>
#include <QSpinBox>
#include <QDialog>
#include <QPlainTextEdit>
#include <QThread>
#include "syspulse/diagnostic_sharing_dialog.hpp"
#include "syspulse/dependency_help.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/window.hpp"
#include "syspulse/fluent_visuals.hpp"
#include "syspulse/cleaner_page.hpp"
#include "syspulse/offline_filesystem.hpp"
#include "syspulse/health_probe.hpp"
#include "syspulse/memory_relief.hpp"
#include "syspulse/memory_plan.hpp"
#include "syspulse/care_plan.hpp"
#include "syspulse/maintenance_preview.hpp"
#include "syspulse/system_memory.hpp"
#include "syspulse/swap_activity.hpp"
#include <QElapsedTimer>
#include <QDateTime>
#include "syspulse/pressure.hpp"
#include "syspulse/performance.hpp"
#include "syspulse/management.hpp"
#include "syspulse/toolbox.hpp"
#include "tux/core.hpp"
#include "tux/trusted_command.hpp"
#include "tux/async_command.hpp"
#include "syspulse/apt_status.hpp"
#include <QTimer>
#include <QSettings>
#include <QAction>
#include <QMenu>
#include "tux/health_dialog.hpp"
#include "tux/report_dialog.hpp"
#include "tux/process_manager.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QRegularExpression>
#include <QCloseEvent>
#include <QCompleter>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QMap>
#include <QStyle>
#include <functional>
#include <memory>

namespace syspulse {
namespace {
class ReviewedProgress final: public QDialog {
public: using QDialog::QDialog; void reject() override {}
protected: void closeEvent(QCloseEvent* e) override {e->ignore();}
};
class PulseMark final: public QWidget {
public:
    explicit PulseMark(QWidget* p=nullptr):QWidget(p) {setFixedSize(42,48);setAccessibleName("SysPulse pulse mark");}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#B51235"),3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        p.drawPolyline(QPolygonF(QVector<QPointF>{{1,25},{10,25},{16,6},{23,42},{30,17},{34,25},{41,25}}));
    }
};
QLabel* text(const QString& value,const char* name=nullptr) {
    auto* label=new QLabel(value);label->setWordWrap(true);
    if(name)label->setObjectName(name);
    return label;
}
QFrame* panel(const char* name="panel") {
    auto* frame=new QFrame;frame->setObjectName(name);return frame;
}
QPushButton* button(const QString& title,std::function<void()> fn,QWidget* context,bool primary=false) {
    auto* b=new QPushButton(QString(title).replace("&","&&"));b->setCursor(Qt::PointingHandCursor);b->setAccessibleName(title);
    b->setProperty("primary",primary);fluent::decorate(b);
    QObject::connect(b,&QPushButton::clicked,context,[fn=std::move(fn)]{fn();});return b;
}
QWidget* workspace(const QString& title,const QString& subtitle,QWidget* child) {
    auto* page=new QWidget;auto* layout=new QVBoxLayout(page);
    layout->setContentsMargins(22,20,22,18);layout->setSpacing(12);
    layout->addWidget(text(title,"pageTitle"));layout->addWidget(text(subtitle,"muted"));
    layout->addWidget(child,1);return page;
}
QString destination(const tux::Action& a) {
    if(a.changes)return "experimental";
    if(a.group=="Storage"||a.group=="Maintenance")return "storage";
    if(a.group=="Network")return "network";
    if(a.group=="Schedules")return "schedules";
    if(a.group=="Security"||a.group=="Diagnostics")return "security";
    return "desktop";
}
}
struct Window::Impl {
    Window* w;
    QStackedWidget* stack=nullptr;
    QLineEdit* search=nullptr;
    QLabel* breadcrumb=nullptr;
    QLabel* footer=nullptr;
    std::function<void()> refreshMemoryKernel;
    std::function<void()> refreshSettingsIndex;
    PerformancePage* performance=nullptr;
    ProcessManagerWidget* processes=nullptr;
    ManagementPage* management=nullptr;
    QMap<QString,QWidget*> pages;
    QMap<QString,QPushButton*> nav;
    QMap<QString,ToolboxPage*> toolboxes;
    QVector<ToolboxPage*> allToolboxes;
    QVector<tux::Result> results;
    QTableWidget* reports=nullptr;
    QLabel* reportStatus=nullptr;
    QLabel* cpu=nullptr;QLabel* memory=nullptr;QLabel* disk=nullptr;QLabel* network=nullptr;
    QLabel* analysisStatus=nullptr;
    QString current="overview";
    QStringList history;
    int historyIndex=-1;
    QPushButton *backButton=nullptr,*forwardButton=nullptr;
    fluent::Backdrop* backdrop=nullptr;
    bool historyReplay=false;
    bool sampling=true;
    explicit Impl(Window* window,bool sample):w(window),sampling(sample) {build();}
    void remember(const tux::Result& result) {
        results.append(result);if(results.size()>20)results.removeFirst();
        reports->setRowCount(results.size());
        for(int row=0;row<results.size();++row) {
            const auto& r=results[results.size()-row-1];
            const QString outcome=resultState(r);
            reports->setItem(row,0,new QTableWidgetItem(r.action));
            reports->setItem(row,1,new QTableWidgetItem(outcome));
            reports->setItem(row,2,new QTableWidgetItem(QString::number(r.seconds,'f',2)+" s"));
            reports->setItem(row,3,new QTableWidgetItem(r.truncated?"Output bounded":"Local session"));
        }
        reportStatus->setText(QString("%1 completed task(s) in this session · Summary exports omit raw diagnostics").arg(results.size()));
    }
    void exportReports() {
        tux::ReportDialog dialog(results,w);dialog.setWindowTitle("Review report · SysPulse");
        for(auto* widget:dialog.findChildren<QWidget*>())widget->setStyleSheet(QString());
        dialog.exec();
    }
    ToolboxPage* toolbox(const QStringList& groups,bool experimental=false) {
        auto* p=new ToolboxPage(groups,experimental,w);allToolboxes.append(p);
        p->canExecute=[this,p]{
            if(management&&management->isBusy())return false;
            for(auto* other:allToolboxes)if(other!=p&&other->isBusy())return false;
            return true;
        };
        p->onResult=[this](const tux::Result& r){remember(r);};
        p->onBusyChanged=[this](bool){updateFooter();};return p;
    }
    void refreshActionIndicators() {
        const bool active=tux::admin::active(),busy=tux::admin::busy();
        if(auto* b=w->findChild<QPushButton*>("globalAdminSession"))b->setText(active?"Admin: unlocked":busy?"Admin: busy":"Admin: locked");
        if(auto* b=w->findChild<QPushButton*>("globalAdminLock"))b->setEnabled(active&&!busy);
        if(auto* b=w->findChild<QPushButton*>("sidebarApplicationUpdates")){const auto* install=w->findChild<QPushButton*>("installApplicationUpdate");b->setText(install&&install->isEnabled()?"Update available":"Application updates");}
    }
    void updateFooter() {
        refreshActionIndicators();
        bool busy=false;for(auto* p:allToolboxes)busy=busy||p->isBusy();
        const bool systemBusy=management&&management->isBusy();
        for(auto* page:allToolboxes)page->setExecutionAllowed(!systemBusy&&(!busy||page->isBusy()));
        if(auto* analyzeButton=w->findChild<QPushButton*>("analyzeSystem"))analyzeButton->setEnabled(!busy&&!systemBusy);
        footer->setText(QString("%1  ·  Automation off  ·  SysPulse %2").arg(busy||systemBusy?"Task in progress":"Manual maintenance").arg(tux::Version));
    }
    void analyze() {
        if(management->isBusy())return;
        for(auto* page:allToolboxes)if(page->isBusy())return;
        tux::HealthDialog dialog(w);dialog.setWindowTitle("Analyze system · SysPulse");
        dialog.setStyleSheet(QString());
        for(auto* widget:dialog.findChildren<QWidget*>())widget->setStyleSheet(QString());
        dialog.exec();
        const auto completed=dialog.results();for(const auto& r:completed)remember(r);
        if(!completed.isEmpty()) {
            int ok=0;for(const auto& r:completed)if(r.code==0&&!r.cancelled&&!r.truncated)++ok;
            QString summary=QString("Analysis recorded: %1 of %2 checks completed successfully. Review the findings in Reports.").arg(ok).arg(completed.size());
            analysisStatus->setToolTip(QString());
            const auto completedCheck=[&completed](const QString& id){
                const auto& tool=tux::action(id);
                for(const auto& result:completed)
                    if(result.action==tool.title && result.command==tool.command && result.code==0 && !result.cancelled && !result.truncated) return true;
                return false;
            };
            // Supplement a completed, reviewed storage and memory analysis with local measurements.
            // Do not run this additional snapshot when either check was skipped, failed or cancelled.
            if(completedCheck("storage") && completedCheck("memory")) {
                const auto sample=syspulse::triage::measure();
                const auto findings=syspulse::triage::assess(sample.ram,sample.root,sample.swap);
                QStringList states, advice;
                using syspulse::triage::Check;
                using syspulse::triage::Severity;
                for(const auto& finding:findings) {
                    const QString name=finding.check==Check::AvailableMemory?"RAM":finding.check==Check::RootStorage?"Root storage":"Swap";
                    const QString state=finding.severity==Severity::Critical?"Critical":finding.severity==Severity::Warning?"Warning":finding.severity==Severity::Unknown?"Unknown":finding.severity==Severity::Info?"Info":"Within threshold";
                    states.append(name+": "+state);
                    advice.append(name+": "+QString::fromLatin1(finding.advice));
                }
                summary+=QString("\nLocal snapshot: %1.").arg(states.join(" · "));
                analysisStatus->setToolTip(advice.join("\n"));
            }
            analysisStatus->setText(summary);
        }
    }
    void build() {
        w->setObjectName("sysPulseWindow");w->setWindowTitle(QString("SysPulse %1 · XFCE Edition").arg(tux::Version));
        w->resize(1366,768);w->setMinimumSize(1000,680);
        auto* root=new fluent::Backdrop(w);backdrop=root;auto* outer=new QVBoxLayout(root);outer->setContentsMargins(0,0,0,0);outer->setSpacing(0);
        auto* shell=new QHBoxLayout;shell->setContentsMargins(0,0,0,0);shell->setSpacing(0);
        outer->addLayout(shell,1);
        auto* sidebar=new QWidget;sidebar->setObjectName("sidebar");sidebar->setFixedWidth(248);
        auto* sl=new QVBoxLayout(sidebar);sl->setContentsMargins(12,14,12,10);sl->setSpacing(3);
        auto* brand=new QHBoxLayout;brand->setSpacing(7);brand->addWidget(new PulseMark);
        auto* brandText=new QVBoxLayout;brandText->setSpacing(0);brandText->addWidget(text("SysPulse","brand"));brandText->addWidget(text("XFCE Edition","brandEdition"));brand->addLayout(brandText);sl->addLayout(brand);
        sl->addSpacing(12);
        auto addNav=[this,sl](const QString& id,const QString& title) {
            auto* b=new QPushButton(QString(title).replace("&","&&"));b->setObjectName("navButton");b->setCheckable(true);b->setAutoExclusive(true);
            b->setProperty("displayTitle",title);
            b->setIcon(fluent::glyph(id));b->setIconSize(QSize(20,20));
            b->setCursor(Qt::PointingHandCursor);b->setToolTip(fluent::hint(title));
            b->setAccessibleName(title);b->setProperty("pageId",id);nav.insert(id,b);sl->addWidget(b);
            QObject::connect(b,&QPushButton::clicked,w,[this,id]{navigate(id);});
        };
        sl->addWidget(text("WORKSPACE","navGroup"));
        addNav("overview","Overview");addNav("performance","Performance");addNav("storage","Storage & cleanup");
        sl->addWidget(text("SYSTEM","navGroup"));
        addNav("control","Control Panel");addNav("management","Startup & services");addNav("network","Network");addNav("desktop","XFCE desktop");addNav("security","Security & privacy");
        sl->addWidget(text("TOOLS","navGroup"));
        addNav("schedules","Schedules");addNav("recovery","Recovery");addNav("reports","Reports");addNav("experimental","Maintenance actions");
        auto* updateShortcut=button("Application updates",[this]{openSettingsSection("applicationUpdateStatus");},w);updateShortcut->setObjectName("sidebarApplicationUpdates");updateShortcut->setIcon(fluent::glyph("updates"));sl->addWidget(updateShortcut);
        auto* cleanerShortcut=button("Clean disk space",[this]{navigate("storage");if(auto* tabs=pages.value("storage")->findChild<QTabWidget*>("storageWorkspaceTabs"))tabs->setCurrentIndex(2);},w,true);cleanerShortcut->setObjectName("sidebarCleaner");sl->addWidget(cleanerShortcut);
        sl->addStretch(1);addNav("settings","Settings & help");
        auto* navScroll=new QScrollArea;navScroll->setObjectName("navigationScroll");navScroll->setWidgetResizable(true);navScroll->setFrameShape(QFrame::NoFrame);navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);navScroll->setFixedWidth(258);sidebar->setMinimumWidth(0);sidebar->setMaximumWidth(QWIDGETSIZE_MAX);navScroll->setWidget(sidebar);shell->addWidget(navScroll);
        auto* body=new QWidget;body->setObjectName("fluentBody");auto* bl=new QVBoxLayout(body);bl->setContentsMargins(0,0,0,0);bl->setSpacing(0);
        auto* utility=new QWidget;utility->setObjectName("utilityBar");auto* ul=new QHBoxLayout(utility);ul->setContentsMargins(24,12,24,12);
        backButton=button("Back",[this]{stepHistory(-1);},w);backButton->setObjectName("navigationBack");backButton->setIcon(fluent::glyph("back"));backButton->setToolTip("Back · Alt+Left");backButton->setText(QString());backButton->setEnabled(false);ul->addWidget(backButton);
        forwardButton=button("Forward",[this]{stepHistory(1);},w);forwardButton->setObjectName("navigationForward");forwardButton->setIcon(fluent::glyph("forward"));forwardButton->setToolTip("Forward · Alt+Right");forwardButton->setText(QString());forwardButton->setEnabled(false);ul->addWidget(forwardButton);
        auto* backShortcut=new QShortcut(QKeySequence(Qt::ALT|Qt::Key_Left),w);QObject::connect(backShortcut,&QShortcut::activated,w,[this]{stepHistory(-1);});
        auto* forwardShortcut=new QShortcut(QKeySequence(Qt::ALT|Qt::Key_Right),w);QObject::connect(forwardShortcut,&QShortcut::activated,w,[this]{stepHistory(1);});
        breadcrumb=text("Workspace / Overview");breadcrumb->setObjectName("breadcrumb");ul->addWidget(breadcrumb,1);
        search=new QLineEdit;search->setObjectName("globalToolSearch");search->setAccessibleName("Find a SysPulse tool");search->setPlaceholderText("Find a tool · Ctrl+F");search->setClearButtonEnabled(true);search->addAction(fluent::glyph("search"),QLineEdit::LeadingPosition);search->setMaximumWidth(420);ul->addWidget(search,1);
        auto* adminShortcut=button("Admin: locked",[this]{openSettingsSection("adminSessionStatus");},w);adminShortcut->setObjectName("globalAdminSession");ul->addWidget(adminShortcut);
        auto* quickLock=button("Lock",[this]{if(auto* lock=w->findChild<QPushButton*>("lockAdminSession"))lock->click();},w);quickLock->setObjectName("globalAdminLock");quickLock->setEnabled(false);ul->addWidget(quickLock);
        // Global controls are refreshed by actual state transitions; no idle polling.
        auto* helpButton=new QPushButton(fluent::glyph("help"),"Help");helpButton->setIconSize(QSize(18,18));helpButton->setObjectName("helpMenuButton");
        auto* helpMenu=new QMenu(helpButton);
        auto* updates=helpMenu->addAction(fluent::glyph("updates"),"Update application…");updates->setObjectName("helpUpdateApplication");
        QObject::connect(updates,&QAction::triggered,w,[this]{
            openSettingsSection("applicationUpdateStatus");
        });
        auto* settings=helpMenu->addAction(fluent::glyph("settings"),"Settings & help");QObject::connect(settings,&QAction::triggered,w,[this]{navigate("settings");});
        helpButton->setMenu(helpMenu);ul->addWidget(helpButton);bl->addWidget(utility);
        stack=new QStackedWidget;stack->setObjectName("sysPulsePages");bl->addWidget(stack,1);shell->addWidget(body,1);
        footer=text(QString(),"statusFooter");footer->setObjectName("statusFooter");outer->addWidget(footer);
        w->setCentralWidget(root);
        performance=new PerformancePage;
        performance->setSamplingEnabled(sampling);
        processes=new ProcessManagerWidget(nullptr,false);
        performance->detailTabs()->addTab(processes,"Processes");
        management=new ManagementPage;
        pages.insert("overview",overview());
        pages.insert("performance",performance);
        auto* storage=toolbox({"Storage","Maintenance"});toolboxes.insert("storage",storage);
        auto* storageTabs=new QTabWidget;storageTabs->setObjectName("storageWorkspaceTabs");storageTabs->addTab(storage,"Diagnostics & cleanup");
        storageTabs->addTab(installedTools("Use installed disk tools. Partition changes can destroy data; back up before modifying a disk.",{{"Disk management","gnome-disks"},{"Partition editor","gparted"},{"Disk usage analyzer","baobab"},{"File manager","thunar"},{"Cache cleanup","bleachbit"}}),"Disk tools");
        storageTabs->addTab(cleanerPage([this](const QString& id){locateMaintenanceTool(id);}),"Cleaner");storageTabs->addTab(offlineDiskPage(),"Filesystem repair");storageTabs->setCurrentIndex(2);
        pages.insert("storage",workspace("Storage & cleanup","Scan disks, review and delete eligible cache files, or run reviewed system maintenance actions.",storageTabs));
        pages.insert("management",management);
        pages.insert("control",controlPanelPage());
        pages.insert("network",networkPage());
        pages.insert("desktop",desktopPage());
        auto* security=toolbox({"Diagnostics","Security"});toolboxes.insert("security",security);
        auto* securityTabs=new QTabWidget;securityTabs->setObjectName("securityWorkspaceTabs");securityTabs->addTab(security,"Diagnostics");
        securityTabs->addTab(installedTools("Open distribution-supported software and security tools; they manage their own authorization.",{{"Update manager","mintupdate"},{"Software manager","mintinstall"},{"Package manager","synaptic"},{"Software sources","mintsources"},{"Firewall","gufw"},{"System logs","gnome-logs"}}),"Software & security tools");
        pages.insert("security",workspace("Security & privacy","Review system warnings, update state, permissions and available firewall tools. Reports stay local.",securityTabs));
        auto* schedules=toolbox({"Schedules"});toolboxes.insert("schedules",schedules);
        auto* schedulePage=new QWidget;auto* scheduleLayout=new QVBoxLayout(schedulePage);auto* scheduleActions=new QHBoxLayout;
        scheduleActions->addWidget(button("Enable weekly SSD TRIM",[this]{locateMaintenanceTool("trim-enable");},w,true));
        scheduleActions->addWidget(button("Disable weekly SSD TRIM",[this]{locateMaintenanceTool("trim-disable");},w));
        scheduleLayout->addLayout(scheduleActions);
        auto* cleanupSchedule=new QHBoxLayout;cleanupSchedule->addWidget(button("Enable weekly APT archive cleanup",[this]{locateMaintenanceTool("weekly-clean-enable");},w,true));cleanupSchedule->addWidget(button("Disable weekly APT archive cleanup",[this]{locateMaintenanceTool("weekly-clean-disable");},w));scheduleLayout->addLayout(cleanupSchedule);
        scheduleLayout->addWidget(schedules,1);
        pages.insert("schedules",workspace("Schedules","Inspect timers and opt in to weekly SSD maintenance or obsolete package archive cleanup. No timer changes at startup.",schedulePage));
        pages.insert("recovery",recoveryPage());
        pages.insert("reports",reportsPage());
        auto* experiments=toolbox({},true);toolboxes.insert("experimental",experiments);
        pages.insert("experimental",workspace("Maintenance actions","Apply system and desktop changes through reviewed, explicitly authorized tools. Experimental or higher-risk operations retain their prerequisites.",experiments));
        pages.insert("settings",settingsPage());
        management->canMutate=[this]{for(auto* page:allToolboxes)if(page->isBusy())return false;return true;};
        management->onBusyChanged=[this](bool){updateFooter();};
        for(auto i=pages.cbegin();i!=pages.cend();++i){i.value()->setProperty("sysPulsePageId",i.key());stack->addWidget(i.value());}
        fluent::decorateTree(w);
        performance->onMetricsChanged=[this]{updateMetrics();};
        performance->onOpenStartup=[this]{navigate("management");management->showStartup();};
        performance->onOpenServices=[this]{navigate("management");management->showServices();};
        QStringList suggestions;for(auto i=nav.cbegin();i!=nav.cend();++i)suggestions.append(i.value()->property("displayTitle").toString());
        for(const auto& a:tux::actions())suggestions.append(a.title);
        for(auto i=pages.cbegin();i!=pages.cend();++i) {
            const auto label=nav[i.key()]->property("displayTitle").toString();
            for(auto* control:i.value()->findChildren<QPushButton*>())
                if(control->property("systemToolCommand").isValid()||control->property("desktopToolCommand").isValid())
                    suggestions.append(label+" / "+fluent::plainTitle(control->text()));
        }
        suggestions.removeDuplicates();
        auto* completer=new QCompleter(suggestions,search);completer->setCaseSensitivity(Qt::CaseInsensitive);completer->setFilterMode(Qt::MatchContains);search->setCompleter(completer);
        QObject::connect(search,&QLineEdit::returnPressed,w,[this]{findTool(search->text());});
        QObject::connect(completer,QOverload<const QString&>::of(&QCompleter::activated),w,[this](const QString& value){findTool(value);});
        auto* findShortcut=new QShortcut(QKeySequence::Find,w);QObject::connect(findShortcut,&QShortcut::activated,w,[this]{search->setFocus();search->selectAll();});
        auto* clearShortcut=new QShortcut(QKeySequence(Qt::Key_Escape),search);clearShortcut->setContext(Qt::WidgetShortcut);QObject::connect(clearShortcut,&QShortcut::activated,search,&QLineEdit::clear);
        navigate("overview");updateMetrics();updateFooter();
    }
    QWidget* overview() {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);layout->setContentsMargins(0,0,0,0);layout->setSpacing(14);
        auto* hero=panel("analysisHero");auto* hl=new QHBoxLayout(hero);hl->setContentsMargins(24,22,24,22);
        auto* mark=new PulseMark;mark->setFixedSize(60,70);hl->addWidget(mark);
        auto* heroText=new QVBoxLayout;heroText->addWidget(text("Ready for analysis","sectionTitle"));
        analysisStatus=text("Run a scan to review storage, memory, services and system warnings.","muted");analysisStatus->setObjectName("analysisStatus");heroText->addWidget(analysisStatus);
        auto* actions=new QHBoxLayout;auto* analyzeButton=button("Analyze system",[this]{analyze();},w,true);analyzeButton->setObjectName("analyzeSystem");actions->addWidget(analyzeButton);actions->addWidget(button("Open reports",[this]{navigate("reports");},w));actions->addStretch();heroText->addLayout(actions);hl->addLayout(heroText,1);
        hl->addWidget(text("MAINTENANCE\nManual\n\nAUTOMATION\nOff","muted"));layout->addWidget(hero);
        auto* metrics=new QGridLayout;metrics->setSpacing(12);
        auto metric=[this,metrics](int column,const QString& label,QLabel** value,const QString& color,int tab) {
            auto* frame=panel("metricCard");auto* ml=new QVBoxLayout(frame);ml->setContentsMargins(16,14,16,14);
            ml->addWidget(text(label,"sectionTitle"));*value=text("Collecting…","metricValue");(*value)->setStyleSheet("color:"+color+";");ml->addWidget(*value);
            auto* open=button("View history →",[this,tab]{navigate("performance");performance->detailTabs()->setCurrentIndex(tab);},w);open->setObjectName("metricLink");ml->addWidget(open);metrics->addWidget(frame,0,column);
        };
        metric(0,"CPU",&cpu,"#148344",1);metric(1,"Memory",&memory,"#C8102E",2);metric(2,"Disk",&disk,"#2875D3",3);metric(3,"Network",&network,"#8444C4",4);layout->addLayout(metrics);
        auto* lower=new QHBoxLayout;lower->setSpacing(14);
        auto* checks=panel();auto* cl=new QVBoxLayout(checks);cl->setContentsMargins(18,16,18,16);cl->addWidget(text("Recommended checks","sectionTitle"));
        cl->addWidget(button("Package health →",[this]{navigate("storage");toolboxes["storage"]->setSearch("package");},w));
        cl->addWidget(button("Startup review →",[this]{navigate("management");management->showStartup();},w));
        cl->addWidget(button("SSD maintenance →",[this]{navigate("storage");toolboxes["storage"]->setSearch("trim");},w));cl->addStretch();lower->addWidget(checks,1);
        auto* quick=panel();auto* ql=new QVBoxLayout(quick);ql->setContentsMargins(18,16,18,16);ql->addWidget(text("Quick tools","sectionTitle"));auto* grid=new QGridLayout;
        grid->addWidget(button("Startup apps",[this]{navigate("management");management->showStartup();},w),0,0);
        grid->addWidget(button("Services",[this]{navigate("management");management->showServices();},w),0,1);
        grid->addWidget(button("Disk usage",[this]{navigate("storage");},w),1,0);
        grid->addWidget(button("XFCE settings",[this]{navigate("desktop");},w),1,1);
        grid->addWidget(button("Control Panel",[this]{navigate("control");},w),2,0);
        grid->addWidget(button("Optimize memory…",[this]{openMemoryOptimizer();},w),2,1);
        auto* care=button("System Care Center",[this]{openControlTab("systemCarePage");},w,true);care->setObjectName("openSystemCare");grid->addWidget(care,3,0,1,2);
        auto* maintenance=button("Maintenance & boost",[this]{openControlTab("maintenanceCenterPage");},w);maintenance->setObjectName("openMaintenanceCenter");grid->addWidget(maintenance,4,0,1,2);
        auto* cleaner=button("Cleaner · reclaim disk space",[this]{navigate("storage");if(auto* tabs=pages.value("storage")->findChild<QTabWidget*>("storageWorkspaceTabs"))tabs->setCurrentIndex(2);},w,true);cleaner->setObjectName("openCleaner");grid->addWidget(cleaner,5,0,1,2);
        ql->addLayout(grid);ql->addWidget(text(QString("%1 reviewed tools · Local reports · Explicit system changes").arg(tux::actions().size()),"muted"));ql->addStretch();lower->addWidget(quick,1);layout->addLayout(lower,1);
        layout->addWidget(text("No maintenance actions run when SysPulse starts.","muted"));
        auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(content);
        return workspace("System overview","Understand your system. Review every change.",scroll);
    }
    void updateMetrics() {
        cpu->setText(performance->cpuValue());memory->setText(performance->memoryValue());disk->setText(performance->diskValue());network->setText(performance->networkValue());
    }
    QWidget* networkPage() {
        auto* tabs=new QTabWidget;tabs->setObjectName("networkWorkspaceTabs");auto* connections=new QWidget;auto* layout=new QVBoxLayout(connections);
        auto* row=new QHBoxLayout;auto* reveal=new QCheckBox("Show local addresses");reveal->setObjectName("showNetworkAddresses");row->addWidget(reveal);row->addStretch();
        auto* table=new QTableWidget(0,4);table->setObjectName("networkConnections");table->setHorizontalHeaderLabels({"Interface","State","Capabilities","Addresses"});table->verticalHeader()->hide();table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        auto refresh=[table,reveal]{
            const auto interfaces=QNetworkInterface::allInterfaces();table->setRowCount(interfaces.size());int row=0;
            for(const auto& iface:interfaces) {
                table->setItem(row,0,new QTableWidgetItem(iface.humanReadableName()));
                table->setItem(row,1,new QTableWidgetItem(iface.flags().testFlag(QNetworkInterface::IsUp)?"Up":"Down"));
                table->setItem(row,2,new QTableWidgetItem(iface.flags().testFlag(QNetworkInterface::IsLoopBack)?"Loopback":iface.flags().testFlag(QNetworkInterface::IsRunning)?"Link available":"No active link"));
                QStringList addresses;if(reveal->isChecked())for(const auto& a:iface.addressEntries())addresses.append(a.ip().toString());
                table->setItem(row++,3,new QTableWidgetItem(reveal->isChecked()?addresses.join(", "):"Hidden · reveal locally"));
            }
        };
        row->addWidget(button("Refresh interfaces",refresh,w));layout->addLayout(row);layout->addWidget(text("Addresses are hidden by default. Nothing is uploaded.","muted"));layout->addWidget(table,1);
        QObject::connect(reveal,&QCheckBox::toggled,w,[refresh]{refresh();});refresh();tabs->addTab(connections,"Connections");
        auto* diagnostics=toolbox({"Network"});toolboxes.insert("network",diagnostics);tabs->addTab(diagnostics,"Diagnostics");
        auto* throughput=new QWidget;auto* tl=new QVBoxLayout(throughput);tl->addWidget(text("Receive/transmit histories and interface selectors are available in Performance.","muted"));tl->addWidget(button("Open network throughput",[this]{navigate("performance");performance->detailTabs()->setCurrentIndex(4);},w,true));tl->addStretch();tabs->addTab(throughput,"Throughput");
        tabs->addTab(installedTools("Network configuration tools may interrupt connectivity. Review changes in their native windows.",{{"Network connections","nm-connection-editor"},{"Firewall","gufw"},{"Bluetooth","blueman-manager"}}),"Network tools");
        return workspace("Network","Inspect connection state, run reviewed diagnostics, and follow resource history.",tabs);
    }
    QFrame* toolTile(QPushButton* b,const QString& title,bool available) {
        auto* tile=panel("systemToolTile");auto* tl=new QVBoxLayout(tile);tl->setContentsMargins(12,10,12,10);tl->setSpacing(5);
        const auto detail=fluent::descriptionFor(title);
        const auto summary=detail.isEmpty()?QString("Open %1 in its native interface.").arg(title):detail;
        auto command=b->property("systemToolCommand");if(!command.isValid())command=b->property("desktopToolCommand");
        const auto name=command.toString();
        const auto tip=summary+(available?"\nInstalled · opens native utility.":"\nUnavailable · "+dependencyHint(name));
        tile->setToolTip(tip);b->setToolTip(tip);b->setAccessibleDescription(summary);b->setObjectName("systemToolTileButton");b->setIcon(fluent::glyph(fluent::tabGlyph(title)));b->setIconSize(QSize(40,40));b->setMinimumHeight(54);
        tl->addWidget(b);auto* desc=text(summary,"toolDescription");desc->setToolTip(tip);tl->addWidget(desc);
        auto* state=text(available?"Installed · native utility":"Not installed or not trusted","toolAvailability");state->setProperty("available",available);state->setToolTip(tip);tl->addWidget(state);
        auto* dependency=button("Review missing dependency…",[this,name]{reviewDependency(w,name);},w);dependency->setObjectName("toolDependency");
        dependency->setToolTip(dependencyHint(name));tl->addWidget(dependency);dependency->setVisible(!available&&!dependencyPackage(name).isEmpty());tl->addStretch();
        return tile;
    }
    QWidget* installedTools(const QString& description,const QVector<QPair<QString,QString>>& entries) {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);
        layout->addWidget(text(description,"muted"));
        auto* grid=new QGridLayout;grid->setSpacing(12);int index=0;
        for(const auto& entry:entries) {
            const auto program=tux::trustedLauncherExecutable(entry.second);
            auto* b=button(entry.first,[this,entry]{
                const auto executable=tux::trustedLauncherExecutable(entry.second);
                if(executable.isEmpty()){QMessageBox::warning(w,entry.first,"The trusted system tool is no longer available.");return;}
                if(QMessageBox::question(w,entry.first,"Open the installed system utility? It may change your computer. Review its own authorization and operation prompts. Back up data before disk or recovery changes.",QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
                if(tux::trustedLauncherExecutable(entry.second)!=executable){QMessageBox::warning(w,entry.first,"The installed utility changed during review. Reopen it and review again.");return;}
                if(!QProcess::startDetached(executable,QStringList{}))QMessageBox::warning(w,entry.first,"The system utility could not be opened. Check the installed application and desktop session.");
            },w);
            b->setProperty("systemToolCommand",entry.second);
            b->setEnabled(!program.isEmpty());
            b->setToolTip(program.isEmpty()?"Unavailable: install the distribution-supported system tool separately.":"Open the installed tool after review; it manages its own authorization.");
            grid->addWidget(toolTile(b,entry.first,!program.isEmpty()),index/2,index%2);++index;
        }
        layout->addLayout(grid);layout->addStretch();return content;
    }
    QWidget* offlineDiskPage() {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);
        layout->addWidget(text("Offline filesystem check and repair","sectionTitle"));
        layout->addWidget(text("Only unmounted ext2/3/4 volumes with unique UUIDs are offered. Back up data first. Use the Disks utility to unmount normally; SysPulse will never force unmount, repair a mounted system volume, or run an automatic repair. UDisks manages its own desktop authorization.","muted"));
        auto* devices=new QComboBox;devices->setObjectName("offlineFilesystemDevice");layout->addWidget(devices);
        auto* state=text("Refresh to find eligible offline filesystems.","muted");state->setObjectName("offlineFilesystemStatus");state->setTextFormat(Qt::PlainText);layout->addWidget(state);
        auto* refresh=new QPushButton("Refresh offline devices");refresh->setObjectName("offlineFilesystemRefresh");
        auto* check=new QPushButton("Check selected filesystem…");check->setObjectName("offlineFilesystemCheck");check->setEnabled(false);
        auto* repair=new QPushButton("Repair selected filesystem…");repair->setObjectName("offlineFilesystemRepair");repair->setEnabled(false);
        auto* row=new QHBoxLayout;row->addWidget(refresh);row->addWidget(check);row->addWidget(repair);layout->addLayout(row);
        auto inventory=std::make_shared<QVector<offline::Device>>();
        auto checked=std::make_shared<std::optional<offline::Device>>();
        const QStringList listArgs={"--json","--list","--output","PATH,UUID,FSTYPE,MAJ:MIN,TYPE,RO,MOUNTPOINTS,SIZE"};
        const auto reload=[=]{
            checked->reset();inventory->clear();devices->clear();check->setEnabled(false);repair->setEnabled(false);
            const auto ls=tux::trustedSystemExecutable("lsblk");
            if(ls.isEmpty()){state->setText("Trusted lsblk not installed.");return;}
            refresh->setEnabled(false);state->setText("Reading block device inventory…");
            AsyncCommand::run(content,ls,listArgs,12000,262144,[=](const AsyncCommandResult& r){
                refresh->setEnabled(true);
                if(!r.ok()){state->setText("Unable to verify device inventory.");return;}
                *inventory=offline::candidates(r.output);
                for(const auto& d:*inventory)devices->addItem(d.path+" · "+d.type+" · "+d.size+" · "+d.uuid);
                check->setEnabled(!inventory->isEmpty());
                state->setText(inventory->isEmpty()?"No eligible unmounted ext filesystem found.":"Select an offline device and run the read-only check.");
            });
        };
        QObject::connect(refresh,&QPushButton::clicked,content,reload);
        QObject::connect(devices,QOverload<int>::of(&QComboBox::currentIndexChanged),content,[=](int){checked->reset();repair->setEnabled(false);});
        const auto operate=[=](bool fixing){
            const int index=devices->currentIndex();if(index<0||index>=inventory->size()||!refresh->isEnabled())return;
            const auto selected=inventory->at(index);
            if(fixing&&(!*checked||!(checked->value()==selected)))return;
            if(QMessageBox::question(content,fixing?"Review offline repair":"Review offline check",QString("%1\nDevice: %2\nUUID: %3\nType: %4\nSize: %5\n%6").arg(fixing?"Repair may change filesystem metadata and recover or discard damaged data. Confirm that you have a backup.":"Check consistency without repairing the filesystem.").arg(selected.path,selected.uuid,selected.type,selected.size,fixing?"Proceed with repair?":"Proceed with check?"),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
            const auto ls=tux::trustedSystemExecutable("lsblk"),udisks=tux::trustedSystemExecutable("udisksctl"),gdbus=tux::trustedSystemExecutable("gdbus");
            if(ls.isEmpty()||udisks.isEmpty()||gdbus.isEmpty()){state->setText("Required trusted lsblk, udisksctl or gdbus unavailable. Install disk tools first.");return;}
            refresh->setEnabled(false);devices->setEnabled(false);check->setEnabled(false);repair->setEnabled(false);
            const auto end=[=]{refresh->setEnabled(true);devices->setEnabled(true);check->setEnabled(true);};
            state->setText("Revalidating device and mount state…");
            AsyncCommand::run(content,ls,listArgs,12000,262144,[=](const AsyncCommandResult& a){
                if(!a.ok()||!offline::candidates(a.output).contains(selected)){state->setText("Device changed or became mounted. Refresh before retrying.");checked->reset();end();return;}
                AsyncCommand::run(content,udisks,{"info","--block-device",selected.path},10000,65536,[=](const AsyncCommandResult& b){
                    const auto object=b.ok()?offline::objectPath(b.output):QString{};
                    const auto args=offline::arguments(object,fixing);
                    if(args.isEmpty()){state->setText("Could not verify the UDisks device object.");checked->reset();end();return;}
                    // UDisks rejects mounted/unsupported filesystems at execution; no forced unmount.
                    ReviewedProgress progress(content);progress.setWindowTitle(fixing?"Repairing offline filesystem":"Checking offline filesystem");progress.setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint|Qt::WindowTitleHint);
                    auto* pl=new QVBoxLayout(&progress);pl->addWidget(text("Waiting for UDisks and desktop authorization. Do not disconnect the device or shut down."));
                    AsyncCommand::runAction(&progress,gdbus,args,65536,[&](const AsyncCommandResult& c){
                        const int answer=c.ok()?offline::boolReply(c.output):-1;
                        checked->reset();
                        if(answer==1)state->setText(fixing?"UDisks reported repair success. Run a fresh consistency check.":"UDisks reported the filesystem consistent.");
                        else if(answer==0&&!fixing){*checked=selected;state->setText("UDisks reported an inconsistent filesystem. Repair may be reviewed after backup.");}
                        else state->setText("Operation not confirmed. Inspect UDisks/Disks before retrying. "+QString::fromUtf8(c.output).left(1800));
                        progress.done(0);
                    });
                    progress.exec();end();repair->setEnabled(checked->has_value());
                });
            });
        };
        QObject::connect(check,&QPushButton::clicked,content,[operate]{operate(false);});QObject::connect(repair,&QPushButton::clicked,content,[operate]{operate(true);});
        layout->addWidget(button("Open Disks for mount/unmount and SMART information…",[this]{
            const auto exe=tux::trustedSystemExecutable("gnome-disks");if(exe.isEmpty()){reviewPackageGroup("disk");return;}
            if(!QProcess::startDetached(exe,QStringList{}))QMessageBox::warning(w,"Disks","Could not open Disks.");
        },content));
        layout->addStretch();return content;
    }
    void reviewPackageGroup(const QString& group) {
        const QStringList request={"packages",group,"install"};
        auto command=tux::admin::typedCommand(request);
        const auto apt=tux::trustedSystemExecutable("apt-get");
        if(command.isEmpty()||apt.isEmpty()||tux::admin::busy()){QMessageBox::warning(w,"Package operation","Trusted package manager unavailable or administrator action already running.");return;}
        command[0]=apt;
        QStringList previewArgs=command.mid(1);previewArgs.prepend("--simulate");
        QDialog review(w);review.setWindowTitle("Review package plan");review.resize(800,540);
        auto* layout=new QVBoxLayout(&review);layout->addWidget(text("Preview from configured distribution repositories. No third-party repository is added. Removal is forbidden; package versions and dependencies may be re-resolved at install time. Existing configuration files are preserved where dpkg offers a choice.","muted"));
        auto* output=new QPlainTextEdit;output->setReadOnly(true);output->setPlainText("Resolving package transaction…");layout->addWidget(output,1);
        auto* approve=new QPushButton("Install reviewed package group…");approve->setEnabled(false);layout->addWidget(approve);
        auto* cancel=new QPushButton("Cancel");layout->addWidget(cancel);
        QObject::connect(cancel,&QPushButton::clicked,&review,&QDialog::reject);
        QObject::connect(approve,&QPushButton::clicked,&review,&QDialog::accept);
        AsyncCommand::run(&review,apt,previewArgs,60000,65536,[=](const AsyncCommandResult& result){
            output->setPlainText(QString::fromUtf8(result.output)+(result.ok()?"":"\nPreview failed, was incomplete, or exceeded the limit. Refresh package lists or use the distribution package manager."));
            approve->setEnabled(result.ok());
        });
        if(review.exec()!=QDialog::Accepted)return;
        if(!tux::admin::active()||tux::admin::busy()){
            QMessageBox::information(w,"Administrator session required","Unlock the SysPulse administrator session first, then review this package plan again. The noninteractive installation uses the protected session and does not store your password.");
            openSettingsSection("adminSessionStatus");return;
        }
        if(tux::trustedSystemExecutable("apt-get")!=apt)return;
        ReviewedProgress progress(w);progress.setWindowTitle("Applying reviewed package transaction");progress.setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint|Qt::WindowTitleHint);
        auto* pl=new QVBoxLayout(&progress);pl->addWidget(text("APT is running. Do not close SysPulse or interrupt power. Package transactions may take time."));
        AsyncCommandResult outcome;
        tux::admin::runReviewed(&progress,request,tux::trustedSystemExecutable("pkexec"),command,65536,[&](const AsyncCommandResult& result){outcome=result;progress.done(0);});
        progress.exec();refreshActionIndicators();
        QMessageBox report(w);report.setWindowTitle("Package transaction result");report.setIcon(outcome.exitCode==0?QMessageBox::Information:QMessageBox::Warning);report.setTextFormat(Qt::PlainText);
        report.setText(outcome.exitCode==0?"APT reported a successful exit. Review the installed applications.":"APT did not confirm completion. Review the package manager and do not retry blindly.");
        report.setDetailedText(QString::fromUtf8(outcome.output)+(!outcome.error.isEmpty()?"\n"+outcome.error:QString())+(outcome.truncated?"\nOutput truncated.":QString()));report.exec();
    }
    QWidget* inputControlPage() {
        auto* page=new QWidget;auto* layout=new QVBoxLayout(page);
        layout->addWidget(text("Physical X11 input controls","sectionTitle"));
        layout->addWidget(text("Enable a detected internal keyboard or touchpad, or temporarily disable it for 15 seconds. An independent recovery process attempts re-enable even if the main helper exits. Keep an alternate input device available. Permanent Xorg/udev rules are deliberately not written.","muted"));
        auto* list=new QComboBox;list->setObjectName("physicalInputDevices");layout->addWidget(list);
        auto* state=text("Refresh to inventory eligible input devices.","muted");state->setTextFormat(Qt::PlainText);layout->addWidget(state);
        auto* refresh=new QPushButton("Refresh physical input devices");auto* enable=new QPushButton("Enable selected device…");auto* test=new QPushButton("Test disable for 15 seconds…");
        enable->setEnabled(false);test->setEnabled(false);auto* row=new QHBoxLayout;row->addWidget(refresh);row->addWidget(enable);row->addWidget(test);layout->addLayout(row);
        const auto load=[=]{
            list->clear();enable->setEnabled(false);test->setEnabled(false);
            const auto helper=tux::trustedSystemExecutable("syspulse-input-control");
            if(helper.isEmpty()){state->setText("Trusted input helper unavailable.");return;}
            refresh->setEnabled(false);
            AsyncCommand::run(page,helper,{"list"},35000,262144,[=](const AsyncCommandResult& r){
                refresh->setEnabled(true);QJsonParseError error;const auto doc=QJsonDocument::fromJson(r.output,&error);
                if(!r.ok()||error.error!=QJsonParseError::NoError||!doc.isArray()){state->setText("Input inventory unavailable. Requires a local X11 desktop and xinput. "+QString::fromUtf8(r.output).left(500));return;}
                for(const auto& v:doc.array()){
                    const auto d=v.toObject();const int id=d.value("id").toInt(-1);const auto name=d.value("name").toString();const int active=d.value("enabled").toInt(-1);
                    if(id<1||id>65535||name.isEmpty()||name.size()>256||(active!=0&&active!=1))continue;
                    list->addItem(name+QString(active?" · enabled":" · disabled"),d.toVariantMap());
                }
                enable->setEnabled(list->count()>0);test->setEnabled(list->count()>0);state->setText(list->count()?"Select the exact device.":"No eligible internal keyboard or touchpad found.");
            });
        };
        QObject::connect(refresh,&QPushButton::clicked,page,load);
        const auto act=[=](bool disable){
            const auto d=list->currentData().toMap();const int id=d.value("id").toInt();const auto name=d.value("name").toString();
            const auto helper=tux::trustedSystemExecutable("syspulse-input-control");
            if(id<1||id>65535||name.isEmpty()||helper.isEmpty())return;
            if(QMessageBox::question(page,"Review input operation",QString("%1\nDevice: %2 (ID %3)\n%4").arg(disable?"Temporarily disable this device?":"Enable this device?",name).arg(id).arg(disable?"Automatic restoration is attempted after 15 seconds; keep an alternate input device available.":"The current device identity will be checked before enabling."),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
            ReviewedProgress progress(page);progress.setWindowTitle("Physical input operation");progress.setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint|Qt::WindowTitleHint);
            auto* pl=new QVBoxLayout(&progress);pl->addWidget(text(disable?"Testing input disable and recovery. Do not close the desktop session.":"Enabling and verifying device state."));
            QString outcome;
            AsyncCommand::runAction(&progress,helper,{disable?"test-disable":"enable",QString::number(id),name},4096,[&](const AsyncCommandResult& r){outcome=(r.ok()?"Verified: ":"Not verified: ")+QString::fromUtf8(r.output).left(800);progress.done(0);});
            progress.exec();QMessageBox::information(page,"Input result",outcome);load();
        };
        QObject::connect(enable,&QPushButton::clicked,page,[act]{act(false);});QObject::connect(test,&QPushButton::clicked,page,[act]{act(true);});
        layout->addStretch();return page;
    }
    QWidget* desktopPage() {
        auto* tabs=new QTabWidget;tabs->setObjectName("desktopWorkspaceTabs");auto* settings=new QWidget;auto* layout=new QVBoxLayout(settings);
        layout->addWidget(text("Open installed XFCE settings tools. Changes are made through their native interfaces.","muted"));
        auto* grid=new QGridLayout;grid->setSpacing(12);int index=0;
        const QVector<QPair<QString,QString>> apps={{"Display","xfce4-display-settings"},{"Keyboard","xfce4-keyboard-settings"},{"Mouse & touchpad","xfce4-mouse-settings"},{"Power manager","xfce4-power-manager-settings"},{"Panel","xfce4-panel"},{"Session","xfce4-session-settings"},{"Appearance","xfce4-appearance-settings"},{"Window manager","xfwm4-settings"}};
        for(const auto& item:apps) {
            const auto program=tux::trustedLauncherExecutable(item.second);
            auto* b=button(item.first,[this,item]{
                const auto program=tux::trustedLauncherExecutable(item.second);
                if(program.isEmpty()){QMessageBox::warning(w,item.first,"The trusted XFCE settings utility is unavailable.");return;}
                QStringList args;if(item.second=="xfce4-panel")args<<"--preferences";
                if(!QProcess::startDetached(program,args))QMessageBox::warning(w,item.first,"The desktop settings tool could not be opened.");
            },w);
            b->setEnabled(!program.isEmpty());b->setProperty("desktopToolCommand",item.second);grid->addWidget(toolTile(b,item.first,!program.isEmpty()),index/3,index%3);++index;
        }
        layout->addLayout(grid);layout->addStretch();auto* desktopScroll=new QScrollArea;desktopScroll->setWidgetResizable(true);desktopScroll->setWidget(settings);tabs->addTab(desktopScroll,"Desktop settings");
        auto* diagnostic=toolbox({"Performance","Input","Gaming","Desktop"});toolboxes.insert("desktop",diagnostic);tabs->addTab(diagnostic,"Diagnostics & integrations");
        tabs->addTab(installedTools("Administration tools are optional and run separately using their own authorization.",{{"Settings manager","xfce4-settings-manager"},{"Desktop preferences","xfdesktop-settings"},{"Driver manager","driver-manager"},{"Users and groups","users-admin"},{"Mint users","mintusers"},{"Login window","lightdm-settings"},{"Printers","system-config-printer"},{"Hardware information","hardinfo"},{"Password and keys","seahorse"}}),"Administration tools");
        auto* integrations=new QWidget;auto* il=new QVBoxLayout(integrations);
        il->addWidget(text("Gaming setup · native package installation","sectionTitle"));
        il->addWidget(text("Review and install fixed package groups from configured distribution repositories. No WineHQ key, PPA or unsigned repository is added. An unlocked administrator session is required for installation.","muted"));
        const QVector<QPair<QString,QString>> groups={{"Wine, Winetricks and Cabextract","wine"},{"Lutris","lutris"},{"Vulkan, Mesa and GameMode","graphics"},{"TLP power management","power"},{"Preload application accelerator","preload"}};
        for(const auto& g:groups)il->addWidget(button("Review/install "+g.first,[this,g]{reviewPackageGroup(g.second);},integrations));
        il->addWidget(text("Brave · bounded cache launch","sectionTitle"));
        auto* cap=new QSpinBox;cap->setRange(64,2048);cap->setValue(256);cap->setSuffix(" MiB");cap->setObjectName("braveCacheCap");il->addWidget(cap);
        auto* brave=button("Launch Brave with selected cache limit…",[this,cap]{
            auto exe=tux::trustedSystemExecutable("brave-browser");if(exe.isEmpty())exe=tux::trustedSystemExecutable("brave-browser-stable");
            if(exe.isEmpty()){QMessageBox::warning(w,"Brave","Trusted Brave browser installation not found.");return;}
            const auto bytes=QString::number(static_cast<qint64>(cap->value())*1048576);
            if(QMessageBox::question(w,"Launch bounded-cache Brave",QString("Close all Brave windows and background browser processes first. Launch Brave with --disk-cache-size=%1? Existing browser instances may ignore new launch flags. This does not alter security settings or delete browsing data.").arg(bytes),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
            if(!QProcess::startDetached(exe,{"--disk-cache-size="+bytes}))QMessageBox::warning(w,"Brave","Could not start browser.");
        },integrations,true);brave->setObjectName("launchBraveBounded");il->addWidget(brave);
        il->addWidget(button("Review browser cache in Cleaner",[this]{
            navigate("storage");auto* outer=pages.value("storage")->findChild<QTabWidget*>("storageWorkspaceTabs");if(!outer)return;outer->setCurrentIndex(2);
            auto* mode=outer->findChild<QComboBox*>("cleanerMode");auto* root=outer->findChild<QLineEdit*>("cleanerRoot");
            if(mode&&root){mode->setCurrentIndex(3);root->setText(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)+"/BraveSoftware/Brave-Browser");}
        },integrations));
        il->addStretch();auto* integrationScroll=new QScrollArea;integrationScroll->setWidgetResizable(true);integrationScroll->setWidget(integrations);tabs->addTab(integrationScroll,"Gaming & browser");tabs->addTab(inputControlPage(),"Input controls");
        return workspace("XFCE desktop","Desktop, input, power and installed Linux integrations.",tabs);
    }
    QWidget* recoveryPage() {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);auto* frame=panel();auto* fl=new QVBoxLayout(frame);fl->setContentsMargins(20,20,20,20);
        fl->addWidget(text("Startup overrides","sectionTitle"));fl->addWidget(text("Review the selected application in Startup apps to restore its system default or inspect an available undo record. Recovery refuses externally changed targets.","muted"));fl->addWidget(button("Review startup recovery",[this]{navigate("management");management->showStartup();},w,true));layout->addWidget(frame);
        auto* limits=panel();auto* ll=new QVBoxLayout(limits);ll->setContentsMargins(20,20,20,20);ll->addWidget(text("Recovery availability","sectionTitle"));ll->addWidget(text("Cleanup and process termination cannot be undone here. Snapshot creation and general configuration rollback are planned; no backup is created automatically.","muted"));layout->addWidget(limits);
        layout->addWidget(button("Review/install snapshot and backup utilities…",[this]{reviewPackageGroup("backup");},content));
        layout->addWidget(installedTools("Recover files to a different disk. Timeshift system snapshots are not personal-file backups.",{{"Timeshift","timeshift-gtk"},{"File recovery","qphotorec"},{"Personal file backups","deja-dup"}}),1);layout->addStretch();
        return workspace("Recovery","Use recovery only where a supported saved state exists.",content);
    }
    QWidget* reportsPage() {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);layout->setContentsMargins(0,0,0,0);
        auto* row=new QHBoxLayout;reportStatus=text("No completed tasks in this session.","muted");reportStatus->setObjectName("reportsStatus");row->addWidget(reportStatus,1);
        auto* exportButton=button("Review export…",[this]{exportReports();},w,true);exportButton->setObjectName("reviewReports");row->addWidget(exportButton);auto* share=button("Review diagnostic sharing…",[this]{diagnostic::reviewSharing(w,results);},w);share->setObjectName("reviewDiagnosticSharing");row->addWidget(share);layout->addLayout(row);
        reports=new QTableWidget(0,4);reports->setObjectName("sessionReports");reports->setHorizontalHeaderLabels({"Task","Outcome","Duration","Record"});reports->verticalHeader()->hide();reports->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);reports->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);reports->setColumnWidth(1,170);reports->setEditTriggers(QAbstractItemView::NoEditTriggers);reports->setSelectionBehavior(QAbstractItemView::SelectRows);layout->addWidget(reports,1);
        layout->addWidget(text("The latest 20 task results stay in memory for this session. Summary is the default export; diagnostic details require a fresh opt-in. No report is sent automatically.","muted"));
        return workspace("Reports","Review analysis and tool outcomes before saving or sharing.",content);
    }
    void openMemoryOptimizer() {
        navigate("control");
        if(auto* tabs=pages.value("control")->findChild<QTabWidget*>("controlPanelTabs"))
            tabs->setCurrentIndex(0);
    }
    QWidget* memoryOptimizerPage() {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);
        auto* frame=panel();auto* fl=new QVBoxLayout(frame);fl->setContentsMargins(20,20,20,20);
        fl->addWidget(text("System-wide memory optimizer","sectionTitle"));
        fl->addWidget(text("Measure the entire system and review kernel memory compression. The kernel can compress future swap writes to reduce disk I/O under pressure; this is not a guaranteed instant increase in free RAM.","muted"));
        auto* status=text("Ready. Measure system RAM, swap and compression eligibility.","muted");
        status->setObjectName("memoryOptimizerStatus");status->setTextFormat(Qt::PlainText);fl->addWidget(status);
        const auto describe=[]{
            const auto snapshot=syspulse::triage::measure();const auto pressure=syspulse::pressure::measure();
            const auto available=snapshot.ram?QString::number(snapshot.ram->available/1024.0,'f',1)+" MiB":"Unknown";
            const auto swap=snapshot.swap?QString::number(*snapshot.swap/1024.0,'f',1)+" MiB":"Unknown";
            const auto stalls=pressure[1].some?QString::number(pressure[1].some->avg10,'f',2)+"%":"Unknown";
            std::optional<std::uint64_t> total,free;
            std::optional<double> psi;
            if(snapshot.ram){total=snapshot.ram->total;free=snapshot.ram->available;}
            if(pressure[1].some)psi=pressure[1].some->avg10;
            const auto plan=syspulse::memoryplan::assess(total,free,psi);
            return QString("System available RAM: %1 · Configured swap: %2\nMemory stall time (10s): %3\n%4").arg(available,swap,stalls,QString::fromLatin1(plan.advice));
        };
        auto* kernelStatus=text("","muted");kernelStatus->setObjectName("systemMemoryStatus");kernelStatus->setTextFormat(Qt::PlainText);fl->addWidget(kernelStatus);
        auto* enable=button("Optimize system RAM (review zswap)…",[this]{locateMaintenanceTool("zswap-enable");},w,true);
        enable->setObjectName("optimizeSystemMemory");fl->addWidget(enable);
        auto* disable=button("Review disabling new zswap compression…",[this]{locateMaintenanceTool("zswap-disable");},w);
        disable->setObjectName("disableSystemMemoryCompression");fl->addWidget(disable);
        const auto refreshKernel=[kernelStatus,enable,disable]{
            using S=syspulse::system_memory::State;
            const auto state=syspulse::system_memory::readEnabled();
            const auto enableProblem=tux::unavailable(tux::action("zswap-enable"));
            const auto disableProblem=tux::unavailable(tux::action("zswap-disable"));
            enable->setEnabled(enableProblem.isEmpty());disable->setEnabled(disableProblem.isEmpty());
            enable->setToolTip(enableProblem);disable->setToolTip(disableProblem);
            kernelStatus->setText(QString("System-wide zswap: %1. %2").arg(state==S::Enabled?"Enabled":state==S::Disabled?"Disabled":"Unavailable / unknown",state==S::Enabled?"Compression is active; new swap writes can be compressed.":enableProblem.isEmpty()?"Compatible configuration detected; review and authorize to enable.":enableProblem));
        };
        refreshMemoryKernel=refreshKernel;refreshKernel();
        auto* swapPanel=panel();auto* swapLayout=new QVBoxLayout(swapPanel);
        swapLayout->addWidget(text("Runtime swap tuning · readback and restore","sectionTitle"));
        auto* swapStatus=text("Read the current kernel value before applying a profile.","muted");swapStatus->setObjectName("runtimeSwappinessStatus");swapLayout->addWidget(swapStatus);
        auto* swapValue=new QSpinBox;swapValue->setRange(0,200);swapValue->setValue(60);swapValue->setObjectName("runtimeSwappinessValue");swapValue->setAccessibleName("Runtime swappiness 0 to 200");
        auto* readSwap=new QPushButton("Read current swappiness");readSwap->setObjectName("readRuntimeSwappiness");
        auto* applySwap=new QPushButton("Apply reviewed value…");applySwap->setObjectName("applyRuntimeSwappiness");applySwap->setEnabled(false);
        auto* restoreSwap=new QPushButton("Restore captured value…");restoreSwap->setObjectName("restoreRuntimeSwappiness");restoreSwap->setEnabled(false);
        auto* swapRow=new QHBoxLayout;swapRow->addWidget(readSwap);swapRow->addWidget(swapValue);swapRow->addWidget(applySwap);swapRow->addWidget(restoreSwap);swapLayout->addLayout(swapRow);
        swapLayout->addWidget(text("Runtime only. Lower values discourage swap; values above 100 may suit measured compressed-swap workloads. Do not assume one value is best for all HDDs, SSDs or zRAM. Changes persist until changed or rebooted; restore uses the value captured in this session.","muted"));
        const auto readValue=[]()->int{QFile file("/proc/sys/vm/swappiness");if(!file.open(QIODevice::ReadOnly))return -1;bool ok=false;const int value=file.read(32).trimmed().toInt(&ok);return ok&&value>=0&&value<=200?value:-1;};
        auto originalSwap=std::make_shared<int>(-1);auto lastApplied=std::make_shared<int>(-1);
        QObject::connect(readSwap,&QPushButton::clicked,content,[=]{const int now=readValue();if(now<0){swapStatus->setText("Swappiness unavailable.");applySwap->setEnabled(false);restoreSwap->setEnabled(false);return;}if(*originalSwap<0){*originalSwap=now;}swapValue->setValue(now);applySwap->setEnabled(true);restoreSwap->setEnabled(*lastApplied>=0&&now==*lastApplied&&now!=*originalSwap);swapStatus->setText(QString("Current: %1 · Original session value: %2").arg(now).arg(*originalSwap));});
        const auto applyValue=[=](bool restore){
            if(tux::admin::busy())return;
            const int before=readValue(),target=restore?*originalSwap:swapValue->value();
            if(before<0||*originalSwap<0||target<0||target>200||(restore&&before!=*lastApplied)){swapStatus->setText("Current state changed or is unavailable; read again before proceeding.");return;}
            if(before==target){swapStatus->setText("Requested value already active.");return;}
            const QStringList request={"memory","swappiness",QString::number(target)};
            auto command=tux::admin::typedCommand(request);const auto auth=tux::trustedSystemExecutable("pkexec"),sysctl=tux::trustedSystemExecutable("sysctl");
            if(command.isEmpty()||auth.isEmpty()||sysctl.isEmpty()){swapStatus->setText("Required trusted administrator utilities unavailable.");return;}
            command[0]=sysctl;
            if(QMessageBox::question(content,"Review runtime swap change",QString("Change vm.swappiness from %1 to %2 for the current boot? No persistent configuration files will be changed.").arg(before).arg(target),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
            if(readValue()!=before||tux::admin::busy()){swapStatus->setText("System state changed during review. Read again.");return;}
            ReviewedProgress progress(content);progress.setWindowTitle("Applying runtime memory setting");progress.setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint|Qt::WindowTitleHint);
            auto* pl=new QVBoxLayout(&progress);pl->addWidget(text("Waiting for administrator authorization and kernel readback…"));
            applySwap->setEnabled(false);restoreSwap->setEnabled(false);readSwap->setEnabled(false);
            tux::admin::runReviewed(&progress,request,auth,command,4096,[&](const AsyncCommandResult& result){
                const int after=readValue();if(result.ok()&&after==target){*lastApplied=after;swapStatus->setText(QString("Verified kernel value: %1 · Original: %2").arg(after).arg(*originalSwap));}else swapStatus->setText(QString("Change not verified. Current kernel value: %1. Review before retrying.").arg(after));
                if(after>=0)swapValue->setValue(after);
                progress.done(0);
            });
            // A modal action must remain alive until the worker returns.
            progress.exec();readSwap->setEnabled(true);applySwap->setEnabled(readValue()>=0);restoreSwap->setEnabled(*lastApplied>=0&&readValue()==*lastApplied&&*lastApplied!=*originalSwap);
            refreshActionIndicators();
        };
        QObject::connect(applySwap,&QPushButton::clicked,content,[applyValue]{applyValue(false);});
        QObject::connect(restoreSwap,&QPushButton::clicked,content,[applyValue]{applyValue(true);});
        layout->addWidget(swapPanel);
        auto* activity=text("Swap activity not measured. Kernel paging counters are not physical disk throughput.","muted");
        activity->setObjectName("swapActivityStatus");activity->setTextFormat(Qt::PlainText);fl->addWidget(activity);
        auto* sample=new QPushButton("Measure system-wide swap activity (2 seconds)");
        sample->setObjectName("measureSwapActivity");fl->addWidget(sample);
        QObject::connect(sample,&QPushButton::clicked,content,[content,sample,activity]{
            using namespace syspulse::swap_activity;
            const auto before=read();const auto page=pageSize();
            if(!before||page<=0){activity->setText("Swap activity unavailable: kernel counters or page size could not be read.");return;}
            QElapsedTimer clock;clock.start();sample->setEnabled(false);
            activity->setText("Measuring system-wide swap activity…");
            QTimer::singleShot(2000,content,[sample,activity,before,page,clock]{
                sample->setEnabled(true);
                const auto after=read();const double seconds=clock.elapsed()/1000.0;
                const auto result=after?rates(*before,*after,seconds,page):std::optional<Rates>{};
                if(!result){activity->setText("Swap activity unavailable: counters reset, interval invalid or read failed.");return;}
                activity->setText(QString("Swap activity over %1 s: in %2 KiB/s · out %3 KiB/s. Kernel paging, not necessarily disk I/O. Zero means no observed paging in this interval.").arg(seconds,0,'f',2).arg(result->inKiB,0,'f',1).arg(result->outKiB,0,'f',1));
            });
        });
        auto* actions=new QHBoxLayout;
        auto* measure=button("Measure system memory",[status,describe,refreshKernel]{status->setText(describe());refreshKernel();},content);
        measure->setObjectName("measureMemory");actions->addWidget(measure);
        auto* optimize=button("Reduce SysPulse only (optional)",[status,describe]{
            const auto before=syspulse::triage::measure();
            const auto result=syspulse::relief::trimSelf();
            const auto after=syspulse::triage::measure();
            const auto show=[](const std::optional<std::uint64_t>& n){return n?QString::number(*n/1024.0,'f',1)+" MiB":"Unknown";};
            const QString detail=!result.supported?"Allocator trim unavailable.":result.released?"Allocator reports unused pages released.":"No allocator pages released.";
            const auto available=[](const syspulse::triage::Snapshot& v)->std::optional<std::uint64_t>{if(v.ram)return v.ram->available;return {};};
            status->setText(QString("SysPulse RSS before: %1 · after: %2\nSystem available RAM before: %3 · after: %4\n%5 System RAM changes include other workloads; no speed gain is guaranteed.\n%6").arg(show(result.before),show(result.after),show(available(before)),show(available(after)),detail,describe()));
        },content);
        optimize->setObjectName("optimizeMemory");actions->addWidget(optimize);fl->addLayout(actions);layout->addWidget(frame);
        auto* system=panel();auto* sl=new QVBoxLayout(system);sl->setContentsMargins(20,20,20,20);
        sl->addWidget(text("System-wide memory and responsiveness","sectionTitle"));
        sl->addWidget(text("For larger savings, review memory-heavy applications, startup entries and compressed swap. Close only workloads you choose; system changes use their normal review and authorization.","muted"));
        auto* review=button("Review largest memory consumers",[this]{navigate("performance");performance->detailTabs()->setCurrentIndex(5);processes->reviewMemoryConsumers();},w);
        review->setObjectName("reviewMemoryConsumers");sl->addWidget(review);
        auto* pause=button("Pause SysPulse resource sampling",[this]{performance->setPaused(true);},w);
        pause->setObjectName("pauseOptimizerSampling");sl->addWidget(pause);
        auto* resume=button("Resume SysPulse resource sampling",[this]{performance->setPaused(false);},w);
        resume->setObjectName("resumeOptimizerSampling");sl->addWidget(resume);
        sl->addWidget(button("Review startup applications",[this]{navigate("management");management->showStartup();},w));
        sl->addWidget(button("Inspect compressed swap",[this]{findTool("Compressed swap status");},w));
        sl->addWidget(button("Open memory history",[this]{navigate("performance");performance->detailTabs()->setCurrentIndex(2);},w));
        layout->addWidget(system);layout->addStretch();return content;
    }
    void openControlTab(const QString& name) {
        navigate("control");
        if(auto* tabs=pages.value("control")->findChild<QTabWidget*>("controlPanelTabs"))
            for(int i=0;i<tabs->count();++i)if(tabs->widget(i)->objectName()==name){tabs->setCurrentIndex(i);break;}
    }
    void locateMaintenanceTool(const QString& id) {
        const auto& action=tux::action(id);
        const auto page=destination(action);
        auto* tools=toolboxes.value(page,nullptr);
        if(!tools||!navigate(page)||!tools->locateTool(id))return;
        for(auto* p=tools->parentWidget();p;p=p->parentWidget()){
            if(auto* tabs=qobject_cast<QTabWidget*>(p)){
                for(int i=0;i<tabs->count();++i)if(tabs->widget(i)==tools||tabs->widget(i)->isAncestorOf(tools)){tabs->setCurrentIndex(i);break;}
                break;
            }
        }
    }
    QWidget* maintenanceCenterPage() {
        auto* content=new QWidget;content->setObjectName("maintenanceCenterPage");
        auto* layout=new QVBoxLayout(content);
        layout->addWidget(text("Maintenance & boost","sectionTitle"));
        layout->addWidget(text("Measure first, review the exact operation, then remeasure. No package removal, cache deletion or desktop change runs from this page automatically.","muted"));
        auto* table=new QTableWidget(3,3);table->setObjectName("maintenanceComparison");
        table->setHorizontalHeaderLabels({"Measurement","Baseline","Latest"});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->verticalHeader()->hide();
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        const QStringList names={"Available RAM","Swap used / configured","APT archive logical size"};
        for(int i=0;i<names.size();++i){table->setItem(i,0,new QTableWidgetItem(names[i]));table->setItem(i,1,new QTableWidgetItem("Not measured"));table->setItem(i,2,new QTableWidgetItem("Not measured"));}
        auto* stamp=text("Capture a baseline before applying maintenance.","muted");stamp->setObjectName("maintenanceStatus");
        auto* baseline=new QPushButton("Capture baseline");baseline->setObjectName("maintenanceBaseline");
        auto* compare=new QPushButton("Recheck measurements");compare->setObjectName("maintenanceRecheck");compare->setEnabled(false);
        auto* row=new QHBoxLayout;row->addWidget(baseline);row->addWidget(compare);row->addStretch();layout->addLayout(row);
        const auto measure=[table,stamp](int column){
            const auto ram=syspulse::triage::measure().ram;
            const auto swap=syspulse::preview::currentSwap();
            const auto cache=syspulse::preview::archives("/var/cache/apt/archives");
            const auto mib=[](double bytes){return QString::number(bytes/1048576.0,'f',2)+" MiB";};
            const auto available=ram?mib(ram->available*1024.0):QString("Unknown");
            const auto used=swap?mib((swap->total-swap->free)*1024.0)+" / "+mib(swap->total*1024.0):QString("Unknown");
            const auto cached=cache.complete?mib(cache.bytes)+" · "+QString::number(cache.files)+" files":QString("Unknown / incomplete");
            const QStringList values={available,used,cached};
            for(int i=0;i<values.size();++i)table->item(i,column)->setText(values[i]);
            stamp->setText(QString("%1 captured at %2. Other workloads may change readings. Archive lengths are not guaranteed freed disk space.").arg(column==1?"Baseline":"Latest",QDateTime::currentDateTime().toString("HH:mm:ss")));
        };
        QObject::connect(baseline,&QPushButton::clicked,content,[table,compare,measure]{
            measure(1);for(int i=0;i<3;++i)table->item(i,2)->setText("Not measured");compare->setEnabled(true);
        });
        QObject::connect(compare,&QPushButton::clicked,content,[measure]{measure(2);});
        layout->addWidget(table,1);layout->addWidget(stamp);
        auto* tools=new QGridLayout;
        const auto add=[this,tools](int r,int c,const QString& title,const QString& id,const char* name){
            auto* b=button(title,[this,id]{locateMaintenanceTool(id);},w);b->setObjectName(name);tools->addWidget(b,r,c);
        };
        add(0,0,"Preview unused packages","apt-preview","maintenanceAptPreview");
        add(0,1,"Review downloaded-package cleanup","apt-clean","maintenanceAptClean");
        add(1,0,"Review journal disk usage","journal-size","maintenanceJournal");
        add(1,1,"Review SSD trim","trim","maintenanceTrim");
        add(2,0,"Review compositor off (may reduce overhead)","compositor-off","maintenanceCompositorOff");
        add(2,1,"Review compositor on (restore effect)","compositor-on","maintenanceCompositorOn");
        add(3,0,"Inspect compressed swap","zram","maintenanceZram");
        auto* memory=button("Review largest memory consumers",[this]{navigate("performance");performance->detailTabs()->setCurrentIndex(5);processes->reviewMemoryConsumers();},w);
        tools->addWidget(memory,3,1);
        add(4,0,"Refresh installed software sources","apt-refresh","maintenanceAptRefresh");
        add(4,1,"Remove obsolete APT downloads","apt-autoclean","maintenanceAptAutoclean");
        add(5,0,"Cap archived journal size (100 MiB)","journal-cap","maintenanceJournalCap");
        add(5,1,"Prune archived journals (7 days)","journal-prune","maintenanceJournalPrune");
        add(6,0,"Enable weekly SSD maintenance","trim-enable","maintenanceTrimEnable");
        add(6,1,"Disable weekly SSD maintenance","trim-disable","maintenanceTrimDisable");
        layout->addLayout(tools);
        auto* packageRow=new QHBoxLayout;packageRow->addWidget(button("Preview and repair broken packages…",[this]{reviewPackageGroup("repair");},content));packageRow->addWidget(button("Install disk maintenance tools…",[this]{reviewPackageGroup("disk");},content));layout->addLayout(packageRow);
        layout->addWidget(text("Actions execute after explicit review. Package cleanup removes downloaded archives, not installed programs; log pruning permanently removes archived logs. Compositor changes can cause tearing; the opposite reviewed action is available. Do not disable swap or drop useful kernel caches.","muted"));
        return content;
    }
    QWidget* systemCarePage() {
        auto* content=new QWidget;content->setObjectName("systemCarePage");
        auto* layout=new QVBoxLayout(content);
        layout->addWidget(text("System Care Center","sectionTitle"));
        layout->addWidget(text("Run a local read-only snapshot. Review the most urgent resource findings first, then open the relevant existing controls.","muted"));
        auto* status=text("No snapshot taken. Run a quick scan to measure six resource indicators.","muted");status->setObjectName("careStatus");layout->addWidget(status);
        auto* table=new QTableWidget(0,4);table->setObjectName("careFindings");
        table->setHorizontalHeaderLabels({"Indicator","Measurement","Priority","Recommended action"});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->verticalHeader()->hide();table->setWordWrap(true);table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
        auto* scan=new QPushButton("Quick scan");scan->setObjectName("runCareScan");scan->setProperty("primary",true);
        auto* review=new QPushButton("Review selected finding");review->setObjectName("reviewCareFinding");review->setEnabled(false);
        auto* row=new QHBoxLayout;row->addWidget(scan);row->addWidget(review);row->addStretch();layout->addLayout(row);layout->addWidget(table,1);
        layout->addWidget(text("Thresholds are advisory, not a complete diagnosis. Missing readings stay unknown. The scan makes no system changes and sends no data.","muted"));
        QObject::connect(table,&QTableWidget::itemSelectionChanged,content,[table,review]{review->setEnabled(table->currentRow()>=0&&!table->selectedItems().isEmpty());});
        QObject::connect(scan,&QPushButton::clicked,content,[table,status,review]{
            const auto snapshot=syspulse::triage::measure();const auto psi=syspulse::pressure::measure();
            const auto findings=syspulse::care::plan(snapshot,psi);
            const auto pct=[](const std::optional<syspulse::triage::Counters>& c){return syspulse::triage::valid(c)?QString::number(static_cast<double>(syspulse::triage::availablePercent(*c)),'f',1)+"% available":QString("Unknown");};
            const auto stalls=[](const syspulse::pressure::Snapshot& p){return p.some?QString::number(p.some->avg10,'f',2)+"% (10s)":QString("Unknown");};
            const QMap<QString,QString> values={{"Available RAM",pct(snapshot.ram)},{"Root filesystem space",pct(snapshot.root)},{"Swap configuration",snapshot.swap?QString::number(*snapshot.swap/1024.0,'f',1)+" MiB":QString("Unknown")},{"CPU stalls",stalls(psi[0])},{"Memory stalls",stalls(psi[1])},{"I/O stalls",stalls(psi[2])}};
            table->clearSelection();table->setRowCount(static_cast<int>(findings.size()));
            int urgent=0,warnings=0,unknown=0,r=0;
            using S=syspulse::triage::Severity;
            for(const auto& f:findings){
                const QString title=QString::fromLatin1(f.title);
                const QString level=f.severity==S::Critical?"High priority":f.severity==S::Warning?"Review":f.severity==S::Unknown?"Unknown":f.severity==S::Info?"Information":"Below threshold";
                urgent+=f.severity==S::Critical;warnings+=f.severity==S::Warning;unknown+=f.severity==S::Unknown;
                auto* item=new QTableWidgetItem(title);item->setData(Qt::UserRole,static_cast<int>(f.route));
                table->setItem(r,0,item);table->setItem(r,1,new QTableWidgetItem(values.value(title,"Unknown")));
                table->setItem(r,2,new QTableWidgetItem(level));table->setItem(r,3,new QTableWidgetItem(QString::fromLatin1(f.advice)));++r;
            }
            table->setCurrentItem(nullptr);table->clearSelection();review->setEnabled(false);table->resizeRowsToContents();
            status->setText(QString("Snapshot %1 · %2 high priority · %3 review · %4 unknown.").arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(urgent).arg(warnings).arg(unknown));
        });
        QObject::connect(review,&QPushButton::clicked,content,[this,table]{
            const int r=table->currentRow();if(r<0||table->selectedItems().isEmpty()||!table->item(r,0))return;
            using R=syspulse::care::Route;
            switch(static_cast<R>(table->item(r,0)->data(Qt::UserRole).toInt())){
            case R::Memory:openMemoryOptimizer();break;
            case R::Storage:navigate("storage");pages["storage"]->findChild<QTabWidget*>("storageWorkspaceTabs")->setCurrentIndex(0);break;
            case R::Swap:locateMaintenanceTool("zram");break;
            case R::Cpu:navigate("performance");performance->detailTabs()->setCurrentIndex(1);break;
            case R::Io:navigate("performance");performance->detailTabs()->setCurrentIndex(3);break;
            }
        });
        return content;
    }
    QWidget* settingsIndex(QTabWidget* tabs) {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);
        auto* filter=new QLineEdit;filter->setObjectName("settingsIndexSearch");filter->setAccessibleName("Search all native settings");filter->setPlaceholderText("Search settings, category or availability");filter->setClearButtonEnabled(true);
        layout->addWidget(filter);
        auto* status=text("","muted");status->setObjectName("settingsIndexStatus");layout->addWidget(status);
        auto* table=new QTableWidget(0,3);table->setObjectName("settingsIndexTable");
        table->setHorizontalHeaderLabels({"Setting","Category","Availability"});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);table->verticalHeader()->hide();
        table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
        QVector<QPushButton*> controls;QVector<int> categories;
        for(int i=0;i<tabs->count();++i)for(auto* control:tabs->widget(i)->findChildren<QPushButton*>()){
            if(!control->property("systemToolCommand").isValid())continue;
            const int row=table->rowCount();table->insertRow(row);
            table->setItem(row,0,new QTableWidgetItem(control->accessibleName()));
            table->setItem(row,1,new QTableWidgetItem(tabs->tabText(i)));
            table->setItem(row,2,new QTableWidgetItem(control->isEnabled()?"Installed":"Not installed"));
            controls.append(control);categories.append(i);
        }
        layout->addWidget(table,1);
        auto* locate=new QPushButton("Locate selected setting");locate->setObjectName("locateIndexedSetting");locate->setEnabled(false);
        layout->addWidget(locate);layout->addWidget(text("Locating a setting never starts it. Unavailable utilities require separate installation.","muted"));
        const auto selected=[table,locate]{locate->setEnabled(table->currentRow()>=0&&!table->selectedItems().isEmpty()&&!table->isRowHidden(table->currentRow()));};
        QObject::connect(table,&QTableWidget::itemSelectionChanged,content,selected);
        const auto update=[table,status,selected,controls](const QString& query){
            int visible=0;const auto tokens=query.simplified().split(' ',Qt::SkipEmptyParts);
            table->clearSelection();table->setCurrentItem(nullptr);
            for(int r=0;r<table->rowCount();++r){
                const auto name=controls[r]->property("systemToolCommand").toString();
                const bool installed=!tux::trustedLauncherExecutable(name).isEmpty();
                table->item(r,2)->setText(installed?"Installed":"Unavailable");table->item(r,2)->setToolTip(installed?"Trusted executable available":dependencyHint(name));
                const QString hay=table->item(r,0)->text()+" "+table->item(r,1)->text()+" "+table->item(r,2)->text()+" "+dependencyPackage(name);
                bool match=true;for(const auto& token:tokens)if(!hay.contains(token,Qt::CaseInsensitive)){match=false;break;}
                table->setRowHidden(r,!match);if(match)++visible;
            }
            status->setText(QString("%1 of %2 native settings · Select an entry to locate it.").arg(visible).arg(table->rowCount()));selected();
        };
        QObject::connect(filter,&QLineEdit::textChanged,content,update);update(QString());
        refreshSettingsIndex=[filter,update]{update(filter->text());};
        QObject::connect(tabs,&QTabWidget::currentChanged,content,[this,tabs,content](int index){if(tabs->widget(index)==content&&refreshSettingsIndex)refreshSettingsIndex();});
        QObject::connect(locate,&QPushButton::clicked,content,[tabs,table,controls,categories]{
            const int row=table->currentRow();
            if(row<0||row>=controls.size()||table->isRowHidden(row)||table->selectedItems().isEmpty())return;
            tabs->setCurrentIndex(categories[row]);
            auto* control=controls[row];
            if(control->isEnabled())control->setFocus(Qt::OtherFocusReason);
            if(auto* scroll=qobject_cast<QScrollArea*>(tabs->widget(categories[row])))scroll->ensureWidgetVisible(control);
        });
        return content;
    }
    QWidget* controlPanelPage() {
        auto* tabs=new QTabWidget;tabs->setObjectName("controlPanelTabs");
        tabs->addTab(memoryOptimizerPage(),"Performance");
        QObject::connect(tabs,&QTabWidget::currentChanged,tabs,[this](int index){if(index==0&&refreshMemoryKernel)refreshMemoryKernel();});
        const auto add=[this,tabs](const QString& title,const QString& description,const QVector<QPair<QString,QString>>& entries){
            auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);
            scroll->setWidget(installedTools(description,entries));tabs->addTab(scroll,title);
        };
        add("Desktop","Open the installed XFCE configuration utilities.",{{"Appearance","xfce4-appearance-settings"},{"Window manager","xfwm4-settings"},{"Window manager tweaks","xfwm4-tweaks-settings"},{"Desktop background","xfdesktop-settings"},{"Display resolution","xfce4-display-settings"},{"Keyboard","xfce4-keyboard-settings"},{"Mouse and touchpad","xfce4-mouse-settings"},{"Power manager","xfce4-power-manager-settings"},{"Session and startup","xfce4-session-settings"},{"Preferred applications","exo-preferred-applications"},{"Accessibility","xfce4-accessibility-settings"},{"Notifications","xfce4-notifyd-config"},{"File manager","thunar-settings"}});
        add("Hardware","Manage installed hardware and drivers in their native tools.",{{"Driver manager","driver-manager"},{"Printers","system-config-printer"},{"Sound","pavucontrol"},{"Bluetooth","blueman-manager"},{"Hardware information","hardinfo"},{"Color management","gcm-viewer"}});
        add("Accounts","Manage identity and login settings. Native tools control their own authorization.",{{"Users and groups","users-admin"},{"Mint user manager","mintusers"},{"Login window","lightdm-settings"},{"Password and keys","seahorse"}});
        add("Storage","Review disks and backups before changing partitions or recovering files.",{{"Disk management","gnome-disks"},{"Partition editor","gparted"},{"Disk space analyzer","baobab"},{"Cache cleanup","bleachbit"},{"System snapshots","timeshift-gtk"},{"Personal backups","deja-dup"},{"File recovery","qphotorec"}});
        add("Network and security","Changes may affect connectivity or protection. Review native prompts.",{{"Network connections","nm-connection-editor"},{"Firewall","gufw"},{"System logs","gnome-logs"}});
        add("Software","Use distribution-supported software tools.",{{"System updates","mintupdate"},{"Software manager","mintinstall"},{"Package manager","synaptic"},{"Software sources","mintsources"}});
        auto* system=new QWidget;auto* sl=new QVBoxLayout(system);
        sl->addWidget(text("Built-in SysPulse administration","sectionTitle"));
        const QVector<QPair<QString,QString>> links={{"System information and diagnostics","overview"},{"Processes and resource history","performance"},{"Startup applications and services","management"},{"Storage diagnostics and cleanup","storage"},{"Network monitoring","network"},{"Security and log diagnostics","security"},{"Scheduled tasks","schedules"},{"Recovery and undo","recovery"},{"Reviewed system changes","experimental"},{"Application updates and preferences","settings"},{"Reports","reports"}};
        for(const auto& link:links)sl->addWidget(button(link.first,[this,link]{navigate(link.second);},w));
        sl->addStretch();auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(system);tabs->addTab(scroll,"System tools");
        tabs->addTab(settingsIndex(tabs),"All settings");
        tabs->addTab(systemCarePage(),"System care");
        tabs->addTab(maintenanceCenterPage(),"Maintenance");
        return workspace("Control Panel","Centralized desktop, hardware, accounts, storage, software and system administration. Uninstalled tools are disabled.",tabs);
    }
    QWidget* applicationUpdates() {
        auto* frame=panel();auto* layout=new QVBoxLayout(frame);
        layout->setContentsMargins(22,20,22,20);
        layout->addWidget(text("Application updates","sectionTitle"));
        layout->addWidget(text(QString("Running version: %1 · Official repository: SysPulse").arg(tux::Version),"muted"));
        auto* status=text("Update authentication is initialized from the trusted installed package. No user account or manual key setup is needed for provisioned releases.","muted");
        status->setObjectName("applicationUpdateStatus");status->setTextFormat(Qt::PlainText);layout->addWidget(status);
        auto* channel=new QComboBox;channel->setObjectName("applicationUpdateChannel");
        channel->addItem("Stable","stable");channel->addItem("Beta (preview)","beta");channel->addItem("Alpha (preview)","alpha");
        layout->addWidget(channel);
        auto* row=new QHBoxLayout;
        auto* check=new QPushButton("Check for SysPulse updates");
        check->setObjectName("checkApplicationUpdate");check->setAccessibleName("Check authenticated SysPulse repository");
        row->addWidget(check);
        auto* install=new QPushButton("Install verified update…");install->setObjectName("installApplicationUpdate");install->setEnabled(false);row->addWidget(install);
        auto* managerButton=button("Open Update Manager…",[this]{
            const auto executable=tux::trustedSystemExecutable("mintupdate");
            if(executable.isEmpty()){QMessageBox::information(w,"Update Manager","Linux Mint Update Manager is unavailable.");return;}
            if(!QProcess::startDetached(executable,QStringList{}))QMessageBox::warning(w,"Update Manager","Unable to open Update Manager.");
        },w);
        managerButton->setObjectName("openSystemUpdateManager");managerButton->setEnabled(!tux::trustedSystemExecutable("mintupdate").isEmpty());row->addWidget(managerButton);
        auto* releaseButton=button("Official releases…",[]{QDesktopServices::openUrl(QUrl("https://github.com/sudipdasin/SysPulse/releases"));},w);
        releaseButton->setObjectName("openOfficialReleases");row->addWidget(releaseButton);layout->addLayout(row);
        auto* automatic=new QCheckBox("Automatically check signed updates every 30 minutes while SysPulse is open (network access)");
        automatic->setObjectName("automaticCachedUpdateCheck");layout->addWidget(automatic);
        auto* timer=new QTimer(frame);timer->setObjectName("cachedUpdateTimer");timer->setInterval(30*60*1000);
        QObject::connect(timer,&QTimer::timeout,check,[check]{if(check->isEnabled())check->click();});
        auto* preferenceStatus=text("","muted");preferenceStatus->setObjectName("updatePreferenceStatus");preferenceStatus->setTextFormat(Qt::PlainText);layout->addWidget(preferenceStatus);
        QSettings saved(QSettings::IniFormat,QSettings::UserScope,"SysPulse","SysPulse");
        const auto selected=saved.value("updates/channel","alpha").toString();channel->setCurrentIndex(qMax(0,channel->findData(selected)));
        const bool initial=saved.status()==QSettings::NoError&&saved.value("updates/automaticSignedCheck",false).toBool();
        automatic->setChecked(initial);if(initial)timer->start();
        if(saved.status()!=QSettings::NoError)preferenceStatus->setText("Update preference could not be read; automatic checks remain off.");
        QObject::connect(automatic,&QCheckBox::toggled,frame,[timer,preferenceStatus](bool enabled){
            if(enabled)timer->start();else timer->stop();
            QSettings settings(QSettings::IniFormat,QSettings::UserScope,"SysPulse","SysPulse");
            settings.setValue("updates/automaticSignedCheck",enabled);settings.setValue("updates/automaticCachedCheck",enabled);settings.sync();
            preferenceStatus->setText(settings.status()==QSettings::NoError?"Update preference saved for the next launch.":"Preference could not be saved; this change applies only to the current session.");
        });
        auto reviewed=std::make_shared<QString>();
        QObject::connect(channel,QOverload<int>::of(&QComboBox::currentIndexChanged),frame,[=](int){
            reviewed->clear();install->setEnabled(false);refreshActionIndicators();status->setText("Channel changed. Check again before installing.");
            QSettings settings(QSettings::IniFormat,QSettings::UserScope,"SysPulse","SysPulse");settings.setValue("updates/channel",channel->currentData());settings.sync();
        });
        QObject::connect(check,&QPushButton::clicked,frame,[=]{
            const auto helper=tux::trustedSystemExecutable("syspulse-apt-update");
            reviewed->clear();install->setEnabled(false);refreshActionIndicators();
            if(helper.isEmpty()){status->setText("Installed update helper unavailable. Reinstall the complete SysPulse DEB.");return;}
            check->setEnabled(false);channel->setEnabled(false);status->setText("Refreshing the selected signed repository…");
            AsyncCommand::run(frame,helper,{"check",channel->currentData().toString()},120000,4096,[=](const AsyncCommandResult& result){
                check->setEnabled(true);channel->setEnabled(true);
                const auto line=QString::fromUtf8(result.output).trimmed();
                if(!result.ok()){status->setText(line.startsWith("ERROR ")?line.mid(6):"Signed repository check failed. No update authorized.");return;}
                if(line.startsWith("UNCONFIGURED ")){status->setText("This release has no trusted signing key for the selected channel. The maintainer must provision the signed feed and a trusted package. Installed: "+line.mid(13));return;}
                if(line.startsWith("CURRENT ")){status->setText("No newer signed package in selected channel. Installed: "+line.mid(8));return;}
                static const QRegularExpression valid("^AVAILABLE ([0-9]+\\.[0-9]+\\.[0-9]+(?:~(?:alpha|beta)[0-9]+)?(?:~render[0-9]+)?)$");
                const auto match=valid.match(line);
                if(!match.hasMatch()){status->setText("Unexpected update metadata. Nothing authorized.");return;}
                *reviewed=match.captured(1);status->setText("Signed update available: "+*reviewed+" · Review official release notes before installation.");
                install->setEnabled(!tux::trustedSystemExecutable("pkexec").isEmpty());refreshActionIndicators();
            });
        });
        QObject::connect(install,&QPushButton::clicked,frame,[=]{
            if(reviewed->isEmpty())return;
            const auto helper=tux::trustedSystemExecutable("syspulse-apt-update"),auth=tux::trustedSystemExecutable("pkexec");
            if(helper.isEmpty()||auth.isEmpty()){status->setText("PolicyKit or the update helper is unavailable.");return;}
            const auto expected=*reviewed;const auto selectedChannel=channel->currentData().toString();
            if(QMessageBox::question(frame,"Review application upgrade","Install complete signed SysPulse package "+expected+" from "+selectedChannel+"? System package manager authorization is required. Existing settings are preserved. Close other SysPulse tools first. Do not interrupt installation. Recovery is manual if needed.",QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
            reviewed->clear();install->setEnabled(false);check->setEnabled(false);channel->setEnabled(false);refreshActionIndicators();
            class UpdateProgress final:public QDialog {public:using QDialog::QDialog;void reject() override {}protected:void closeEvent(QCloseEvent* e) override {e->ignore();}};
            UpdateProgress progress(frame);progress.setWindowTitle("Installing signed SysPulse update");progress.setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint|Qt::WindowTitleHint);
            auto* pl=new QVBoxLayout(&progress);pl->addWidget(text("Waiting for administrator authorization and APT completion. Do not close SysPulse or shut down during installation."));
            tux::admin::runReviewed(&progress,{"update",selectedChannel,expected},auth,{helper,"install",selectedChannel,expected},4096,[&](const AsyncCommandResult& result){
                const auto line=QString::fromUtf8(result.output).trimmed();
                status->setText(result.ok()&&line=="INSTALLED "+expected?"Installed and verified "+expected+". Close and relaunch SysPulse.":line.startsWith("ERROR ")?line.mid(6):"Update not confirmed. Review package manager state before retrying.");
                progress.done(0);
            });
            progress.exec();check->setEnabled(true);channel->setEnabled(true);
        });
        // Only an explicit saved network opt-in triggers a startup check. No blocking work during construction.
        QTimer::singleShot(1500,frame,[automatic,check]{if(automatic->isChecked()&&check->isEnabled())check->click();});
        layout->addWidget(text("The selected channel uses the package-managed public release key when provisioned. APT verifies server metadata signatures and package hashes. No client identity or private signing key is created. Updates require administrator approval; no unattended installation or downgrade.","muted"));
        return frame;
    }
    QWidget* settingsPage() {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);layout->setContentsMargins(0,0,0,0);
        auto* about=panel();auto* al=new QVBoxLayout(about);al->setContentsMargins(22,20,22,20);al->addWidget(text(QString("SysPulse %1").arg(tux::Version),"sectionTitle"));
        al->addWidget(text("Developed by Sudip Das · GNU GPL version 3 or later\nXFCE-first Linux system tools, built on the Tux Steward foundation.","muted"));
        al->addWidget(text("Privacy: no automatic telemetry. Optional diagnostic sharing is off by default; each report requires preview and fresh consent.","muted"));al->addWidget(button("Review diagnostic sharing…",[this]{diagnostic::reviewSharing(w,results);},w));layout->addWidget(about);
        auto* authorization=panel();auto* authLayout=new QVBoxLayout(authorization);authLayout->setContentsMargins(22,20,22,20);
        authLayout->addWidget(text("Administrator session","sectionTitle"));
        auto* remember=new QCheckBox("Remember administrator authorization for this SysPulse session");remember->setObjectName("rememberAdminSession");authLayout->addWidget(remember);
        auto* authStatus=text("Locked · authenticate through the desktop when needed.","muted");authStatus->setObjectName("adminSessionStatus");authStatus->setTextFormat(Qt::PlainText);authLayout->addWidget(authStatus);
        authLayout->addWidget(text("Built-in maintenance, signed updates, system service management and protected volume inspection reuse the session. External system tools retain their own authorization. Every action still requires review. No password is stored.","muted"));
        auto* unlock=new QPushButton("Unlock administrator session…");unlock->setObjectName("unlockAdminSession");unlock->setEnabled(false);authLayout->addWidget(unlock);
        auto* relock=new QPushButton("Lock now");relock->setObjectName("lockAdminSession");relock->setEnabled(false);authLayout->addWidget(relock);
        auto* sessionTimer=new QTimer(authorization);sessionTimer->setInterval(1000);
        QObject::connect(remember,&QCheckBox::toggled,authorization,[=](bool checked){
            if(!checked){if(!tux::admin::lock()){remember->blockSignals(true);remember->setChecked(true);remember->blockSignals(false);authStatus->setText("Wait for the active administrator action to finish before locking.");return;}sessionTimer->stop();relock->setEnabled(false);authStatus->setText("Locked · session authorization cleared.");}
            unlock->setEnabled(checked&&!tux::admin::active());refreshActionIndicators();
        });
        QObject::connect(relock,&QPushButton::clicked,authorization,[=]{if(!tux::admin::lock()){authStatus->setText("Wait for the active administrator action to finish.");return;}remember->setChecked(false);relock->setEnabled(false);authStatus->setText("Locked · session authorization cleared.");refreshActionIndicators();});
        QObject::connect(unlock,&QPushButton::clicked,authorization,[=]{
            if(!remember->isChecked()||tux::admin::busy())return;
            remember->setEnabled(false);unlock->setEnabled(false);relock->setEnabled(false);authStatus->setText("Waiting for desktop administrator authentication…");if(auto* b=w->findChild<QPushButton*>("globalAdminSession"))b->setText("Admin: authenticating");
            auto message=std::make_shared<QString>();auto success=std::make_shared<bool>(false);
            auto* worker=QThread::create([message,success]{*success=tux::admin::unlock(*message);});
            worker->setObjectName("adminSessionUnlockWorker");worker->setParent(authorization);
            QObject::connect(worker,&QThread::finished,authorization,[=]{remember->setEnabled(true);unlock->setEnabled(!*success&&remember->isChecked());relock->setEnabled(*success);if(*success)sessionTimer->start();else sessionTimer->stop();authStatus->setText(*success?"Unlocked · supported internal administrator operations can reuse authorization until locked or closed.":*message);refreshActionIndicators();});
            QObject::connect(worker,&QThread::finished,worker,&QObject::deleteLater);worker->start();
        });
        QObject::connect(sessionTimer,&QTimer::timeout,authorization,[=]{if(remember->isEnabled()&&relock->isEnabled()&&!tux::admin::active()&&!tux::admin::busy()){sessionTimer->stop();relock->setEnabled(false);unlock->setEnabled(remember->isChecked());authStatus->setText("Session ended. Unlock again before retrying a reviewed action.");}refreshActionIndicators();});
        layout->addWidget(authorization);
        layout->addWidget(applicationUpdates());
        auto* preferences=panel();auto* pl=new QVBoxLayout(preferences);pl->setContentsMargins(22,20,22,20);pl->addWidget(text("Preferences","sectionTitle"));pl->addWidget(text("Precision is the selected visual identity. Sampling speed and pause controls are available in Performance. System actions use your normal desktop authorization.","muted"));pl->addWidget(button("Open monitoring controls",[this]{navigate("performance");},w));
        auto* artwork=new QCheckBox("Show translucent background artwork");artwork->setObjectName("appearanceArtwork");artwork->setAccessibleDescription("Disable decorative wallpaper for a simpler appearance.");
        QSettings appearance(QSettings::IniFormat,QSettings::UserScope,"SysPulse","SysPulse");
        const bool showArtwork=appearance.status()==QSettings::NoError&&appearance.value("appearance/artwork",true).toBool();
        artwork->setChecked(showArtwork);backdrop->setArtwork(showArtwork);pl->addWidget(artwork);
        auto* appearanceStatus=text("Appearance is a local preference; no system settings are changed.","muted");appearanceStatus->setObjectName("appearanceStatus");pl->addWidget(appearanceStatus);
        QObject::connect(artwork,&QCheckBox::toggled,w,[this,appearanceStatus](bool enabled){
            backdrop->setArtwork(enabled);QSettings preferences(QSettings::IniFormat,QSettings::UserScope,"SysPulse","SysPulse");
            preferences.setValue("appearance/artwork",enabled);preferences.sync();
            appearanceStatus->setText(preferences.status()==QSettings::NoError?"Appearance preference saved.":"Appearance changed for this session; preference could not be saved.");
        });layout->addWidget(preferences);
        auto* help=panel();auto* hl=new QHBoxLayout(help);hl->setContentsMargins(22,20,22,20);
        hl->addWidget(button("Report a problem…",[]{QDesktopServices::openUrl(QUrl("https://github.com/sudipdasin/SysPulse/issues/new/choose"));},w));
        hl->addWidget(button("Contribute…",[]{QDesktopServices::openUrl(QUrl("https://github.com/sudipdasin/SysPulse"));},w));
        hl->addWidget(button("License",[this]{QMessageBox::information(w,"Free and open source","SysPulse is licensed under GNU GPL version 3 or later.\nCopyright Sudip Das.\nSee the included LICENSE for the complete terms.");},w));layout->addWidget(help);layout->addStretch();
        auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(content);
        return workspace("Settings & help","Local preferences, project information and developer feedback.",scroll);
    }
    void openSettingsSection(const QString& objectName) {
        if(!navigate("settings"))return;
        auto* page=pages.value("settings");auto* scroll=page->findChild<QScrollArea*>();auto* control=page->findChild<QWidget*>(objectName);
        if(scroll&&control)scroll->ensureWidgetVisible(control,20,30);
    }
    void stepHistory(int direction) {
        const int next=historyIndex+direction;if(next<0||next>=history.size())return;
        historyIndex=next;historyReplay=true;navigate(history.at(next));historyReplay=false;
    }
    bool navigate(const QString& id) {
        if(!pages.contains(id))return false;
        if(!historyReplay&&(historyIndex<0||history.value(historyIndex)!=id)) {
            while(history.size()>historyIndex+1)history.removeLast();
            history.append(id);if(history.size()>64)history.removeFirst();historyIndex=history.size()-1;
        }
        backButton->setEnabled(historyIndex>0);forwardButton->setEnabled(historyIndex+1<history.size());
        current=id;stack->setCurrentWidget(pages[id]);nav[id]->setChecked(true);
        if(auto* tools=toolboxes.value(id,nullptr))tools->refreshAvailability();
        for(auto* control:pages[id]->findChildren<QPushButton*>()){
            auto command=control->property("systemToolCommand");if(!command.isValid())command=control->property("desktopToolCommand");
            if(!command.isValid())continue;
            const bool available=!tux::trustedLauncherExecutable(command.toString()).isEmpty();control->setEnabled(available);
            auto* tile=control->parentWidget();if(!tile||tile->objectName()!="systemToolTile")continue;
            const auto summary=control->accessibleDescription();
            const auto tip=summary+(available?"\nInstalled · opens native utility.":"\nUnavailable · "+dependencyHint(command.toString()));
            if(auto* dependency=tile->findChild<QPushButton*>("toolDependency"))dependency->setVisible(!available&&!dependencyPackage(command.toString()).isEmpty());
            control->setToolTip(tip);tile->setToolTip(tip);
            if(auto* state=tile->findChild<QLabel*>("toolAvailability")){state->setText(available?"Installed · native utility":"Not installed or not trusted");state->setProperty("available",available);state->setToolTip(tip);state->style()->unpolish(state);state->style()->polish(state);}
        }
        const auto group=(id=="overview"||id=="performance"||id=="storage")?"Workspace":(id=="control"||id=="management"||id=="network"||id=="desktop"||id=="security")?"System":"Tools";
        breadcrumb->setText(QString(group)+" / "+nav[id]->property("displayTitle").toString());
        performance->setSamplingEnabled(sampling&&(id=="overview"||id=="performance"));
        if(id=="control"&&refreshMemoryKernel)refreshMemoryKernel();
        if(id=="control"&&refreshSettingsIndex)refreshSettingsIndex();
        refreshActionIndicators();
        return true;
    }
    void findTool(const QString& query) {
        const auto trimmed=query.trimmed();if(trimmed.isEmpty())return;
        if(trimmed.compare("Experimental",Qt::CaseInsensitive)==0){navigate("experimental");return;}
        if(trimmed.compare("Application updates",Qt::CaseInsensitive)==0){openSettingsSection("applicationUpdateStatus");return;}
        if(trimmed.compare("Administrator session",Qt::CaseInsensitive)==0){openSettingsSection("adminSessionStatus");return;}
        for(auto i=nav.cbegin();i!=nav.cend();++i)if(i.value()->property("displayTitle").toString().compare(trimmed,Qt::CaseInsensitive)==0){navigate(i.key());return;}
        // Qualified titles disambiguate repeated labels. Search never launches a tool.
        for(auto i=pages.cbegin();i!=pages.cend();++i) {
            const auto label=nav[i.key()]->property("displayTitle").toString();
            for(auto* control:i.value()->findChildren<QPushButton*>()) {
                if(!control->property("systemToolCommand").isValid()&&!control->property("desktopToolCommand").isValid())continue;
                if((label+" / "+fluent::plainTitle(control->text())).compare(trimmed,Qt::CaseInsensitive)!=0)continue;
                navigate(i.key());
                for(auto* ancestor=control->parentWidget();ancestor;ancestor=ancestor->parentWidget()) {
                    if(auto* tabs=qobject_cast<QTabWidget*>(ancestor)) {
                        for(int index=0;index<tabs->count();++index) {
                            auto* tab=tabs->widget(index);
                            if(tab==control || tab->isAncestorOf(control)){tabs->setCurrentIndex(index);break;}
                        }
                    }
                    if(auto* scroll=qobject_cast<QScrollArea*>(ancestor))scroll->ensureWidgetVisible(control);
                    if(ancestor==i.value())break;
                }
                if(control->isEnabled())control->setFocus(Qt::OtherFocusReason);
                search->setToolTip(control->isEnabled()?"Tool located. Select it to review the launch.":"Tool located, but unavailable on this system.");
                return;
            }
        }
        for(const auto& a:tux::actions())if(a.title.compare(trimmed,Qt::CaseInsensitive)==0||a.id.compare(trimmed,Qt::CaseInsensitive)==0) {
            locateMaintenanceTool(a.id);return;
        }
        for(const auto& a:tux::actions())if(tux::matches(a,trimmed)) {
            const auto id=destination(a);navigate(id);toolboxes[id]->setSearch(trimmed);
            if(id=="network")pages[id]->findChild<QTabWidget*>("networkWorkspaceTabs")->setCurrentIndex(1);
            if(id=="desktop") {auto* tabs=toolboxes[id]->parentWidget();while(tabs&&!qobject_cast<QTabWidget*>(tabs))tabs=tabs->parentWidget();if(auto* t=qobject_cast<QTabWidget*>(tabs))t->setCurrentIndex(1);}
            if(id=="storage"||id=="security"){auto* tabs=pages[id]->findChild<QTabWidget*>(id=="storage"?"storageWorkspaceTabs":"securityWorkspaceTabs");if(tabs)tabs->setCurrentIndex(0);}
            return;
        }
        search->setToolTip("No matching tool. Try a shorter name or choose a suggestion.");
    }
};
Window::Window(bool samplingEnabled,QWidget* parent):QMainWindow(parent),d(std::make_unique<Impl>(this,samplingEnabled)) {}
Window::~Window()=default;
bool Window::navigate(const QString& id) {return d->navigate(id);}
QString Window::currentPageId()const{return d->current;}
int Window::navigationCount()const{return d->nav.size();}
void Window::closeEvent(QCloseEvent* event) {
    if(tux::admin::busy()||!findChildren<QThread*>("adminSessionUnlockWorker").isEmpty()){QMessageBox::information(this,"Administrator session busy","Finish or cancel the authentication or maintenance operation before closing.");event->ignore();return;}
    if(d->management->isBusy()) {QMessageBox::information(this,"System action in progress","Wait for the service action to finish or cancel the desktop authorization before closing SysPulse.");event->ignore();return;}
    for(auto* page:d->allToolboxes)if(page->isBusy()) {
        if(page->isCancellable()&&QMessageBox::question(this,"Diagnostic in progress","Stop the active diagnostic? Close SysPulse after it finishes.",QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)page->requestStop();
        else if(!page->isCancellable())QMessageBox::information(this,"Task in progress","Wait for the system action to finish before closing SysPulse.");
        event->ignore();return;
    }
    if(!tux::admin::lock()){event->ignore();return;}
    event->accept();
}
}
