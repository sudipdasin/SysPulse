// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include "tux/trusted_command.hpp"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QStandardPaths>
namespace syspulse::diagnostic {
inline const QSet<QString>& allowedStatuses(){static const QSet<QString> s={"PASS",
        "DISCREPANCY",
        "FAILED",
        "MISSING",
        "INCOMPLETE",
        "TIMEOUT",
        "REVIEW",
        "PREFLIGHT_ONLY",
        "BLOCKED_BY_DESIGN",
        "NOT_IMPLEMENTED",
        "NOT_TESTED",
        "UNAVAILABLE",
        "CANCELLED"};return s;}
inline const QSet<QString>& allowedKeys(){static const QSet<QString> s={
        "preflight/storage",
        "preflight/devices",
        "preflight/mounts",
        "preflight/trim-check",
        "preflight/trim",
        "preflight/failed",
        "preflight/kernel",
        "preflight/journal",
        "preflight/journal-size",
        "preflight/apt-preview",
        "preflight/apt-clean",
        "preflight/journal-prune",
        "preflight/timers",
        "preflight/user-timers",
        "preflight/trim-enable",
        "preflight/memory",
        "preflight/zram",
        "preflight/zswap-enable",
        "preflight/zswap-disable",
        "preflight/power",
        "preflight/compositor-on",
        "preflight/compositor-off",
        "preflight/input",
        "preflight/wine",
        "preflight/lutris",
        "preflight/vulkan",
        "preflight/arch",
        "preflight/network-links",
        "preflight/network-addresses",
        "preflight/network-routes",
        "preflight/firewall-status",
        "preflight/available-updates",
        "preflight/package-audit",
        "preflight/desktop-channels",
        "preflight/filesystem-config",
        "preflight/session-state",
        "preflight/boot-time",
        "preflight/cpu-topology",
        "preflight/kernel-version",
        "preflight/uptime-summary",
        "preflight/systemd-state",
        "preflight/boot-history",
        "preflight/discard-support",
        "preflight/disk-transport",
        "preflight/package-holds",
        "preflight/package-origins",
        "preflight/native-architecture",
        "preflight/pci-inventory",
        "preflight/usb-inventory",
        "preflight/display-modes",
        "preflight/compositor-state",
        "preflight/graphics-renderer",
        "preflight/link-counters",
        "preflight/socket-summary",
        "preflight/network-manager-state",
        "preflight/system-timer-files",
        "preflight/user-timer-files",
        "run/storage",
        "run/devices",
        "run/mounts",
        "run/trim-check",
        "run/trim",
        "run/failed",
        "run/kernel",
        "run/journal",
        "run/journal-size",
        "run/apt-preview",
        "run/apt-clean",
        "run/journal-prune",
        "run/timers",
        "run/user-timers",
        "run/trim-enable",
        "run/memory",
        "run/zram",
        "run/zswap-enable",
        "run/zswap-disable",
        "run/power",
        "run/compositor-on",
        "run/compositor-off",
        "run/input",
        "run/wine",
        "run/lutris",
        "run/vulkan",
        "run/arch",
        "run/network-links",
        "run/network-addresses",
        "run/network-routes",
        "run/firewall-status",
        "run/available-updates",
        "run/package-audit",
        "run/desktop-channels",
        "run/filesystem-config",
        "run/session-state",
        "run/boot-time",
        "run/cpu-topology",
        "run/kernel-version",
        "run/uptime-summary",
        "run/systemd-state",
        "run/boot-history",
        "run/discard-support",
        "run/disk-transport",
        "run/package-holds",
        "run/package-origins",
        "run/native-architecture",
        "run/pci-inventory",
        "run/usb-inventory",
        "run/display-modes",
        "run/compositor-state",
        "run/graphics-renderer",
        "run/link-counters",
        "run/socket-summary",
        "run/network-manager-state",
        "run/system-timer-files",
        "run/user-timer-files",
        "audit/catalog/storage",
        "audit/catalog/devices",
        "audit/catalog/mounts",
        "audit/catalog/trim-check",
        "audit/catalog/trim",
        "audit/catalog/failed",
        "audit/catalog/kernel",
        "audit/catalog/journal",
        "audit/catalog/journal-size",
        "audit/catalog/apt-preview",
        "audit/catalog/apt-clean",
        "audit/catalog/journal-prune",
        "audit/catalog/timers",
        "audit/catalog/user-timers",
        "audit/catalog/trim-enable",
        "audit/catalog/memory",
        "audit/catalog/zram",
        "audit/catalog/zswap-enable",
        "audit/catalog/zswap-disable",
        "audit/catalog/power",
        "audit/catalog/compositor-on",
        "audit/catalog/compositor-off",
        "audit/catalog/input",
        "audit/catalog/wine",
        "audit/catalog/lutris",
        "audit/catalog/vulkan",
        "audit/catalog/arch",
        "audit/catalog/network-links",
        "audit/catalog/network-addresses",
        "audit/catalog/network-routes",
        "audit/catalog/firewall-status",
        "audit/catalog/available-updates",
        "audit/catalog/package-audit",
        "audit/catalog/desktop-channels",
        "audit/catalog/filesystem-config",
        "audit/catalog/session-state",
        "audit/catalog/boot-time",
        "audit/catalog/cpu-topology",
        "audit/catalog/kernel-version",
        "audit/catalog/uptime-summary",
        "audit/catalog/systemd-state",
        "audit/catalog/boot-history",
        "audit/catalog/discard-support",
        "audit/catalog/disk-transport",
        "audit/catalog/package-holds",
        "audit/catalog/package-origins",
        "audit/catalog/native-architecture",
        "audit/catalog/pci-inventory",
        "audit/catalog/usb-inventory",
        "audit/catalog/display-modes",
        "audit/catalog/compositor-state",
        "audit/catalog/graphics-renderer",
        "audit/catalog/link-counters",
        "audit/catalog/socket-summary",
        "audit/catalog/network-manager-state",
        "audit/catalog/system-timer-files",
        "audit/catalog/user-timer-files",
        "audit/launcher/Storage/Disk tools/gnome-disks",
        "audit/launcher/Storage/Disk tools/gparted",
        "audit/launcher/Storage/Disk tools/baobab",
        "audit/launcher/Storage/Disk tools/thunar",
        "audit/launcher/Storage/Disk tools/bleachbit",
        "audit/launcher/Security/Software and security/mintupdate",
        "audit/launcher/Security/Software and security/mintinstall",
        "audit/launcher/Security/Software and security/synaptic",
        "audit/launcher/Security/Software and security/mintsources",
        "audit/launcher/Security/Software and security/gufw",
        "audit/launcher/Security/Software and security/gnome-logs",
        "audit/launcher/Network/Network tools/nm-connection-editor",
        "audit/launcher/Network/Network tools/gufw",
        "audit/launcher/Network/Network tools/blueman-manager",
        "audit/launcher/Desktop/Desktop settings/xfce4-display-settings",
        "audit/launcher/Desktop/Desktop settings/xfce4-keyboard-settings",
        "audit/launcher/Desktop/Desktop settings/xfce4-mouse-settings",
        "audit/launcher/Desktop/Desktop settings/xfce4-power-manager-settings",
        "audit/launcher/Desktop/Desktop settings/xfce4-panel",
        "audit/launcher/Desktop/Desktop settings/xfce4-session-settings",
        "audit/launcher/Desktop/Desktop settings/xfce4-appearance-settings",
        "audit/launcher/Desktop/Desktop settings/xfwm4-settings",
        "audit/launcher/Desktop/Administration/xfce4-settings-manager",
        "audit/launcher/Desktop/Administration/xfdesktop-settings",
        "audit/launcher/Desktop/Administration/driver-manager",
        "audit/launcher/Desktop/Administration/users-admin",
        "audit/launcher/Desktop/Administration/mintusers",
        "audit/launcher/Desktop/Administration/lightdm-settings",
        "audit/launcher/Desktop/Administration/system-config-printer",
        "audit/launcher/Desktop/Administration/hardinfo",
        "audit/launcher/Desktop/Administration/seahorse",
        "audit/launcher/Recovery/timeshift-gtk",
        "audit/launcher/Recovery/qphotorec",
        "audit/launcher/Recovery/deja-dup",
        "audit/launcher/Control Panel/Desktop/xfce4-appearance-settings",
        "audit/launcher/Control Panel/Desktop/xfwm4-settings",
        "audit/launcher/Control Panel/Desktop/xfwm4-tweaks-settings",
        "audit/launcher/Control Panel/Desktop/xfdesktop-settings",
        "audit/launcher/Control Panel/Desktop/xfce4-display-settings",
        "audit/launcher/Control Panel/Desktop/xfce4-keyboard-settings",
        "audit/launcher/Control Panel/Desktop/xfce4-mouse-settings",
        "audit/launcher/Control Panel/Desktop/xfce4-power-manager-settings",
        "audit/launcher/Control Panel/Desktop/xfce4-session-settings",
        "audit/launcher/Control Panel/Desktop/exo-preferred-applications",
        "audit/launcher/Control Panel/Desktop/xfce4-accessibility-settings",
        "audit/launcher/Control Panel/Desktop/xfce4-notifyd-config",
        "audit/launcher/Control Panel/Desktop/thunar-settings",
        "audit/launcher/Control Panel/Hardware/driver-manager",
        "audit/launcher/Control Panel/Hardware/system-config-printer",
        "audit/launcher/Control Panel/Hardware/pavucontrol",
        "audit/launcher/Control Panel/Hardware/blueman-manager",
        "audit/launcher/Control Panel/Hardware/hardinfo",
        "audit/launcher/Control Panel/Hardware/gcm-viewer",
        "audit/launcher/Control Panel/Accounts/users-admin",
        "audit/launcher/Control Panel/Accounts/mintusers",
        "audit/launcher/Control Panel/Accounts/lightdm-settings",
        "audit/launcher/Control Panel/Accounts/seahorse",
        "audit/launcher/Control Panel/Storage/gnome-disks",
        "audit/launcher/Control Panel/Storage/gparted",
        "audit/launcher/Control Panel/Storage/baobab",
        "audit/launcher/Control Panel/Storage/bleachbit",
        "audit/launcher/Control Panel/Storage/timeshift-gtk",
        "audit/launcher/Control Panel/Storage/deja-dup",
        "audit/launcher/Control Panel/Storage/qphotorec",
        "audit/launcher/Control Panel/Network and security/nm-connection-editor",
        "audit/launcher/Control Panel/Network and security/gufw",
        "audit/launcher/Control Panel/Network and security/gnome-logs",
        "audit/launcher/Control Panel/Software/mintupdate",
        "audit/launcher/Control Panel/Software/mintinstall",
        "audit/launcher/Control Panel/Software/synaptic",
        "audit/launcher/Control Panel/Software/mintsources",
        "audit/Cleaner/home",
        "audit/Cleaner/quick-cache",
        "audit/Cleaner/standard-cleanup-cache",
        "audit/Cleaner/quick-scan-prediction",
        "audit/Cleaner/deletion-eligibility",
        "audit/Cleaner/bounded-enumeration",
        "audit/installation/syspulse",
        "audit/installation/tux-steward",
        "audit/installation/tux-task-manager",
        "audit/installation/tux-setup",
        "audit/installation/syspulse-memory-control",
        "audit/installation/pkexec",
        "audit/installation/apturl",
        "audit/installation/reference-version",
        "audit/installation/package-integrity",
        "audit/hardware/storage-class",
        "audit/resource-monitor//proc/stat",
        "audit/resource-monitor//proc/meminfo",
        "audit/resource-monitor//proc/diskstats",
        "audit/resource-monitor//proc/net/dev",
        "audit/resource-monitor//proc/vmstat",
        "audit/resource-monitor//proc/swaps",
        "audit/resource-monitor//proc/pressure/cpu",
        "audit/resource-monitor//proc/pressure/memory",
        "audit/resource-monitor//proc/pressure/io",
        "audit/resource-monitor//sys/dev/block",
        "audit/resource-monitor//sys/block",
        "audit/resource-monitor//sys/class/net",
        "audit/Task Manager/process-stat",
        "audit/Startup/system-autostart",
        "audit/Startup/user-autostart",
        "audit/updates/production-signed-feed",
        "audit/desktop/real-GUI-interaction",
        "audit/recovery/destructive-operations",
        "audit/coverage/planned-native-capabilities"
    };return s;}
inline bool allowed(const QString& key,const QString& value){return allowedKeys().contains(key)&&allowedStatuses().contains(value);}
inline bool mediumKey(const QString& key){return key.startsWith("preflight/")||key.startsWith("run/")||key.startsWith("audit/catalog/");}
inline const char* auditReference(){return "3beb151833b9faf2ec9266e67b0123f2cab91f5d";}
inline QMap<QString,QString> importAudit(const QByteArray& data,bool& valid){
    valid=false;QMap<QString,QString> result;
    if(data.size()>512*1024)return result;
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(data,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return result;
    const auto root=doc.object();const auto facts=root.value("facts").toObject();
    if(facts.value("reference_commit").toString()!=auditReference()||facts.value("audit_version").toString()!="2.0.0"||!root.value("results").isArray())return result;
    const auto records=root.value("results").toArray();if(records.size()>512)return result;
    for(const auto& v:records){
        const auto r=v.toObject();
        const auto key=QString("audit/")+r.value("section").toString()+"/"+r.value("id").toString();
        const auto state=r.value("status").toString();
        if(!allowed(key,state))continue;
        if(result.contains(key)){result.clear();return result;} // reject ambiguous duplicate evidence
        result.insert(key,state);
    }
    valid=!result.isEmpty();return result;
}
inline QString sessionState(const tux::Result& r,const tux::Action& a){
    if(r.cancelled)return "CANCELLED";
    if(r.code==124)return "TIMEOUT";
    if(r.code!=0)return "FAILED";
    if(r.truncated)return "INCOMPLETE";
    return a.changes?"REVIEW":"PASS";
}
inline QMap<QString,QString> snapshot(const QVector<tux::Result>& results,const QMap<QString,QString>& audit){
    QMap<QString,QString> checks;
    for(auto i=audit.cbegin();i!=audit.cend();++i)if(i.key().startsWith("audit/")&&allowed(i.key(),i.value()))checks.insert(i.key(),i.value());
    for(const auto& a:tux::actions()){
        const auto problem=tux::unavailable(a);
        const auto state=problem.isEmpty()?"PREFLIGHT_ONLY":problem.startsWith("Missing dependency")?"MISSING":"BLOCKED_BY_DESIGN";
        const auto key="preflight/"+a.id;if(allowed(key,state))checks.insert(key,state);
        for(const auto& r:results)if(r.action==a.title){const auto rk="run/"+a.id;const auto rs=sessionState(r,a);if(allowed(rk,rs))checks.insert(rk,rs);}
    }
    return checks;
}
inline QJsonObject payload(const QMap<QString,QString>& checks,int level){
    if(level<1||level>3)return {};
    QJsonObject counts,details;
    for(auto i=checks.cbegin();i!=checks.cend();++i){
        if(!allowed(i.key(),i.value()))continue;
        counts.insert(i.value(),counts.value(i.value()).toInt()+1);
        if(level==3||(level==2&&mediumKey(i.key())))details.insert(i.key(),i.value());
    }
    QJsonObject obj{{"schema",1},{"release",QString(tux::Version)},{"level",level==1?"min":level==2?"med":"max"},{"counts",counts}};
    if(level>=2)obj.insert("checks",details);
    return obj;
}
inline QString advice(const QMap<QString,QString>& checks){
    int discrepancies=0,missing=0,failed=0;
    for(const auto& state:checks){discrepancies+=state=="DISCREPANCY";missing+=state=="MISSING";failed+=state=="FAILED"||state=="TIMEOUT"||state=="INCOMPLETE";}
    QString out=QString("Local analysis: %1 discrepancy, %2 missing prerequisite, %3 failed/incomplete check records. Records can overlap; not a feature success percentage.").arg(discrepancies).arg(missing).arg(failed);
    if(checks.value("audit/Cleaner/quick-cache")=="DISCREPANCY")out+="\nCleaner: redirected cache detected. Inspect only; do not bypass deletion guards.";
    if(missing)out+="\nReview dependency guidance and refresh availability.";
    if(discrepancies||failed)out+="\nPrioritize reproducing failed checks; verify outcomes before any system change.";
    out+="\nNo setting changes, installation, deletion or automatic updates are performed.";
    return out;
}
}
