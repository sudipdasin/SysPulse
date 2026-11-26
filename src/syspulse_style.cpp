// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/window.hpp"
#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
namespace syspulse {
QString precisionStyle() {
    return QStringLiteral(R"QSS(
QMainWindow,QDialog { background:#F4F6FA; color:#142336; }
QWidget { font-family:"DejaVu Sans","Sans Serif"; font-size:13px; color:#142336; }
QWidget#sidebar { background:#F9FAFC; border-right:1px solid #DBE2EB; }
QWidget#utilityBar { background:#FFFFFF; border-bottom:1px solid #DBE2EB; }
QLabel { background:transparent; }
QLabel#brand { font-size:29px; font-weight:700; color:#142336; }
QLabel#brandEdition,QLabel#muted,QLabel#intro,QLabel#fileStatus { color:#506079; }
QLabel#navGroup { color:#506079; font-size:11px; font-weight:700; padding:9px 6px 2px; }
QLabel#pageTitle { font-size:29px; font-weight:700; color:#142336; }
QLabel#sectionTitle { font-size:17px; font-weight:700; }
QLabel#metricValue { font-size:27px; font-weight:700; }
QLabel#statusFooter { background:#FFFFFF; color:#506079; font-size:11px; padding:7px 14px; border-top:1px solid #DBE2EB; }
QLabel#breadcrumb { color:#506079; font-size:13px; font-weight:600; }
QLabel#analysisStatus { color:#506079; }
QFrame#panel,QWidget#panel,QFrame#metricCard { background:#FFFFFF; border:1px solid #DBE2EB; border-radius:12px; }
QFrame#analysisHero { background:#FFFFFF; border:1px solid #DBE2EB; border-top:4px solid #B51235; border-radius:10px; }
QPushButton,QToolButton { background:#FFFFFF; border:1px solid #CBD5E1; border-radius:6px; padding:8px 12px; min-height:18px; }
QPushButton:hover,QToolButton:hover { background:#F1F3F7; border-color:#98A2B3; }
QPushButton:pressed { background:#E9ECF2; }
QPushButton:focus,QToolButton:focus,QLineEdit:focus,QComboBox:focus { border:2px solid #B51235; }
QPushButton:disabled { background:#F1F2F4; color:#9198A4; border-color:#E2E5EA; }
QPushButton[primary="true"],QPushButton#primaryAction { background:#B51235; border-color:#B51235; color:white; font-weight:600; }
QPushButton[primary="true"]:hover,QPushButton#primaryAction:hover { background:#95102F; }
QPushButton#dangerAction { color:#A80D27; border:1px solid #DCA1AC; }
QPushButton#navButton { background:transparent; border:0; text-align:left; padding:8px 11px; min-height:16px; font-size:13px; }
QPushButton#navButton:hover { background:#F1F4F8; }
QPushButton#navButton:checked { background:#FCE9ED; color:#A31130; font-weight:700; border-left:3px solid #B51235; padding-left:8px; }
QPushButton#navButton:focus { border:1px solid #B51235; }
QPushButton#metricLink { text-align:left; border:0; background:transparent; font-size:12px; color:#506079; }
QLineEdit,QComboBox,QSpinBox { background:#FFFFFF; border:1px solid #CBD5E1; border-radius:6px; padding:7px 9px; min-height:18px; selection-background-color:#F6C5CF; selection-color:#142336; }
QComboBox::drop-down { border:0; width:24px; }
QTableView,QTreeView,QListView,QPlainTextEdit,QTextBrowser { background:#FFFFFF; alternate-background-color:#F8FAFC; border:1px solid #DBE2EB; border-radius:6px; selection-background-color:#FCE9ED; selection-color:#142336; gridline-color:#E3E6EC; }
QTableView::item,QTreeView::item { padding:7px 5px; border:0; }
QTableView::item:selected,QTreeView::item:selected { background:#FCE9ED; color:#142336; }
QHeaderView::section { background:#F1F4F8; color:#333844; font-weight:600; padding:9px 7px; border:0; border-bottom:1px solid #DBE2EB; }
QPlainTextEdit { padding:8px; font-family:"DejaVu Sans Mono",monospace; font-size:12px; }
QTabWidget::pane { border:0; background:transparent; }
QTabBar::tab { background:#FFFFFF; color:#475467; border:1px solid #DBE2EB; border-radius:5px; padding:8px 12px; margin:0 4px 6px 0; }
QTabBar::tab:selected { background:#FCE9ED; border-color:#D898A6; color:#A31130; font-weight:700; }
QTabBar::tab:hover:!selected { background:#FCE9ED; }
QTabBar::tab:focus { border:1px solid #B51235; }
QScrollArea { border:0; background:transparent; }
QScrollBar:vertical { background:#F1F3F6; width:10px; margin:0; }
QScrollBar::handle:vertical { background:#B9C0CD; border-radius:5px; min-height:25px; }
QScrollBar:horizontal { background:#F1F3F6; height:10px; margin:0; }
QScrollBar::handle:horizontal { background:#B9C0CD; border-radius:5px; min-width:25px; }
QScrollBar::add-line,QScrollBar::sub-line { height:0; width:0; }
QSplitter::handle { background:#E7EAF0; }
QSplitter::handle:horizontal { width:3px; }
QSplitter::handle:vertical { height:3px; }
QProgressBar { border:1px solid #DBE2EB; border-radius:5px; background:#EEF0F4; text-align:center; }
QProgressBar::chunk { background:#B51235; border-radius:4px; }
QCheckBox { spacing:7px; }
QCheckBox::indicator { width:16px; height:16px; }
QToolTip { color:#FFFFFF; background:#15283E; border:0; padding:6px; }
QMenu { background:#FFFFFF; border:1px solid #DBE2EB; padding:5px; }
QMenu::item { padding:7px 20px; }
QMenu::item:selected { background:#FCE9ED; }

/* Fluent-inspired light treatment. Alpha surfaces reveal the local cached backdrop, not other windows. */
QWidget#fluentBody,QStackedWidget#sysPulsePages { background:transparent; }
QWidget#sidebar { background:rgba(248,250,253,205); border-right:1px solid #D8E1EC; }
QWidget#utilityBar { background:rgba(255,255,255,210); border-bottom:1px solid #DCE4EE; }
QLabel#brand { font-size:27px; font-weight:700; }
QLabel#navGroup { color:#526981; font-size:11px; padding:8px 10px 3px; }
QPushButton#navButton { border:1px solid transparent; border-radius:6px; padding:6px 10px; min-height:20px; icon-size:20px; }
QPushButton#navButton:hover { background:rgba(255,255,255,220); border:1px solid #DDE5EF; }
QPushButton#navButton:checked { background:rgba(255,255,255,235); color:#A31130; border:1px solid #DCE4EE; border-left:3px solid #B51235; padding-left:8px; }
QPushButton#navButton:focus { border:1px solid #B51235; }
QFrame#panel,QWidget#panel,QFrame#metricCard { background:rgba(255,255,255,235); border:1px solid #DCE4EF; border-radius:10px; }
QFrame#analysisHero { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #FFFFFF,stop:1 #ECF4FC); border:1px solid #D5E3F2; border-left:4px solid #B51235; border-radius:12px; }
QPushButton,QToolButton { border:1px solid #CBD6E2; border-bottom:2px solid #BFCBDA; border-radius:6px; }
QPushButton:hover,QToolButton:hover { background:#F1F6FC; border-color:#9EB4CA; }
QPushButton[primary="true"],QPushButton#primaryAction { background:#B51235; border:1px solid #98102D; color:white; }
QPushButton[primary="true"]:hover,QPushButton#primaryAction:hover { background:#95102F; }
QPushButton:disabled,QToolButton:disabled { background:#F1F2F4; color:#747D89; border:1px solid #E2E5EA; }
QPushButton:focus,QToolButton:focus { border:2px solid #B51235; }
QLineEdit { border-bottom:2px solid #B7C5D5; }
QLineEdit:focus { border:1px solid #C0CDDC; border-bottom:2px solid #B51235; }
QTabBar::tab { border-radius:6px; background:rgba(255,255,255,190); border:1px solid #DCE4EE; padding:7px 10px; }
QTabBar::tab:selected { background:#FFFFFF; border:1px solid #DCE4EE; border-bottom:3px solid #B51235; color:#A31130; }
QTabBar::tab:focus { border:1px solid #B51235; }
QMenu { background:#FFFFFF; border:1px solid #D1DBE7; border-radius:8px; padding:6px; }
QMenu::item { padding:8px 25px 8px 32px; border-radius:5px; }
QMenu::icon { left:9px; }
QMenu::item:selected { background:#EAF1F9; color:#142336; }
QLabel#statusFooter { background:rgba(255,255,255,218); border-top:1px solid #DCE4EE; }

)QSS");
}
void applyPrecisionTheme() {
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window,QColor("#F4F6FA"));
    palette.setColor(QPalette::WindowText,QColor("#142336"));
    palette.setColor(QPalette::Base,Qt::white);
    palette.setColor(QPalette::AlternateBase,QColor("#F8FAFC"));
    palette.setColor(QPalette::Text,QColor("#142336"));
    palette.setColor(QPalette::Button,Qt::white);
    palette.setColor(QPalette::ButtonText,QColor("#142336"));
    palette.setColor(QPalette::Mid,QColor("#DBE2EB"));
    palette.setColor(QPalette::Highlight,QColor("#B51235"));
    palette.setColor(QPalette::HighlightedText,Qt::white);
    palette.setColor(QPalette::Link,QColor("#A31130"));
    palette.setColor(QPalette::Disabled,QPalette::Text,QColor("#9198A4"));
    palette.setColor(QPalette::Disabled,QPalette::ButtonText,QColor("#9198A4"));
    QApplication::setPalette(palette);
    qApp->setStyleSheet(precisionStyle());
}
}
