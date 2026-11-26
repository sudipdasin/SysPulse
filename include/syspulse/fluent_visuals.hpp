// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QIcon>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QString>
#include <initializer_list>
#include <QApplication>
#include <QPushButton>
#include <QToolButton>
#include <QTabWidget>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QHash>
namespace syspulse::fluent {
inline QIcon glyph(const QString& id,bool light=false) {
    static QHash<QString,QIcon> cache;
    const QString key=id+(light?"/light":"/dark");const auto found=cache.constFind(key);if(found!=cache.cend())return *found;
    QPixmap pix(96,96); pix.fill(Qt::transparent); pix.setDevicePixelRatio(2);
    QPainter p(&pix); p.scale(2,2);p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(light?"#FFFFFF":"#46647F"),1.65,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    auto line=[&](qreal x,qreal y,qreal a,qreal b){p.drawLine(QPointF(x,y),QPointF(a,b));};
    auto rect=[&](qreal x,qreal y,qreal a,qreal b){p.drawRoundedRect(QRectF(x,y,a,b),2,2);};
    auto circle=[&](qreal x,qreal y,qreal r){p.drawEllipse(QPointF(x,y),r,r);};
    auto path=[&](std::initializer_list<QPointF> pts){QPolygonF poly;for(const auto& pt:pts)poly<<pt;p.drawPolyline(poly);};
    if(id=="overview"){rect(3,3,18,18);line(12,3,12,21);line(3,12,21,12);}
    else if(id=="performance"){rect(3,4,18,16);path({{5,14},{8,14},{11,8},{14,16},{17,11},{19,11}});}
    else if(id=="storage"){rect(3,5,18,14);line(3,14,21,14);circle(17,16.5,.65);circle(14,16.5,.65);}
    else if(id=="control"){for(int y:{6,12,18})line(4,y,20,y);circle(9,6,2);circle(15,12,2);circle(11,18,2);}
    else if(id=="management"){rect(4,3,16,18);for(int y:{7,12,17}){line(7,y,9,y);line(12,y,17,y);}}
    else if(id=="network"){circle(12,12,9);p.drawEllipse(QRectF(8,3,8,18));line(3,12,21,12);}
    else if(id=="desktop"){rect(3,4,18,13);line(12,17,12,21);line(8,21,16,21);}
    else if(id=="security"){path({{12,3},{20,6},{19,14},{16,18},{12,21},{8,18},{5,14},{4,6},{12,3}});path({{8,12},{11,15},{16,9}});}
    else if(id=="schedules"){rect(3,5,18,16);line(3,10,21,10);line(8,3,8,7);line(16,3,16,7);circle(12,15,2);}
    else if(id=="recovery"||id=="updates"){p.drawArc(QRectF(4,4,16,16),25*16,285*16);path({{4,5},{4,11},{10,11}});}
    else if(id=="reports"){rect(5,3,14,18);line(8,8,16,8);line(8,12,16,12);line(8,16,14,16);}
    else if(id=="experimental"){path({{9,3},{15,3}});path({{10,3},{10,9},{4,19},{5,21},{19,21},{20,19},{14,9},{14,3}});line(7,15,17,15);}
    else if(id=="settings"){circle(12,12,7);circle(12,12,2.5);line(12,2,12,5);line(12,19,12,22);line(2,12,5,12);line(19,12,22,12);line(5,5,7,7);line(17,17,19,19);line(5,19,7,17);line(17,7,19,5);}
    else if(id=="keyboard"){rect(2,5,20,14);for(int y:{8,11,14})for(int x:{5,8,11,14,17})line(x,y,x+1,y);line(8,17,16,17);}
    else if(id=="mouse"){rect(6,3,12,18);line(6,10,18,10);line(12,3,12,10);}
    else if(id=="power"){path({{13,2},{5,13},{11,13},{10,22},{19,10},{13,10},{13,2}});}
    else if(id=="panel"){rect(3,4,18,16);line(3,15,21,15);for(int x:{6,9,12,18})circle(x,17.5,.55);}
    else if(id=="session"){circle(12,8,4);p.drawArc(QRectF(4,13,16,12),0,180*16);}
    else if(id=="appearance"){p.drawEllipse(QRectF(3,3,18,18));for(const QPointF& a:{QPointF(8,8),QPointF(13,6),QPointF(17,10),QPointF(7,14)})circle(a.x(),a.y(),1);p.drawArc(QRectF(12,13,7,7),0,200*16);}
    else if(id=="windows"){rect(3,4,15,12);rect(7,8,14,12);line(7,11,21,11);}
    else if(id=="printer"){rect(6,2,12,6);rect(3,8,18,10);rect(6,15,12,7);line(17,11,19,11);}
    else if(id=="audio"){path({{3,9},{7,9},{12,5},{12,19},{7,15},{3,15},{3,9}});p.drawArc(QRectF(9,6,12,12),-60*16,120*16);}
    else if(id=="bluetooth"){path({{7,6},{17,16},{12,21},{12,3},{17,8},{7,18}});}
    else if(id=="folder"){path({{3,7},{3,19},{21,19},{21,8},{12,8},{9,5},{3,5},{3,7}});}
    else if(id=="key"){circle(8,8,4);path({{11,11},{20,20},{21,17},{18,14},{16,16}});}
    else if(id=="package"){path({{3,7},{12,3},{21,7},{21,17},{12,21},{3,17},{3,7},{12,11},{21,7}});line(12,11,12,21);}
    else if(id=="refresh"){p.drawArc(QRectF(4,4,16,16),35*16,280*16);path({{20,3},{20,9},{14,9}});}
    else if(id=="play"){path({{8,4},{19,12},{8,20},{8,4}});}
    else if(id=="stop"){rect(5,5,14,14);}
    else if(id=="pause"){rect(6,4,4,16);rect(14,4,4,16);}
    else if(id=="close"){line(6,6,18,18);line(18,6,6,18);}
    else if(id=="check"){path({{4,12},{9,17},{20,6}});}
    else if(id=="export"){rect(4,9,16,12);path({{8,7},{12,3},{16,7}});line(12,3,12,15);}
    else if(id=="delete"){line(4,6,20,6);line(9,3,15,3);rect(6,6,12,15);line(10,10,10,17);line(14,10,14,17);}
    else if(id=="back"){path({{14,5},{7,12},{14,19}});line(7,12,21,12);}
    else if(id=="forward"){path({{10,5},{17,12},{10,19}});line(3,12,17,12);}
    else if(id=="search"){circle(10,10,6);line(14.5,14.5,20,20);}
    else if(id=="help"){circle(12,12,9);p.drawArc(QRectF(9,6,6,6),0,180*16);path({{15,9},{15,11},{12,13},{12,15}});circle(12,18,.45);}
    else {rect(4,4,16,16);line(8,9,16,9);line(8,14,16,14);}
    p.end(); QIcon icon(pix);cache.insert(key,icon);return icon;
}
inline QString tabGlyph(QString title) {
    title=title.toLower().remove('&');
    if(title=="panel")return "panel";
    if(title.contains("keyboard")||title.contains("shortcut"))return "keyboard";
    if(title.contains("mouse")||title.contains("touchpad")||title.contains("input device"))return "mouse";
    if(title.contains("power")||title.contains("battery")||title.contains("sleep"))return "power";
    if(title.contains("window manager")||title.contains("window tweaks"))return "windows";
    if(title.contains("appearance")||title.contains("background")||title.contains("theme")||title.contains("color management"))return "appearance";
    if(title.contains("panel settings"))return "panel";
    if(title.contains("session")||title.contains("account")||title.contains("user")||title.contains("login"))return "session";
    if(title.contains("printer"))return "printer";
    if(title.contains("sound")||title.contains("audio"))return "audio";
    if(title.contains("bluetooth"))return "bluetooth";
    if(title.contains("password")||title.contains("keyring"))return "key";
    if(title.contains("file manager")||title.contains("folder")||title.contains("browse"))return "folder";
    if(title.contains("export")||title.contains("save")||title.contains("copy"))return "export";
    if(title.contains("refresh")||title.contains("recheck")||title.contains("reload"))return "refresh";
    if(title.contains("pause"))return "pause";
    if(title.contains("stop"))return "stop";
    if(title.contains("resume")||title.contains("start diagnostic"))return "play";
    if(title.contains("cancel")||title.contains("close"))return "close";
    if(title.contains("delete")||title.contains("remove"))return "delete";
    if(title.contains("package")||title.contains("software")||title.contains("driver"))return "package";
    if(title.contains("help")||title.contains("license")||title.contains("feedback"))return "help";
    if(title.contains("firewall")||title.contains("permission"))return "security";
    if(title.contains("backup")||title.contains("recover")||title.contains("undo"))return "recovery";
    if(title.contains("schedule")||title.contains("timer"))return "schedules";
    if(title.contains("administration")||title.contains("control panel"))return "control";
    if(title.contains("find")||title.contains("locate")||title.contains("search"))return "search";
    if(title.contains("review")||title.contains("check")||title.contains("scan"))return "check";
    if(title.contains("memory")||title.contains("cpu")||title.contains("performance")||title.contains("swap"))return "performance";
    if(title.contains("disk")||title.contains("storage")||title.contains("clean"))return "storage";
    if(title.contains("network"))return "network";
    if(title.contains("startup")||title.contains("service")||title.contains("process"))return "management";
    if(title.contains("security")||title.contains("diagnostic")||title.contains("care"))return "security";
    if(title.contains("desktop")||title.contains("display"))return "desktop";
    if(title.contains("update")||title.contains("maintenance"))return "updates";
    if(title.contains("setting")||title.contains("index"))return "settings";
    return "reports";
}
inline QString plainTitle(QString title) {return title.replace("&&","\x01").remove('&').replace("\x01","&").trimmed();}
inline QString descriptionFor(const QString& title) {
    const auto n=plainTitle(title).toLower();
    if(n.contains("display")||n.contains("display resolution"))return "Resolution, scaling and monitor arrangement.";
    if(n.contains("keyboard"))return "Keyboard layouts, repeat and desktop shortcuts.";
    if(n.contains("mouse")||n.contains("touchpad"))return "Pointer speed, buttons and touchpad preferences.";
    if(n.contains("power manager"))return "Power, sleep, lid and battery preferences.";
    if(n.contains("panel"))return "Panel position, size, launchers and plugins.";
    if(n.contains("session"))return "Session behavior and desktop startup preferences.";
    if(n.contains("appearance"))return "Themes, icons, fonts and visual preferences.";
    if(n.contains("window manager"))return "Window borders, focus and window behavior.";
    if(n.contains("desktop background")||n.contains("desktop preferences"))return "Wallpaper, desktop icons and desktop behavior.";
    if(n.contains("accessibility"))return "Keyboard and pointer accessibility preferences.";
    if(n.contains("notification"))return "Desktop notification behavior and preferences.";
    if(n.contains("preferred applications"))return "Choose default applications for common tasks.";
    if(n.contains("file manager"))return "File browsing and file manager preferences.";
    if(n.contains("printer"))return "View and configure installed printers.";
    if(n.contains("bluetooth"))return "View Bluetooth devices and connection settings.";
    if(n.contains("sound"))return "Audio devices, input and output preferences.";
    if(n.contains("network connection"))return "Network profiles and connection configuration.";
    if(n.contains("firewall"))return "Review firewall rules; authorization may be required.";
    if(n.contains("partition"))return "Inspect partitions; back up data before making changes.";
    if(n.contains("snapshot")||n.contains("timeshift"))return "Manage system snapshots; not personal-file backups.";
    if(n.contains("recovery"))return "Review recovery options before restoring files.";
    if(n.contains("backup"))return "Review backup locations and saved copies.";
    if(n.contains("cache"))return "Review cache cleanup options before removing files.";
    if(n.contains("disk")||n.contains("storage"))return "Inspect storage devices and disk information.";
    if(n.contains("update"))return "Review update availability and installation options.";
    if(n.contains("software")||n.contains("package"))return "Review installed software and package management options.";
    if(n.contains("user")||n.contains("account")||n.contains("login"))return "Review identity, login and account settings.";
    if(n.contains("driver"))return "Review supported hardware driver options.";
    if(n.contains("password")||n.contains("keyring"))return "Review locally managed credentials and keys.";
    if(n.contains("diagnostic"))return "Review diagnostic options and inspect results.";
    if(n.contains("administration"))return "Open administration tools; native authorization may apply.";
    if(n.contains("performance")||n.contains("memory"))return "Inspect resource usage and performance information.";
    return {};
}
inline QString hint(const QString& title) {const auto clean=plainTitle(title);const auto detail=descriptionFor(clean);return detail.isEmpty()?QString("Select %1.").arg(clean):detail;}
inline void decorate(QWidget* widget) {
    if(auto* b=qobject_cast<QPushButton*>(widget)) {
        const auto label=plainTitle(b->text());
        if(!label.isEmpty()){
            if(b->icon().isNull()){b->setIcon(glyph(tabGlyph(label),b->property("primary").toBool()||b->objectName()=="primaryAction"));b->setIconSize(QSize(20,20));}
            if(b->toolTip().isEmpty())b->setToolTip(hint(label));
            if(b->accessibleName().isEmpty())b->setAccessibleName(label);
        }
    }else if(auto* b=qobject_cast<QToolButton*>(widget)) {
        const auto label=plainTitle(b->text());
        if(!label.isEmpty()){if(b->icon().isNull())b->setIcon(glyph(tabGlyph(label)));if(b->toolTip().isEmpty())b->setToolTip(hint(label));if(b->accessibleName().isEmpty())b->setAccessibleName(label);}
    }else if(auto* tabs=qobject_cast<QTabWidget*>(widget)){
        tabs->setIconSize(QSize(20,20));
        for(int i=0;i<tabs->count();++i){if(tabs->tabIcon(i).isNull())tabs->setTabIcon(i,glyph(tabGlyph(tabs->tabText(i))));if(tabs->tabToolTip(i).isEmpty())tabs->setTabToolTip(i,hint(tabs->tabText(i)));}
    }else if(auto* menu=qobject_cast<QMenu*>(widget)){
        menu->setToolTipsVisible(true);
        for(auto* action:menu->actions()){if(action->isSeparator()||action->text().isEmpty())continue;if(action->icon().isNull())action->setIcon(glyph(tabGlyph(action->text())));if(action->toolTip().isEmpty()||action->toolTip()==plainTitle(action->text()))action->setToolTip(hint(action->text()));}
    }
}
inline void decorateTree(QWidget* root) {decorate(root);for(auto* child:root->findChildren<QWidget*>())decorate(child);}
class VisualFilter final : public QObject {
public:
    explicit VisualFilter(QObject* parent):QObject(parent){}
    bool eventFilter(QObject* object,QEvent* event) override {
        if(event->type()==QEvent::Show){if(auto* widget=qobject_cast<QWidget*>(object)){if(widget->isWindow())decorateTree(widget);else decorate(widget);}}
        return false;
    }
};
inline void installVisualFilter() {
    if(!qApp||qApp->property("sysPulseVisualFilterInstalled").toBool())return;
    qApp->setProperty("sysPulseVisualFilterInstalled",true);qApp->installEventFilter(new VisualFilter(qApp));
}
class Backdrop final : public QWidget {
    QPixmap image;
    bool artwork=true;
public:
    explicit Backdrop(QWidget* parent=nullptr):QWidget(parent),image(640,360) {
        setObjectName("fluentBackdrop");
        setAttribute(Qt::WA_OpaquePaintEvent);
        QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient base(0,0,640,360);base.setColorAt(0,QColor("#F1F5FB"));base.setColorAt(1,QColor("#E6EEF8"));p.fillRect(image.rect(),base);
        QRadialGradient glow(QPointF(570,40),450);glow.setColorAt(0,QColor(150,189,234,105));glow.setColorAt(1,QColor(150,189,234,0));p.fillRect(image.rect(),glow);
        QPainterPath ribbon;ribbon.moveTo(640,4);ribbon.cubicTo(480,5,555,220,320,245);ribbon.cubicTo(180,260,125,335,0,360);ribbon.lineTo(640,360);ribbon.closeSubpath();
        QLinearGradient silk(280,160,640,360);silk.setColorAt(0,QColor(255,255,255,20));silk.setColorAt(1,QColor(155,194,232,88));p.fillPath(ribbon,silk);
        QPainterPath lower;lower.moveTo(640,150);lower.cubicTo(505,120,510,300,285,335);lower.lineTo(185,360);lower.lineTo(640,360);lower.closeSubpath();p.fillPath(lower,QColor(255,255,255,105));
    }
    void setArtwork(bool enabled) {artwork=enabled;update();}
    bool artworkEnabled() const {return artwork;}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::SmoothPixmapTransform);if(artwork)p.drawPixmap(rect(),image);else p.fillRect(rect(),QColor("#F4F6FA"));
    }
};
}
