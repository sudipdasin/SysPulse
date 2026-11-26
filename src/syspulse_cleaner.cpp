// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/cleaner_page.hpp"
#include "syspulse/cleaner.hpp"
#include "tux/async_command.hpp"
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
    auto* modes=new QComboBox;modes->setObjectName("cleanerMode");modes->addItems({"Quick · user cache","Deep · home folder","Advanced · selected filesystem","Custom · selected folder and thresholds"});
    auto* root=new QLineEdit;root->setObjectName("cleanerRoot");root->setPlaceholderText("Absolute folder path");root->setText(QDir::homePath());
    auto* browse=push("Browse…");auto* minimum=new QSpinBox;minimum->setObjectName("cleanerMinimum");minimum->setRange(0,1048576);minimum->setSuffix(" MiB");minimum->setValue(0);
    auto* age=new QSpinBox;age->setObjectName("cleanerAge");age->setRange(7,3650);age->setSuffix(" days");age->setValue(7);
    auto* scan=push("Start scan","cleanerScan");scan->setProperty("primary",true);auto* cancel=push("Cancel scan","cleanerCancel");cancel->setEnabled(false);
    auto* form=new QGridLayout;form->addWidget(label("Scan mode"),0,0);form->addWidget(modes,0,1,1,3);
    form->addWidget(label("Folder"),1,0);form->addWidget(root,1,1,1,2);form->addWidget(browse,1,3);
    form->addWidget(label("Minimum listed size"),2,0);form->addWidget(minimum,2,1);form->addWidget(label("Cache minimum age"),2,2);form->addWidget(age,2,3);
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
    auto* apt=push("Review system APT download cache");auto* journal=push("Review archived journal cleanup");auto* links=new QHBoxLayout;links->addWidget(apt);links->addWidget(journal);cl->addLayout(links);tabs->addTab(cachePage,"Cleanup candidates");
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
            for(const auto& line:result.output.split('\n',Qt::SkipEmptyParts)){const auto fields=line.split('\t');if(fields.size()!=3){++skipped;continue;}if(fields[0]!="installed")continue;bool ok=false;auto kib=fields[2].toULongLong(&ok);if(!ok||kib>std::numeric_limits<qulonglong>::max()/1024||fields[1].isEmpty()){++skipped;continue;}rows.append({kib*1024,fields[1]});}
            std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.first>b.first;});const int count=qMin(10000,rows.size());packages->setRowCount(count);
            for(int i=0;i<count;++i){packages->setItem(i,0,new QTableWidgetItem(sizeText(rows[i].first)));packages->setItem(i,1,new QTableWidgetItem(rows[i].second));packages->setRowHidden(i,!rows[i].second.contains(packageFilter->text(),Qt::CaseInsensitive));}
            packageStatus->setText(QString("%1 installed packages listed · %2 invalid entries · %3 omitted by limit. Review removals in the distribution package manager.").arg(count).arg(skipped).arg(rows.size()-count));
        });
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
            QStringList owners;for(const auto& line:result.output.split('\n',Qt::SkipEmptyParts)){const int at=line.indexOf(": ");if(at>0&&line.mid(at+2)==path)owners<<line.left(at);}
            ownershipStatus->setText(owners.isEmpty()?"No exact registered owner confirmed; not proof of an orphan.":"Registered owner: "+owners.join(", ")+" · "+path);
        });
    });
    const auto modeChanged=[modes,root,browse,minimum,age]{const int i=modes->currentIndex();root->setEnabled(i>=2);browse->setEnabled(i>=2);minimum->setEnabled(i==3);age->setEnabled(i==3);};
    QObject::connect(modes,QOverload<int>::of(&QComboBox::currentIndexChanged),page,[modeChanged](int){modeChanged();});modeChanged();
    QObject::connect(browse,&QPushButton::clicked,page,[page,root]{const auto folder=QFileDialog::getExistingDirectory(page,"Select scan folder",root->text());if(!folder.isEmpty())root->setText(folder);});
    QObject::connect(cancel,&QPushButton::clicked,page,[token,status]{token->store(true);status->setText("Cancellation requested…");});
    QObject::connect(page,&QObject::destroyed,[token]{token->store(true);});
    QObject::connect(scan,&QPushButton::clicked,page,[=]{
        if(*busy||!ownership->isEnabled())return;
        const int mode=modes->currentIndex();const auto cache=c::homeCache();c::fs::path path;
        if(mode==0)path=cache;else if(mode==1)path=cache.empty()?c::fs::path{}:cache.parent_path();else path=QFile::encodeName(root->text()).constData();
        if(path.empty()||!path.is_absolute()){status->setText("Choose an absolute folder path.");return;}
        c::Limits limits;limits.entries=mode==0?25000:mode==1?150000:300000;limits.seconds=mode==0?10:mode==1?45:60;
        if(mode==3){limits.minBytes=static_cast<std::uint64_t>(minimum->value())*1048576;limits.ageDays=age->value();}
        *scannedAge=limits.ageDays;*busy=true;token->store(false);scan->setEnabled(false);cancel->setEnabled(true);remove->setEnabled(false);
        modes->setEnabled(false);root->setEnabled(false);browse->setEnabled(false);minimum->setEnabled(false);age->setEnabled(false);
        *current={};fill(largest,current->largest,false);fill(leftovers,current->review,false);fill(candidates,current->candidates,true);disk->setValues({});types->setValues({});bigChart->setValues({});ownershipStatus->setText("Select a possible leftover for ownership lookup.");totals->setText("Scan running…");
        status->setText("Scanning "+pathText(path)+" · bounded, same filesystem, no symlink traversal…");
        auto result=std::make_shared<c::Result>();
        auto* thread=QThread::create([result,path,cache,limits,token]{try{*result=c::scan(path,cache,limits,*token);}catch(const std::exception&){result->error="Scan interrupted by a filesystem or resource error.";}}); 
        QObject::connect(thread,&QThread::finished,page,[=]{
            *busy=false;*current=std::move(*result);scan->setEnabled(true);cancel->setEnabled(false);modes->setEnabled(true);modeChanged();
            fill(largest,current->largest,false);fill(leftovers,current->review,false);fill(candidates,current->candidates,true);
            if(current->volume.known)disk->setValues({{"Used",current->volume.used},{"Available",current->volume.available},{"Reserved/free unavailable",current->volume.reserved}});
            QVector<QPair<QString,qulonglong>> series;for(int i=0;i<c::Count;++i)series.append({categories[i],current->categories[i]});types->setValues(series);
            QVector<QPair<QString,qulonglong>> big;for(unsigned i=0;i<current->largest.size()&&i<5;++i)big.append({pathText(current->largest[i].path.filename()),current->largest[i].bytes});bigChart->setValues(big);
            status->setText(!current->error.empty()?QString::fromStdString(current->error):QString("%1 · %2 entries · %3 regular files · %4 skipped").arg(current->cancelled?"Cancelled — partial results":current->limited?"Limit reached — partial results":current->complete?"Scan complete":"Partial scan — inaccessible or excluded entries").arg(current->visited).arg(current->files).arg(current->skipped));
            totals->setText(QString("%1 largest files shown · %2 possible leftovers · %3 old cache candidates. Scan totals are logical bytes and may include hard links or sparse files.").arg(current->largest.size()).arg(current->review.size()).arg(current->candidates.size()));
            remove->setEnabled(current->error.empty()&&!current->cancelled&&!current->candidates.empty());
        });QObject::connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();
    });
    QObject::connect(remove,&QPushButton::clicked,page,[=]{
        if(*busy||current->cancelled)return;
        std::vector<c::Entry> selected;std::uint64_t logical=0;for(int i=0;i<candidates->rowCount();++i)if(candidates->item(i,0)->checkState()==Qt::Checked){const auto& e=current->candidates[i];if(e.bytes>std::numeric_limits<std::uint64_t>::max()-logical){QMessageBox::warning(page,"Cleanup","Selected sizes exceed the safe accounting limit.");return;}selected.push_back(e);logical+=e.bytes;}
        if(selected.empty()){QMessageBox::information(page,"Cleanup","Select the exact cache files to remove.");return;}
        if(selected.size()>100){QMessageBox::information(page,"Cleanup","Select at most 100 files per batch.");return;}
        if(QMessageBox::question(page,"Permanently remove selected cache files?",QString("Delete %1 selected cache files (%2 logical bytes)? Close affected applications first. Changed, linked or unsafe files will be refused. This is not reversible.").arg(selected.size()).arg(sizeText(logical)),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        const auto cache=c::homeCache();unsigned removed=0,failed=0;std::uint64_t bytes=0;
        for(const auto& e:selected){if(c::removeCache(cache,e,*scannedAge)){++removed;bytes+=e.bytes;}else ++failed;}
        remove->setEnabled(false);fill(candidates,{},true);status->setText(QString("Removed %1 files · %2 refused/failed · %3 logical bytes removed. Start a new scan to measure actual volume availability.").arg(removed).arg(failed).arg(sizeText(bytes)));totals->setText("Previous scan results are stale after cleanup; rescan before further action.");
    });
    return page;
}
}
