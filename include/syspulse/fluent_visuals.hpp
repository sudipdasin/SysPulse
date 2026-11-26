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
namespace syspulse::fluent {
inline QIcon glyph(const QString& id) {
    QPixmap pix(48,48); pix.fill(Qt::transparent); pix.setDevicePixelRatio(2);
    QPainter p(&pix); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#46647F"),1.65,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
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
    else if(id=="back"){path({{14,5},{7,12},{14,19}});line(7,12,21,12);}
    else if(id=="forward"){path({{10,5},{17,12},{10,19}});line(3,12,17,12);}
    else if(id=="search"){circle(10,10,6);line(14.5,14.5,20,20);}
    else if(id=="help"){circle(12,12,9);p.drawArc(QRectF(9,6,6,6),0,180*16);path({{15,9},{15,11},{12,13},{12,15}});circle(12,18,.45);}
    else {rect(4,4,16,16);line(8,9,16,9);line(8,14,16,14);}
    p.end(); return QIcon(pix);
}
inline QString tabGlyph(QString title) {
    title=title.toLower();
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
