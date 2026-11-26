// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/window.hpp"
#include "syspulse/health_probe.hpp"
#include "syspulse/performance.hpp"
#include "syspulse/management.hpp"
#include "syspulse/toolbox.hpp"
#include "tux/core.hpp"
#include "tux/trusted_command.hpp"
#include "tux/async_command.hpp"
#include <QRegularExpression>
#include "tux/health_dialog.hpp"
#include "tux/report_dialog.hpp"
#include "tux/process_manager.hpp"
#include <QApplication>
#include <QCheckBox>
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

namespace syspulse {
namespace {
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
    b->setProperty("primary",primary);
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
    PerformancePage* performance=nullptr;
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
    bool sampling=true;
    explicit Impl(Window* window,bool sample):w(window),sampling(sample) {build();}
    void remember(const tux::Result& result) {
        results.append(result);if(results.size()>20)results.removeFirst();
        reports->setRowCount(results.size());
        for(int row=0;row<results.size();++row) {
            const auto& r=results[results.size()-row-1];
            const QString outcome=r.cancelled?"Cancelled":r.code==0?"Completed":r.code==124?"Timed out":QString("Needs attention (%1)").arg(r.code);
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
    void updateFooter() {
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
            int ok=0;for(const auto& r:completed)if(r.code==0&&!r.cancelled)++ok;
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
        auto* root=new QWidget(w);auto* outer=new QVBoxLayout(root);outer->setContentsMargins(0,0,0,0);outer->setSpacing(0);
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
            const QMap<QString,QStyle::StandardPixmap> icons={{"overview",QStyle::SP_ComputerIcon},{"performance",QStyle::SP_MediaPlay},{"storage",QStyle::SP_DriveHDIcon},{"management",QStyle::SP_FileDialogDetailedView},{"network",QStyle::SP_DriveNetIcon},{"desktop",QStyle::SP_DesktopIcon},{"security",QStyle::SP_MessageBoxInformation},{"schedules",QStyle::SP_BrowserReload},{"recovery",QStyle::SP_ArrowBack},{"reports",QStyle::SP_FileIcon},{"experimental",QStyle::SP_MessageBoxWarning},{"settings",QStyle::SP_DialogHelpButton}};
            b->setIcon(w->style()->standardIcon(icons.value(id,QStyle::SP_FileIcon)));b->setIconSize(QSize(16,16));
            b->setAccessibleName(title);b->setProperty("pageId",id);nav.insert(id,b);sl->addWidget(b);
            QObject::connect(b,&QPushButton::clicked,w,[this,id]{navigate(id);});
        };
        sl->addWidget(text("WORKSPACE","navGroup"));
        addNav("overview","Overview");addNav("performance","Performance");addNav("storage","Storage & cleanup");
        sl->addWidget(text("SYSTEM","navGroup"));
        addNav("management","Startup & services");addNav("network","Network");addNav("desktop","XFCE desktop");addNav("security","Security & privacy");
        sl->addWidget(text("TOOLS","navGroup"));
        addNav("schedules","Schedules");addNav("recovery","Recovery");addNav("reports","Reports");addNav("experimental","Experimental");
        sl->addStretch(1);addNav("settings","Settings & help");
        shell->addWidget(sidebar);
        auto* body=new QWidget;auto* bl=new QVBoxLayout(body);bl->setContentsMargins(0,0,0,0);bl->setSpacing(0);
        auto* utility=new QWidget;utility->setObjectName("utilityBar");auto* ul=new QHBoxLayout(utility);ul->setContentsMargins(24,12,24,12);
        breadcrumb=text("Workspace / Overview");breadcrumb->setObjectName("breadcrumb");ul->addWidget(breadcrumb,1);
        search=new QLineEdit;search->setObjectName("globalToolSearch");search->setAccessibleName("Find a SysPulse tool");search->setPlaceholderText("Find a tool · Ctrl+F");search->setClearButtonEnabled(true);search->setMaximumWidth(420);ul->addWidget(search,1);
        ul->addWidget(button("Help",[this]{navigate("settings");},w));bl->addWidget(utility);
        stack=new QStackedWidget;stack->setObjectName("sysPulsePages");bl->addWidget(stack,1);shell->addWidget(body,1);
        footer=text(QString(),"statusFooter");footer->setObjectName("statusFooter");outer->addWidget(footer);
        w->setCentralWidget(root);
        performance=new PerformancePage;
        performance->setSamplingEnabled(sampling);
        auto* processes=new ProcessManagerWidget(nullptr,false);
        performance->detailTabs()->addTab(processes,"Processes");
        management=new ManagementPage;
        pages.insert("overview",overview());
        pages.insert("performance",performance);
        auto* storage=toolbox({"Storage","Maintenance"});toolboxes.insert("storage",storage);
        auto* storageTabs=new QTabWidget;storageTabs->setObjectName("storageWorkspaceTabs");storageTabs->addTab(storage,"Diagnostics & cleanup");
        storageTabs->addTab(installedTools("Use installed disk tools. Partition changes can destroy data; back up before modifying a disk.",{{"Disk management","gnome-disks"},{"Partition editor","gparted"},{"Disk usage analyzer","baobab"},{"File manager","thunar"},{"Cache cleanup","bleachbit"}}),"Disk tools");
        pages.insert("storage",workspace("Storage & cleanup","Inspect storage and review maintenance commands. Native changes remain in Experimental.",storageTabs));
        pages.insert("management",management);
        pages.insert("network",networkPage());
        pages.insert("desktop",desktopPage());
        auto* security=toolbox({"Diagnostics","Security"});toolboxes.insert("security",security);
        auto* securityTabs=new QTabWidget;securityTabs->setObjectName("securityWorkspaceTabs");securityTabs->addTab(security,"Diagnostics");
        securityTabs->addTab(installedTools("Open distribution-supported software and security tools; they manage their own authorization.",{{"Update manager","mintupdate"},{"Software manager","mintinstall"},{"Package manager","synaptic"},{"Software sources","mintsources"},{"Firewall","gufw"},{"System logs","gnome-logs"}}),"Software & security tools");
        pages.insert("security",workspace("Security & privacy","Review system warnings, update state, permissions and available firewall tools. Reports stay local.",securityTabs));
        auto* schedules=toolbox({"Schedules"});toolboxes.insert("schedules",schedules);
        pages.insert("schedules",workspace("Schedules","Inspect system and user timers. SysPulse does not enable automation at startup.",schedules));
        pages.insert("recovery",recoveryPage());
        pages.insert("reports",reportsPage());
        auto* experiments=toolbox({},true);toolboxes.insert("experimental",experiments);
        pages.insert("experimental",workspace("Experimental","Settings-changing maintenance tools. Review scope and limitations before applying a change.",experiments));
        pages.insert("settings",settingsPage());
        management->canMutate=[this]{for(auto* page:allToolboxes)if(page->isBusy())return false;return true;};
        management->onBusyChanged=[this](bool){updateFooter();};
        for(auto i=pages.cbegin();i!=pages.cend();++i){i.value()->setProperty("sysPulsePageId",i.key());stack->addWidget(i.value());}
        performance->onMetricsChanged=[this]{updateMetrics();};
        performance->onOpenStartup=[this]{navigate("management");management->showStartup();};
        performance->onOpenServices=[this]{navigate("management");management->showServices();};
        QStringList suggestions;for(auto i=nav.cbegin();i!=nav.cend();++i)suggestions.append(i.value()->property("displayTitle").toString());
        for(const auto& a:tux::actions())suggestions.append(a.title);
        for(auto i=pages.cbegin();i!=pages.cend();++i) {
            const auto label=nav[i.key()]->property("displayTitle").toString();
            for(auto* control:i.value()->findChildren<QPushButton*>())
                if(control->property("systemToolCommand").isValid())
                    suggestions.append(label+" / "+control->text());
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
    QWidget* installedTools(const QString& description,const QVector<QPair<QString,QString>>& entries) {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);
        layout->addWidget(text(description,"muted"));
        auto* grid=new QGridLayout;grid->setSpacing(12);int index=0;
        for(const auto& entry:entries) {
            const auto program=tux::trustedSystemExecutable(entry.second);
            auto* b=button(entry.first,[this,entry]{
                const auto executable=tux::trustedSystemExecutable(entry.second);
                if(executable.isEmpty()){QMessageBox::warning(w,entry.first,"The trusted system tool is no longer available.");return;}
                if(QMessageBox::question(w,entry.first,"Open the installed system utility? It may change your computer. Review its own authorization and operation prompts. Back up data before disk or recovery changes.",QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
                if(!QProcess::startDetached(executable,QStringList{}))QMessageBox::warning(w,entry.first,"The system utility could not be opened.");
            },w);
            b->setProperty("systemToolCommand",entry.second);
            b->setEnabled(!program.isEmpty());
            b->setToolTip(program.isEmpty()?"Unavailable: install the distribution-supported system tool separately.":"Open the installed tool after review; it manages its own authorization.");
            grid->addWidget(b,index/2,index%2);++index;
        }
        layout->addLayout(grid);layout->addStretch();return content;
    }
    QWidget* desktopPage() {
        auto* tabs=new QTabWidget;auto* settings=new QWidget;auto* layout=new QVBoxLayout(settings);
        layout->addWidget(text("Open installed XFCE settings tools. Changes are made through their native interfaces.","muted"));
        auto* grid=new QGridLayout;grid->setSpacing(12);int index=0;
        const QVector<QPair<QString,QString>> apps={{"Display","xfce4-display-settings"},{"Keyboard","xfce4-keyboard-settings"},{"Mouse & touchpad","xfce4-mouse-settings"},{"Power manager","xfce4-power-manager-settings"},{"Panel","xfce4-panel"},{"Session","xfce4-session-settings"},{"Appearance","xfce4-appearance-settings"},{"Window manager","xfwm4-settings"}};
        for(const auto& item:apps) {
            const auto program=QStandardPaths::findExecutable(item.second);
            auto* b=button(item.first,[this,program,item]{
                QStringList args;if(item.second=="xfce4-panel")args<<"--preferences";
                if(!QProcess::startDetached(program,args))QMessageBox::warning(w,item.first,"The desktop settings tool could not be opened.");
            },w);
            b->setEnabled(!program.isEmpty());b->setToolTip(program.isEmpty()?"This XFCE settings tool is not installed.":"Open the native XFCE settings window.");grid->addWidget(b,index/2,index%2);++index;
        }
        layout->addLayout(grid);layout->addStretch();tabs->addTab(settings,"Desktop settings");
        auto* diagnostic=toolbox({"Performance","Input","Gaming","Desktop"});toolboxes.insert("desktop",diagnostic);tabs->addTab(diagnostic,"Diagnostics & integrations");
        tabs->addTab(installedTools("Administration tools are optional and run separately using their own authorization.",{{"Settings manager","xfce4-settings-manager"},{"Desktop preferences","xfdesktop-settings"},{"Driver manager","driver-manager"},{"Users and groups","users-admin"},{"Mint users","mintusers"},{"Login window","lightdm-settings"},{"Printers","system-config-printer"},{"Hardware information","hardinfo"},{"Password and keys","seahorse"}}),"Administration tools");
        return workspace("XFCE desktop","Desktop, input, power and installed Linux integrations.",tabs);
    }
    QWidget* recoveryPage() {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);auto* frame=panel();auto* fl=new QVBoxLayout(frame);fl->setContentsMargins(20,20,20,20);
        fl->addWidget(text("Startup overrides","sectionTitle"));fl->addWidget(text("Review the selected application in Startup apps to restore its system default or inspect an available undo record. Recovery refuses externally changed targets.","muted"));fl->addWidget(button("Review startup recovery",[this]{navigate("management");management->showStartup();},w,true));layout->addWidget(frame);
        auto* limits=panel();auto* ll=new QVBoxLayout(limits);ll->setContentsMargins(20,20,20,20);ll->addWidget(text("Recovery availability","sectionTitle"));ll->addWidget(text("Cleanup and process termination cannot be undone here. Snapshot creation and general configuration rollback are planned; no backup is created automatically.","muted"));layout->addWidget(limits);
        layout->addWidget(installedTools("Recover files to a different disk. Timeshift system snapshots are not personal-file backups.",{{"Timeshift","timeshift-gtk"},{"File recovery","qphotorec"},{"Personal file backups","deja-dup"}}),1);layout->addStretch();
        return workspace("Recovery","Use recovery only where a supported saved state exists.",content);
    }
    QWidget* reportsPage() {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);layout->setContentsMargins(0,0,0,0);
        auto* row=new QHBoxLayout;reportStatus=text("No completed tasks in this session.","muted");reportStatus->setObjectName("reportsStatus");row->addWidget(reportStatus,1);
        auto* exportButton=button("Review export…",[this]{exportReports();},w,true);exportButton->setObjectName("reviewReports");row->addWidget(exportButton);layout->addLayout(row);
        reports=new QTableWidget(0,4);reports->setObjectName("sessionReports");reports->setHorizontalHeaderLabels({"Task","Outcome","Duration","Record"});reports->verticalHeader()->hide();reports->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);reports->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);reports->setColumnWidth(1,170);reports->setEditTriggers(QAbstractItemView::NoEditTriggers);reports->setSelectionBehavior(QAbstractItemView::SelectRows);layout->addWidget(reports,1);
        layout->addWidget(text("The latest 20 task results stay in memory for this session. Summary is the default export; diagnostic details require a fresh opt-in. No report is sent automatically.","muted"));
        return workspace("Reports","Review analysis and tool outcomes before saving or sharing.",content);
    }
    QWidget* applicationUpdates() {
        auto* frame=panel();auto* layout=new QVBoxLayout(frame);
        layout->setContentsMargins(22,20,22,20);
        layout->addWidget(text("Application updates","sectionTitle"));
        layout->addWidget(text(QString("Running version: %1 · Official repository: SysPulse").arg(tux::Version),"muted"));
        auto* status=text("No update check performed. A signed SysPulse update feed has not been provisioned by this build.","muted");
        status->setObjectName("applicationUpdateStatus");status->setTextFormat(Qt::PlainText);layout->addWidget(status);
        auto* row=new QHBoxLayout;
        auto* check=new QPushButton("Check for SysPulse updates");
        check->setObjectName("checkApplicationUpdate");check->setAccessibleName("Check cached APT version of SysPulse");
        row->addWidget(check);
        auto* managerButton=button("Open Update Manager…",[this]{
            const auto executable=tux::trustedSystemExecutable("mintupdate");
            if(executable.isEmpty()){QMessageBox::information(w,"Update Manager","Linux Mint Update Manager is unavailable.");return;}
            if(!QProcess::startDetached(executable,QStringList{}))QMessageBox::warning(w,"Update Manager","Unable to open Update Manager.");
        },w);
        managerButton->setObjectName("openSystemUpdateManager");managerButton->setEnabled(!tux::trustedSystemExecutable("mintupdate").isEmpty());row->addWidget(managerButton);
        auto* releaseButton=button("Official releases…",[]{QDesktopServices::openUrl(QUrl("https://github.com/sudipdasin/SysPulse/releases"));},w);
        releaseButton->setObjectName("openOfficialReleases");row->addWidget(releaseButton);layout->addLayout(row);
        QObject::connect(check,&QPushButton::clicked,frame,[frame,check,status]{
            const auto apt=tux::trustedSystemExecutable("apt-cache");
            const auto env=tux::trustedSystemExecutable("env");
            if(apt.isEmpty()||env.isEmpty()){status->setText("APT version query is unavailable.");return;}
            check->setEnabled(false);status->setText("Reading existing APT metadata locally…");
            AsyncCommand::run(frame,env,{"LC_ALL=C",apt,"policy","syspulse"},5000,16384,[check,status](const AsyncCommandResult& result){
                check->setEnabled(true);
                if(!result.ok()){status->setText("APT query failed, timed out or returned incomplete output. No update status can be established.");return;}
                const auto output=QString::fromUtf8(result.output);
                const QRegularExpression installedRx("(?m)^  Installed: ([A-Za-z0-9.+:~()-]+)$");
                const QRegularExpression candidateRx("(?m)^  Candidate: ([A-Za-z0-9.+:~()-]+)$");
                const auto installed=installedRx.match(output),candidate=candidateRx.match(output);
                if(!installed.hasMatch()||!candidate.hasMatch()){
                    status->setText("No SysPulse version found in local APT metadata. Check the official release page; no automatic installation will be attempted.");return;
                }
                status->setText(QString("APT installed: %1 · Cached candidate: %2\nThis is local APT metadata, not proof of a current signed SysPulse release. Refresh and review packages in Update Manager.").arg(installed.captured(1),candidate.captured(1)));
            });
        });
        layout->addWidget(text("Checking here is read-only and does not refresh repositories. Installation, trust enrollment and automatic repository checks require a configured signed release feed.","muted"));
        return frame;
    }
    QWidget* settingsPage() {
        auto* content=new QWidget;auto* layout=new QVBoxLayout(content);layout->setContentsMargins(0,0,0,0);
        auto* about=panel();auto* al=new QVBoxLayout(about);al->setContentsMargins(22,20,22,20);al->addWidget(text(QString("SysPulse %1").arg(tux::Version),"sectionTitle"));
        al->addWidget(text("Developed by Sudip Das · GNU GPL version 3 or later\nXFCE-first Linux system tools, built on the Tux Steward foundation.","muted"));
        al->addWidget(text("Privacy: no telemetry or automatic report uploads. Review detailed reports before sharing.","muted"));layout->addWidget(about);
        layout->addWidget(applicationUpdates());
        auto* preferences=panel();auto* pl=new QVBoxLayout(preferences);pl->setContentsMargins(22,20,22,20);pl->addWidget(text("Preferences","sectionTitle"));pl->addWidget(text("Precision is the selected visual identity. Sampling speed and pause controls are available in Performance. System actions use your normal desktop authorization.","muted"));pl->addWidget(button("Open monitoring controls",[this]{navigate("performance");},w));layout->addWidget(preferences);
        auto* help=panel();auto* hl=new QHBoxLayout(help);hl->setContentsMargins(22,20,22,20);
        hl->addWidget(button("Report a problem…",[]{QDesktopServices::openUrl(QUrl("https://github.com/sudipdasin/SysPulse/issues/new/choose"));},w));
        hl->addWidget(button("Contribute…",[]{QDesktopServices::openUrl(QUrl("https://github.com/sudipdasin/SysPulse"));},w));
        hl->addWidget(button("License",[this]{QMessageBox::information(w,"Free and open source","SysPulse is licensed under GNU GPL version 3 or later.\nCopyright Sudip Das.\nSee the included LICENSE for the complete terms.");},w));layout->addWidget(help);layout->addStretch();
        return workspace("Settings & help","Local preferences, project information and developer feedback.",content);
    }
    bool navigate(const QString& id) {
        if(!pages.contains(id))return false;
        current=id;stack->setCurrentWidget(pages[id]);nav[id]->setChecked(true);
        const auto group=(id=="overview"||id=="performance"||id=="storage")?"Workspace":(id=="management"||id=="network"||id=="desktop"||id=="security")?"System":"Tools";
        breadcrumb->setText(QString(group)+" / "+nav[id]->property("displayTitle").toString());
        performance->setSamplingEnabled(sampling&&(id=="overview"||id=="performance"));return true;
    }
    void findTool(const QString& query) {
        const auto trimmed=query.trimmed();if(trimmed.isEmpty())return;
        for(auto i=nav.cbegin();i!=nav.cend();++i)if(i.value()->property("displayTitle").toString().compare(trimmed,Qt::CaseInsensitive)==0){navigate(i.key());return;}
        // Qualified titles disambiguate repeated labels. Search never launches a tool.
        for(auto i=pages.cbegin();i!=pages.cend();++i) {
            const auto label=nav[i.key()]->property("displayTitle").toString();
            for(auto* control:i.value()->findChildren<QPushButton*>()) {
                if(!control->property("systemToolCommand").isValid())continue;
                if((label+" / "+control->text()).compare(trimmed,Qt::CaseInsensitive)!=0)continue;
                navigate(i.key());
                for(auto* ancestor=control->parentWidget();ancestor;ancestor=ancestor->parentWidget()) {
                    if(auto* tabs=qobject_cast<QTabWidget*>(ancestor)) {
                        for(int index=0;index<tabs->count();++index) {
                            auto* tab=tabs->widget(index);
                            if(tab==control || tab->isAncestorOf(control)){tabs->setCurrentIndex(index);break;}
                        }
                    }
                    if(ancestor==i.value())break;
                }
                if(control->isEnabled())control->setFocus(Qt::OtherFocusReason);
                search->setToolTip(control->isEnabled()?"Tool located. Select it to review the launch.":"Tool located, but unavailable on this system.");
                return;
            }
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
    if(d->management->isBusy()) {QMessageBox::information(this,"System action in progress","Wait for the service action to finish or cancel the desktop authorization before closing SysPulse.");event->ignore();return;}
    for(auto* page:d->allToolboxes)if(page->isBusy()) {
        if(page->isCancellable()&&QMessageBox::question(this,"Diagnostic in progress","Stop the active diagnostic? Close SysPulse after it finishes.",QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)page->requestStop();
        else if(!page->isCancellable())QMessageBox::information(this,"Task in progress","Wait for the system action to finish before closing SysPulse.");
        event->ignore();return;
    }
    event->accept();
}
}
