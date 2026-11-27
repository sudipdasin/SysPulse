// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/cleaner_page.hpp"
#include "syspulse/cleaner.hpp"
#include "syspulse/volume_inspection.hpp"
#include "syspulse/duplicates.hpp"
#include "tux/async_command.hpp"
#include "tux/admin_session_async.hpp"
#include "tux/trusted_command.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QGridLayout>
#include <QScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QProcess>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSet>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <memory>
namespace syspulse {
namespace {
namespace c=cleaner;
QString sizeText(std::uint64_t n){
    const char* units[]={"B","KiB","MiB","GiB","TiB","PiB"};double v=n;int i=0;
    while(v>=1024&&i<5){v/=1024;++i;}
    return QString::number(v,'f',i?2:0)+" "+units[i];
}
QString pathText(const c::fs::path& p){
    QString s=QFile::decodeName(p.native().c_str());
    s.replace("\n","\\n").replace("\r","\\r").replace("\t","\\t");
    return s;
}
QLabel* label(const QString& t,const char* name=nullptr){auto* l=new QLabel(t);l->setWordWrap(true);l->setTextFormat(Qt::PlainText);if(name)l->setObjectName(name);return l;}
QPushButton* push(const QString& t,const char* name=nullptr){auto* b=new QPushButton(t);if(name)b->setObjectName(name);return b;}
QTableWidget* table(const QStringList& headers,const char* name){
    auto* t=new QTableWidget(0,headers.size());t->setObjectName(name);t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);t->verticalHeader()->hide();t->setAlternatingRowColors(true);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);t->horizontalHeader()->setStretchLastSection(true);return t;
}
class Donut:public QWidget{
    QVector<QPair<QString,qulonglong>> values;
public:
    Donut(){setMinimumHeight(155);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);}
    void setValues(QVector<QPair<QString,qulonglong>> v){
        values=std::move(v);QStringList lines;for(const auto& pair:values)lines<<pair.first+": "+sizeText(pair.second);setAccessibleName(lines.join("; "));update();
    }
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const QColor colors[]={QColor("#B51235"),QColor("#419991"),QColor("#E5A148"),QColor("#7489B8"),QColor("#B3B9C5")};
        const int side=qMin(132,qMin(height()-12,width()/3));const QRectF r(8,(height()-side)/2.0,side,side);
        long double sum=0;for(auto& v:values)sum+=v.second;
        p.setPen(Qt::NoPen);p.setBrush(QColor("#E5E7EB"));p.drawEllipse(r);
        int start=90*16,remaining=360*16;for(int i=0;i<values.size()&&sum>0;++i){
            const int span=i==values.size()-1?remaining:qMin(remaining,int(values[i].second/sum*360*16));p.setBrush(colors[i%5]);p.drawPie(r,start,-span);start-=span;remaining-=span;
        }
        p.setBrush(palette().color(QPalette::Window));p.drawEllipse(r.adjusted(side*.26,side*.26,-side*.26,-side*.26));
        p.setPen(palette().color(QPalette::WindowText));p.drawText(r,Qt::AlignCenter,sum>0?"Usage":"No data");
        int y=qMax(6,(height()-values.size()*24)/2);for(int i=0;i<values.size();++i){p.setPen(Qt::NoPen);p.setBrush(colors[i%5]);p.drawRoundedRect(QRect(side+22,y+5,10,10),2,2);p.setPen(palette().color(QPalette::WindowText));p.drawText(QRect(side+39,y,qMax(0,width()-side-43),23),Qt::AlignVCenter,p.fontMetrics().elidedText(values[i].first+" · "+sizeText(values[i].second),Qt::ElideRight,qMax(0,width()-side-43)));y+=24;}
    }
};
const QStringList categories={"User cache","Possible leftovers — review","Archives/installers","Media","Other files"};
void fill(QTableWidget* t,const std::vector<c::Entry>& items,bool checks){
    t->setRowCount(0);t->setRowCount(static_cast<int>(items.size()));
    for(int i=0;i<t->rowCount();++i){const auto& e=items[i];int col=0;
        if(checks){auto* item=new QTableWidgetItem;item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Unchecked);const auto raw=e.path.native();const auto display=QFile::decodeName(raw.c_str());if(QFile::encodeName(display)!=QByteArray(raw.data(),static_cast<int>(raw.size()))||display.contains('\n')||display.contains('\r')||display.contains('\t')){item->setFlags(Qt::ItemIsSelectable);item->setToolTip("Filename cannot be represented safely for cleanup review.");}t->setItem(i,col++,item);}
        t->setItem(i,col++,new QTableWidgetItem(sizeText(e.bytes)));
        t->setItem(i,col++,new QTableWidgetItem(pathText(e.path.filename())));
        auto* location=new QTableWidgetItem(pathText(e.path.parent_path()));location->setToolTip(pathText(e.path));t->setItem(i,col,location);
    }
}
}
QWidget* cleanerPage(std::function<void(const QString&)> route){
    auto* page=new QWidget;page->setObjectName("cleanerPage");auto* layout=new QVBoxLayout(page);
    layout->addWidget(label("Cleaner · Disk space intelligence","sectionTitle"));
    layout->addWidget(label("Scan first. Review exact paths and sizes. Personal files and suspected leftovers are never automatically deleted. Results stay on this device.","muted"));
    auto* modes=new QComboBox;modes->setObjectName("cleanerMode");modes->addItems({"Quick · user cache","Deep · home folder","Whole disk / mounted filesystem","Custom · selected folder and thresholds"});
    auto* volumes=new QComboBox;volumes->setObjectName("cleanerVolume");
    auto* reloadVolumes=push("Refresh disks","cleanerRefreshVolumes");
    auto* budget=new QComboBox;budget->setObjectName("cleanerVolumeBudget");budget->addItems({"Standard · 3 min / 1 million entries","Extended · 10 min / 3 million entries"});
    auto* root=new QLineEdit;root->setObjectName("cleanerRoot");root->setPlaceholderText("Absolute folder path");root->setText(QDir::homePath());
    auto* browse=push("Browse…","cleanerBrowse");auto* minimum=new QSpinBox;minimum->setObjectName("cleanerMinimum");minimum->setRange(0,1048576);minimum->setSuffix(" MiB");minimum->setValue(0);
    auto* age=new QSpinBox;age->setObjectName("cleanerAge");age->setRange(7,3650);age->setSuffix(" days");age->setValue(7);
    auto* scan=push("Start scan","cleanerScan");scan->setProperty("primary",true);auto* cancel=push("Cancel scan","cleanerCancel");cancel->setEnabled(false);
    auto* form=new QGridLayout;form->addWidget(label("Scan mode"),0,0);form->addWidget(modes,0,1,1,3);
    form->addWidget(label("Folder"),1,0);form->addWidget(root,1,1,1,2);form->addWidget(browse,1,3);
    form->addWidget(label("Minimum listed size"),2,0);form->addWidget(minimum,2,1);form->addWidget(label("Cache minimum age"),2,2);form->addWidget(age,2,3);
    auto* duplicateOpt=new QCheckBox("Compare file contents for duplicates (at least 1 MiB; up to 256 MiB read)");duplicateOpt->setObjectName("cleanerDuplicateOpt");form->addWidget(duplicateOpt,3,0,1,4);
    auto* diskLabel=label("Mounted volume");form->addWidget(diskLabel,4,0);form->addWidget(volumes,4,1,1,2);form->addWidget(reloadVolumes,4,3);
    auto* budgetLabel=label("Volume scan budget");form->addWidget(budgetLabel,5,0);form->addWidget(budget,5,1,1,3);
    auto* privileged=new QCheckBox("Include protected directories (administrator · read-only metadata)");privileged->setObjectName("cleanerAdminScan");form->addWidget(privileged,6,0,1,4);
    auto* scopeHint=label("Each mounted filesystem is scanned separately. Administrator scanning reads metadata only and cannot delete files. System cleanup requires separate authorization.","muted");form->addWidget(scopeHint,7,0,1,4);
    layout->addLayout(form);auto* actions=new QHBoxLayout;actions->addWidget(scan);actions->addWidget(cancel);actions->addStretch();layout->addLayout(actions);
    auto* status=label("Ready · no scan performed.","cleanerStatus");layout->addWidget(status);
    auto* tabs=new QTabWidget;tabs->setObjectName("cleanerTabs");layout->addWidget(tabs,1);
    auto* summary=new QWidget;auto* sl=new QVBoxLayout(summary);
    sl->addWidget(label("Filesystem capacity (whole selected volume)","sectionTitle"));auto* disk=new Donut;disk->setObjectName("cleanerDiskChart");sl->addWidget(disk);
    sl->addWidget(label("Scanned file distribution (logical sizes, not disk blocks)","sectionTitle"));auto* types=new Donut;types->setObjectName("cleanerCategoryChart");sl->addWidget(types);
    auto* totals=label("No scan data.","cleanerTotals");sl->addWidget(totals);sl->addStretch();auto* summaryScroll=new QScrollArea;summaryScroll->setWidgetResizable(true);summaryScroll->setWidget(summary);tabs->addTab(summaryScroll,"Space overview");
    auto* largest=table({"Size","File","Location"},"cleanerLargest");auto* leftovers=table({"Size","File","Location"},"cleanerLeftovers");
    auto current=std::make_shared<c::Result>();auto token=std::make_shared<std::atomic_bool>(false);auto busy=std::make_shared<bool>(false);auto scannedAge=std::make_shared<unsigned>(7);
    auto* filesPage=new QWidget;auto* fl=new QVBoxLayout(filesPage);fl->addWidget(label("Largest 250 regular files in scanned scope. Chart compares the top five only; location column shows each parent folder. Logical sizes may include hard links.","muted"));auto* bigChart=new Donut;bigChart->setObjectName("cleanerLargestChart");fl->addWidget(bigChart);fl->addWidget(largest,1);
    auto* openLargest=push("Open containing folder");auto* copyLargest=push("Copy exact path");auto* largestActions=new QHBoxLayout;largestActions->addWidget(openLargest);largestActions->addWidget(copyLargest);fl->addLayout(largestActions);tabs->addTab(filesPage,"Largest files");
    auto* reviewPage=new QWidget;auto* rl=new QVBoxLayout(reviewPage);rl->addWidget(label("Possible temporary files, old backups, logs and partial downloads. Names alone cannot establish that a file is orphaned or safe to delete.","muted"));rl->addWidget(leftovers,1);
    auto* openReview=push("Open containing folder");auto* ownership=push("Check package ownership","cleanerOwnership");auto* ownershipStatus=label("Select a possible leftover for an optional package ownership lookup.","cleanerOwnershipStatus");auto* reviewActions=new QHBoxLayout;reviewActions->addWidget(openReview);reviewActions->addWidget(ownership);rl->addLayout(reviewActions);rl->addWidget(ownershipStatus);tabs->addTab(reviewPage,"Leftovers review");
    auto* cachePage=new QWidget;auto* cl=new QVBoxLayout(cachePage);cl->addWidget(label("Only old, single-link regular files owned by you inside your account's ~/.cache qualify. Close affected applications first. Select individual files; deletion is permanent.","muted"));
    auto* candidates=table({"Select","Size","File","Location"},"cleanerCandidates");cl->addWidget(candidates,1);
    auto* remove=push("Review and delete selected cache files…","cleanerDelete");remove->setEnabled(false);cl->addWidget(remove);
    auto* apt=push("System APT cache · administrator approval");auto* journal=push("Archived journals · administrator approval");auto* links=new QHBoxLayout;links->addWidget(apt);links->addWidget(journal);cl->addLayout(links);tabs->addTab(cachePage,"Cleanup candidates");
    QObject::connect(apt,&QPushButton::clicked,page,[route]{route("apt-clean");});QObject::connect(journal,&QPushButton::clicked,page,[route]{route("journal-prune");});
    auto* packagesPage=new QWidget;auto* pl=new QVBoxLayout(packagesPage);pl->addWidget(label("Installed Debian packages by reported installed size. Includes dependencies and system components; size is not proof of obsolescence.","muted"));
    auto* packageStatus=label("Inventory not loaded.","cleanerPackageStatus");pl->addWidget(packageStatus);
    auto* packageFilter=new QLineEdit;packageFilter->setPlaceholderText("Filter package name");pl->addWidget(packageFilter);
    auto* packages=table({"Installed size","Package"},"cleanerPackages");pl->addWidget(packages,1);
    auto* inventory=push("Load installed packages","cleanerPackageScan");auto* orphan=push("Preview unused dependencies");auto* manager=push("Open package manager");
    auto* pr=new QHBoxLayout;pr->addWidget(inventory);pr->addWidget(orphan);pr->addWidget(manager);pl->addLayout(pr);tabs->addTab(packagesPage,"Applications & packages");
    QObject::connect(orphan,&QPushButton::clicked,page,[route]{route("apt-preview");});
    QObject::connect(manager,&QPushButton::clicked,page,[page]{const auto app=tux::trustedSystemExecutable("synaptic");if(app.isEmpty()||!QProcess::startDetached(app,QStringList{}))QMessageBox::information(page,"Package manager","Synaptic is unavailable. Use your distribution's installed software manager.");});
    QObject::connect(packageFilter,&QLineEdit::textChanged,page,[packages](const QString& q){for(int i=0;i<packages->rowCount();++i)packages->setRowHidden(i,!packages->item(i,1)->text().contains(q,Qt::CaseInsensitive));});
    QObject::connect(inventory,&QPushButton::clicked,page,[page,inventory,packages,packageStatus,packageFilter]{
        const auto dpkg=tux::trustedSystemExecutable("dpkg-query");if(dpkg.isEmpty()){packageStatus->setText("dpkg-query unavailable.");return;}
        inventory->setEnabled(false);packages->setRowCount(0);packageStatus->setText("Reading package database…");
        AsyncCommand::run(page,dpkg,{"-W","-f=${db:Status-Status}\\t${binary:Package}\\t${Installed-Size}\\n"},15000,2*1024*1024,[inventory,packages,packageStatus,packageFilter](const AsyncCommandResult& result){
            inventory->setEnabled(true);if(!result.ok()){packageStatus->setText("Package inventory failed or exceeded output limit.");return;}
            QVector<QPair<qulonglong,QString>> rows;int skipped=0;
            for(const auto& line:QFile::decodeName(result.output).split('\n',Qt::SkipEmptyParts)){const auto fields=line.split('\t');if(fields.size()!=3){++skipped;continue;}if(fields[0]!="installed")continue;bool ok=false;auto kib=fields[2].toULongLong(&ok);if(!ok||kib>std::numeric_limits<qulonglong>::max()/1024||fields[1].isEmpty()){++skipped;continue;}rows.append(qMakePair(static_cast<qulonglong>(kib*1024),fields[1]));}
            std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.first>b.first;});const int count=qMin(10000,rows.size());packages->setRowCount(count);
            for(int i=0;i<count;++i){packages->setItem(i,0,new QTableWidgetItem(sizeText(rows[i].first)));packages->setItem(i,1,new QTableWidgetItem(rows[i].second));packages->setRowHidden(i,!rows[i].second.contains(packageFilter->text(),Qt::CaseInsensitive));}
            packageStatus->setText(QString("%1 installed packages listed · %2 invalid entries · %3 omitted by limit. Review removals in the distribution package manager.").arg(count).arg(skipped).arg(rows.size()-count));
        });
    });
    auto* dupPage=new QWidget;auto* dl=new QVBoxLayout(dupPage);
    auto* dupStatus=label("Duplicate comparison is optional and read-only. Enable it before scanning.","cleanerDuplicateStatus");dl->addWidget(dupStatus);
    auto* dupTable=table({"Group","Logical size","File","Location"},"cleanerDuplicates");dl->addWidget(dupTable,1);
    auto* dupOpen=push("Open containing folder");dl->addWidget(dupOpen);tabs->addTab(dupPage,"Duplicate files");
    auto duplicateResult=std::make_shared<c::DuplicateResult>();
    QObject::connect(dupOpen,&QPushButton::clicked,page,[=]{const int r=dupTable->currentRow();if(r<0||!dupTable->item(r,0))return;
        const auto g=dupTable->item(r,0)->data(Qt::UserRole).toUInt(),i=dupTable->item(r,0)->data(Qt::UserRole+1).toUInt();
        if(g>=duplicateResult->groups.size()||i>=duplicateResult->groups[g].files.size())return;
        const auto raw=duplicateResult->groups[g].files[i].path.parent_path().native();const auto decoded=QFile::decodeName(raw.c_str());
        if(QFile::encodeName(decoded)!=QByteArray(raw.data(),static_cast<int>(raw.size()))||!QDesktopServices::openUrl(QUrl::fromLocalFile(decoded)))QMessageBox::information(page,"Open folder","The folder could not be opened.");
    });
    const auto open=[page,current](QTableWidget* t,bool review){int i=t->currentRow();const auto& v=review?current->review:current->largest;if(i<0||i>=static_cast<int>(v.size()))return;
        const auto raw=v[i].path.parent_path().native();const auto decoded=QFile::decodeName(raw.c_str());if(QFile::encodeName(decoded)!=QByteArray(raw.data(),static_cast<int>(raw.size()))||!QDesktopServices::openUrl(QUrl::fromLocalFile(decoded)))QMessageBox::information(page,"Open folder","The folder could not be opened.");};
    QObject::connect(openLargest,&QPushButton::clicked,page,[open,largest]{open(largest,false);});QObject::connect(openReview,&QPushButton::clicked,page,[open,leftovers]{open(leftovers,true);});
    QObject::connect(copyLargest,&QPushButton::clicked,page,[current,largest]{
        const int i=largest->currentRow();if(i<0||i>=static_cast<int>(current->largest.size()))return;
        const auto raw=current->largest[i].path.native();const auto decoded=QFile::decodeName(raw.c_str());
        if(QFile::encodeName(decoded)==QByteArray(raw.data(),static_cast<int>(raw.size())))QApplication::clipboard()->setText(decoded);
    });
    QObject::connect(ownership,&QPushButton::clicked,page,[=]{
        const int i=leftovers->currentRow();if(*busy||i<0||i>=static_cast<int>(current->review.size()))return;
        const auto raw=current->review[i].path.native();const auto path=QFile::decodeName(raw.c_str());
        if(QFile::encodeName(path)!=QByteArray(raw.data(),static_cast<int>(raw.size()))||path.contains('*')||path.contains('?')||path.contains('[')||path.contains('\\')||path.contains('\n')||path.contains('\r')){ownershipStatus->setText("Exact ownership lookup unavailable for this filename.");return;}
        const auto dpkg=tux::trustedSystemExecutable("dpkg-query");if(dpkg.isEmpty()){ownershipStatus->setText("Package ownership lookup unavailable.");return;}
        ownership->setEnabled(false);ownershipStatus->setText("Checking registered package ownership…");
        AsyncCommand::run(page,dpkg,{"-S","--",path},5000,8192,[ownership,ownershipStatus,path](const AsyncCommandResult& result){
            ownership->setEnabled(true);
            if(!result.ok()){ownershipStatus->setText("No confirmed owner or lookup failed for "+path+". This does not establish that the file is orphaned or safe to remove.");return;}
            QStringList owners;for(const auto& line:QFile::decodeName(result.output).split('\n',Qt::SkipEmptyParts)){const int at=line.indexOf(": ");if(at>0&&line.mid(at+2)==path)owners<<line.left(at);}
            ownershipStatus->setText(owners.isEmpty()?"No exact registered owner confirmed; not proof of an orphan.":"Registered owner: "+owners.join(", ")+" · "+path);
        });
    });
    const auto cachePath=[] {return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);};
    auto customPath=std::make_shared<QString>(QDir::homePath());
    const auto refreshVolumes=[=]{
        const auto previous=volumes->currentData().toString();volumes->clear();
        const QSignalBlocker block(volumes);
        QSet<QString> added;
        const auto addVolume=[&](const QStorageInfo& v){
            if(!c::localVolume(v)||added.contains(v.rootPath()))return;
            const auto path=v.rootPath();added.insert(path);
            volumes->addItem(QString("%1 · %2 · %3%4").arg(path,QString::fromLatin1(v.fileSystemType()),sizeText(qMax<qint64>(0,v.bytesTotal())),v.isReadOnly()?" · read-only":""),path);
        };
        addVolume(QStorageInfo::root());for(const auto& v:QStorageInfo::mountedVolumes())addVolume(v);
        const int index=volumes->findData(previous);if(index>=0)volumes->setCurrentIndex(index);
    };
    auto generation=std::make_shared<unsigned long long>(0);
    auto hasResults=std::make_shared<bool>(false);
    const auto resetResults=[=]{
        ++*generation;*hasResults=false;*current={};*duplicateResult={};
        remove->setEnabled(false);fill(largest,{},false);fill(leftovers,{},false);fill(candidates,{},true);
        dupTable->setRowCount(0);disk->setValues({});types->setValues({});bigChart->setValues({});
        ownershipStatus->setText("Select a possible leftover for ownership lookup.");
        dupStatus->setText("Start a new scan for current duplicate results.");
        totals->setText("No current scan data.");
    };
    const auto invalidate=[=]{
        if(*busy)return;
        if(*hasResults){resetResults();status->setText("Scan settings changed. Start a new scan before using results.");}
    };
    const auto modeChanged=[=]{const int i=modes->currentIndex();
        if(i==2&&volumes->count()==0)refreshVolumes();
        root->setText(i==0?cachePath():i==1?QDir::homePath():i==2?volumes->currentData().toString():*customPath);
        root->setEnabled(i==3);browse->setEnabled(i==3);minimum->setEnabled(i==3);age->setEnabled(i==3);
        diskLabel->setVisible(i==2);volumes->setVisible(i==2);reloadVolumes->setVisible(i==2);budgetLabel->setVisible(i==2);budget->setVisible(i==2);
        volumes->setEnabled(i==2);reloadVolumes->setEnabled(i==2);budget->setEnabled(i==2);
        privileged->setVisible(i==2);privileged->setEnabled(i==2&&!tux::trustedSystemExecutable("pkexec").isEmpty()&&!tux::trustedSystemExecutable("syspulse-volume-scan").isEmpty());
        duplicateOpt->setEnabled(i!=2||!privileged->isChecked());
        root->setToolTip(i==0?"Actual user cache location, including XDG configuration.":i==1?"Actual desktop user home directory.":i==2?"Mounted filesystem root; choose a disk above or use Custom for a folder.":"Choose an absolute folder to scan.");
    };
    QObject::connect(root,&QLineEdit::textChanged,page,[=](const QString& value){if(modes->currentIndex()==3)*customPath=value;});
    QObject::connect(modes,QOverload<int>::of(&QComboBox::currentIndexChanged),page,[=](int){invalidate();modeChanged();});modeChanged();
    QObject::connect(volumes,QOverload<int>::of(&QComboBox::currentIndexChanged),page,[=](int){invalidate();if(modes->currentIndex()==2)modeChanged();});
    QObject::connect(reloadVolumes,&QPushButton::clicked,page,[=]{invalidate();refreshVolumes();modeChanged();});
    QObject::connect(budget,QOverload<int>::of(&QComboBox::currentIndexChanged),page,[invalidate](int){invalidate();});
    QObject::connect(privileged,&QCheckBox::toggled,page,[=](bool on){invalidate();duplicateOpt->setEnabled(!on);});
    QObject::connect(root,&QLineEdit::textChanged,page,[invalidate]{invalidate();});
    QObject::connect(minimum,QOverload<int>::of(&QSpinBox::valueChanged),page,[invalidate](int){invalidate();});
    QObject::connect(age,QOverload<int>::of(&QSpinBox::valueChanged),page,[invalidate](int){invalidate();});
    QObject::connect(duplicateOpt,&QCheckBox::toggled,page,[invalidate](bool){invalidate();});
    QObject::connect(browse,&QPushButton::clicked,page,[page,root]{const auto folder=QFileDialog::getExistingDirectory(page,"Select scan folder",root->text());if(!folder.isEmpty())root->setText(folder);});
    QObject::connect(cancel,&QPushButton::clicked,page,[token,status]{token->store(true);status->setText("Cancellation requested…");});
    QObject::connect(page,&QObject::destroyed,[token]{token->store(true);});
    QObject::connect(scan,&QPushButton::clicked,page,[=]{
        if(*busy||!ownership->isEnabled())return;
        const int mode=modes->currentIndex();
        const c::fs::path cache=QFile::encodeName(cachePath()).constData();
        const auto cleanupCache=c::homeCache();
        const c::fs::path path=QFile::encodeName(root->text()).constData();
        if(path.empty()||!path.is_absolute()){resetResults();status->setText("Choose an absolute folder path.");return;}
        if(mode==2&&!c::mountedVolume(root->text())){resetResults();status->setText("Selected disk is no longer mounted or supported. Refresh disks.");return;}
        const bool admin=mode==2&&privileged->isChecked();
        const auto helper=tux::trustedSystemExecutable("syspulse-volume-scan"),auth=tux::trustedSystemExecutable("pkexec");
        if(admin){if(helper.isEmpty()||auth.isEmpty()){status->setText("Administrative scan is unavailable; use normal-user inspection.");return;}
            if(QMessageBox::question(page,"Administrator scan","Inspect protected directory metadata on this mounted filesystem? The scan may show private filenames of other accounts locally. No file contents or deletions. It runs until completion or its bounded deadline.",QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;}
        c::Limits limits;limits.entries=mode==0?25000:mode==1?150000:mode==2?(budget->currentIndex()==1?3000000:1000000):300000;limits.seconds=mode==0?10:mode==1?45:mode==2?(budget->currentIndex()==1?600:180):60;
        if(mode==3){limits.minBytes=static_cast<std::uint64_t>(minimum->value())*1048576;limits.ageDays=age->value();}
        limits.collectDuplicates=!admin&&duplicateOpt->isChecked();
        *scannedAge=limits.ageDays;*busy=true;token->store(false);scan->setEnabled(false);cancel->setEnabled(!admin);remove->setEnabled(false);
        duplicateOpt->setEnabled(false);modes->setEnabled(false);root->setEnabled(false);browse->setEnabled(false);minimum->setEnabled(false);age->setEnabled(false);volumes->setEnabled(false);reloadVolumes->setEnabled(false);budget->setEnabled(false);privileged->setEnabled(false);
        resetResults();dupStatus->setText(limits.collectDuplicates?"Scanning and comparing selected file scope…":"Duplicate comparison not requested.");totals->setText("Scan running…");
        status->setText("Scanning "+pathText(path)+(admin?" · awaiting administrator authorization; read-only metadata…":" · bounded, same filesystem, no symlink traversal…"));
        auto result=std::make_shared<c::Result>();
        auto comparison=std::make_shared<c::DuplicateResult>();
        const auto finish=[=]{
            *current=std::move(*result);*duplicateResult=std::move(*comparison);
            if(token->load()){current->cancelled=true;current->complete=false;}
            duplicateOpt->setEnabled(true);scan->setEnabled(true);cancel->setEnabled(false);modes->setEnabled(true);modeChanged();*busy=false;
            *hasResults=true;
            if(QFile::encodeName(root->text())!=QByteArray(path.native().c_str())){resetResults();status->setText("Scan location changed during execution. Start a new scan.");return;}
            fill(largest,current->largest,false);fill(leftovers,current->review,false);fill(candidates,current->candidates,true);
            if(current->volume.known)disk->setValues({{"Used",current->volume.used},{"Available",current->volume.available},{"Reserved/free unavailable",current->volume.reserved}});
            if(!current->error.empty()){
                status->setText(QString::fromStdString(current->error));
                totals->setText("Scan failed. No disk or file totals are available.");
                dupStatus->setText("No duplicate results: scan did not complete.");
                return;
            }
            QVector<QPair<QString,qulonglong>> series;for(int i=0;i<c::Count;++i)series.append({categories[i],current->categories[i]});types->setValues(series);
            QVector<QPair<QString,qulonglong>> big;for(unsigned i=0;i<current->largest.size()&&i<5;++i)big.append({pathText(current->largest[i].path.filename()),current->largest[i].bytes});bigChart->setValues(big);
            status->setText(!current->error.empty()?QString::fromStdString(current->error):QString("%1 · %2 entries · %3 regular files · %4 permission-denied · %5 other skipped · %6 mount/alias exclusions").arg(current->cancelled?"Cancelled — partial results":current->limited?"Limit reached — partial results":current->complete?"Scan complete":"Partial scan — inaccessible or changed entries").arg(current->visited).arg(current->files).arg(current->permissionDenied).arg(current->otherSkipped).arg(current->mountExcluded));
            totals->setText(QString("%1 largest files shown · %2 possible leftovers · %3 old cache candidates. Scan totals are logical bytes and may include hard links or sparse files.").arg(current->largest.size()).arg(current->review.size()).arg(current->candidates.size()));
            if(!current->accountingOverflow)totals->setText(totals->text()+QString("\nUnique-inode logical size: %1 · reported allocated blocks: %2 · %3 hard-link aliases excluded. Shared extents/compression can affect actual reclaimable space.").arg(sizeText(current->logical),sizeText(current->allocated)).arg(current->hardlinkAliases));
            else totals->setText(totals->text()+"\nUnique-inode accounting overflow; totals unavailable.");
            if(limits.collectDuplicates){
                int row=0;for(unsigned g=0;g<duplicateResult->groups.size();++g)for(unsigned i=0;i<duplicateResult->groups[g].files.size();++i){
                    const auto& e=duplicateResult->groups[g].files[i];dupTable->insertRow(row);auto* id=new QTableWidgetItem(QString::number(g+1));id->setData(Qt::UserRole,g);id->setData(Qt::UserRole+1,i);dupTable->setItem(row,0,id);dupTable->setItem(row,1,new QTableWidgetItem(sizeText(e.bytes)));dupTable->setItem(row,2,new QTableWidgetItem(pathText(e.path.filename())));dupTable->setItem(row,3,new QTableWidgetItem(pathText(e.path.parent_path())));++row;
                }
                const bool partial=!current->complete||current->duplicateInputLimited||duplicateResult->limited||duplicateResult->cancelled||duplicateResult->unverified;
                dupStatus->setText(QString("%1 · %2 byte-matched groups · %3 logical extra copies · %4 read · %5 comparisons · %6 unverified. Results are scan-time evidence only; never delete automatically.").arg(partial?"Partial results":"Comparison complete").arg(duplicateResult->groups.size()).arg(sizeText(duplicateResult->logicalExtra),sizeText(duplicateResult->bytesRead)).arg(duplicateResult->compared).arg(duplicateResult->unverified));
            }
            if(current->permissionDenied)status->setText(status->text()+" · protected entries not scanned; use authorized system cleanup tools where applicable. Do not run SysPulse as root.");
            if(current->missingCache)status->setText("Cache directory does not exist yet · nothing to scan or clean. Deep scan remains available.");
            else if(current->redirected)status->setText(status->text()+" · resolved folder: "+pathText(current->resolvedRoot)+" · inspection only");
            else if(current->readOnly)status->setText(status->text()+(admin?" · administrator metadata inspection only; no deletion":" · inspection only (nonstandard or unavailable cleanup cache)"));
            current->duplicateInput.clear();
            remove->setEnabled(current->error.empty()&&!current->readOnly&&!current->cancelled&&!duplicateResult->cancelled&&!current->candidates.empty());
        };
        if(admin){
            tux::admin::runReviewed(page,{"volume",root->text(),budget->currentIndex()==1?"extended":"standard"},auth,{helper,root->text(),budget->currentIndex()==1?"extended":"standard"},4*1024*1024,[=](const AsyncCommandResult& response){
                if(!response.ok()||!c::decodeInspection(response.output,path,*result))result->error="Administrative scan failed, was denied, exceeded its deadline, or returned invalid data. No cleanup performed.";
                finish();
            });
        }else{
        auto* thread=QThread::create([result,comparison,path,cache,cleanupCache,limits,token,mode]{try{*result=c::scanRequested(path,cache,cleanupCache,limits,*token,mode==0);if(limits.collectDuplicates&&!result->cancelled&&result->error.empty())*comparison=c::duplicates(result->duplicateInput,*token);}catch(const std::exception&){result->error="Scan interrupted by a filesystem or resource error.";}}); 
            QObject::connect(thread,&QThread::finished,page,finish);QObject::connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();
        }
    });
    QObject::connect(remove,&QPushButton::clicked,page,[=]{
        if(*busy||!*hasResults||current->readOnly||!current->error.empty()||current->cancelled||duplicateResult->cancelled)return;
        const auto reviewedGeneration=*generation;const auto reviewedAge=*scannedAge;
        std::vector<c::Entry> selected;std::uint64_t logical=0;for(int i=0;i<candidates->rowCount();++i)if(candidates->item(i,0)->checkState()==Qt::Checked){const auto& e=current->candidates[i];if(e.bytes>std::numeric_limits<std::uint64_t>::max()-logical){QMessageBox::warning(page,"Cleanup","Selected sizes exceed the safe accounting limit.");return;}selected.push_back(e);logical+=e.bytes;}
        if(selected.empty()){QMessageBox::information(page,"Cleanup","Select the exact cache files to remove.");return;}
        if(selected.size()>100){QMessageBox::information(page,"Cleanup","Select at most 100 files per batch.");return;}
        if(QMessageBox::question(page,"Permanently remove selected cache files?",QString("Delete %1 selected cache files (%2 logical bytes)? Close affected applications first. Changed, linked or unsafe files will be refused. This is not reversible.").arg(selected.size()).arg(sizeText(logical)),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(*busy||!*hasResults||*generation!=reviewedGeneration){QMessageBox::warning(page,"Cleanup","Scan results changed during review. Rescan and review again.");return;}
        const auto cache=c::homeCache();unsigned removed=0,failed=0;std::uint64_t bytes=0;
        for(const auto& e:selected){if(c::removeCache(cache,e,reviewedAge)){++removed;bytes+=e.bytes;}else ++failed;}
        resetResults();dupStatus->setText("Cleanup changed the filesystem; rescan for current duplicate results.");status->setText(QString("Removed %1 files · %2 refused/failed · %3 logical bytes removed. Start a new scan to measure actual volume availability.").arg(removed).arg(failed).arg(sizeText(bytes)));totals->setText("Previous scan results are stale after cleanup; rescan before further action.");
    });
    return page;
}
}
