// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/trusted_command.hpp"
#include <QApplication>
#include <QClipboard>
#include <QHash>
#include <QMessageBox>
#include <QProcess>
namespace syspulse {
inline QString dependencyPackage(const QString& command) {
    // Fixed distribution package names only; never derive an install target from user text.
    static const QHash<QString,QString> packages={
        {"df","coreutils"},
        {"uname","coreutils"},
        {"uptime","procps"},
        {"lsblk","util-linux"},
        {"findmnt","util-linux"},
        {"lscpu","util-linux"},
        {"fstrim","util-linux"},
        {"zramctl","util-linux"},
        {"systemctl","systemd"},
        {"journalctl","systemd"},
        {"loginctl","systemd"},
        {"systemd-analyze","systemd"},
        {"apt","apt"},
        {"apt-get","apt"},
        {"apt-cache","apt"},
        {"apt-mark","apt"},
        {"dpkg","dpkg"},
        {"dpkg-query","dpkg"},
        {"free","procps"},
        {"tlp-stat","tlp"},
        {"xfconf-query","xfconf"},
        {"xinput","xinput"},
        {"wine","wine"},
        {"lutris","lutris"},
        {"vulkaninfo","vulkan-tools"},
        {"ip","iproute2"},
        {"ss","iproute2"},
        {"ufw","ufw"},
        {"lspci","pciutils"},
        {"lsusb","usbutils"},
        {"xrandr","x11-xserver-utils"},
        {"glxinfo","mesa-utils"},
        {"nmcli","network-manager"},
        {"pkexec","pkexec"},
        {"xfce4-display-settings","xfce4-settings"},
        {"xfce4-keyboard-settings","xfce4-settings"},
        {"xfce4-mouse-settings","xfce4-settings"},
        {"xfce4-appearance-settings","xfce4-settings"},
        {"xfce4-accessibility-settings","xfce4-settings"},
        {"xfce4-settings-manager","xfce4-settings"},
        {"xfce4-power-manager-settings","xfce4-power-manager"},
        {"xfce4-panel","xfce4-panel"},
        {"xfce4-session-settings","xfce4-session"},
        {"xfwm4-settings","xfwm4"},
        {"xfwm4-tweaks-settings","xfwm4"},
        {"xfdesktop-settings","xfdesktop4"},
        {"xfce4-notifyd-config","xfce4-notifyd"},
        {"thunar","thunar"},
        {"thunar-settings","thunar"},
        {"gnome-disks","gnome-disk-utility"},
        {"gparted","gparted"},
        {"baobab","baobab"},
        {"bleachbit","bleachbit"},
        {"gufw","gufw"},
        {"blueman-manager","blueman"},
        {"nm-connection-editor","network-manager-gnome"},
        {"system-config-printer","system-config-printer"},
        {"pavucontrol","pavucontrol"},
        {"gcm-viewer","gnome-color-manager"},
        {"users-admin","gnome-system-tools"},
        {"lightdm-settings","lightdm-settings"},
        {"seahorse","seahorse"},
        {"timeshift-gtk","timeshift"},
        {"deja-dup","deja-dup"},
        {"gnome-logs","gnome-logs"},
        {"synaptic","synaptic"},
        {"mintupdate","mintupdate"},
        {"mintinstall","mintinstall"},
        {"mintsources","mintsources"},
    };
    return packages.value(command);
}
inline QString dependencyHint(const QString& command) {
    const auto package=dependencyPackage(command);
    return package.isEmpty()?QString("No reviewed package mapping. Check your distribution's software manager."):
        QString("Distribution package: %1 (availability depends on release and enabled repositories).").arg(package);
}
inline void reviewDependency(QWidget* parent,const QString& command) {
    const auto package=dependencyPackage(command);
    if(package.isEmpty()||!tux::trustedSystemExecutable(command).isEmpty())return;
    const auto installer=tux::trustedSystemExecutable("apturl");
    QMessageBox box(QMessageBox::Question,"Review missing dependency",
        QString("The selected tool requires package %1. Use only your distribution's configured repositories. Review dependencies, disk space and authorization in the package manager.").arg(package),
        QMessageBox::Cancel,parent);
    box.setTextFormat(Qt::PlainText);
    auto* copy=box.addButton("Copy package name",QMessageBox::ActionRole);
    auto* open=installer.isEmpty()?nullptr:box.addButton("Open package installer",QMessageBox::AcceptRole);
    box.setDefaultButton(QMessageBox::Cancel);box.setEscapeButton(QMessageBox::Cancel);box.exec();
    if(box.clickedButton()==copy){QApplication::clipboard()->setText(package);return;}
    if(open&&box.clickedButton()==open){
        if(tux::trustedSystemExecutable("apturl")!=installer||!QProcess::startDetached(installer,QStringList{"apt:"+package}))
            QMessageBox::warning(parent,"Package installer unavailable","Open the distribution software manager manually and search for "+package+".");
    }
}
}
