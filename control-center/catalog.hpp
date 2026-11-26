#pragma once
#include <array>
#include <string>
#include <vector>
#include <unistd.h>
namespace sp {
struct Tool { const char* group; const char* name; const char* detail; const char* commands; };
inline const std::array<const char*,13> groups={"Overview","Desktop","Storage","Recovery","Hardware","Accounts","Printing","Performance","Software","Network","Display","Logs","Backups"};
inline const std::vector<Tool> tools={
{"Desktop","Settings manager","Open the desktop's own settings hub","xfce4-settings-manager|gnome-control-center|systemsettings"},
{"Desktop","Appearance","Themes, fonts and icons","xfce4-appearance-settings"},
{"Desktop","Keyboard","Keyboard layout and shortcuts","xfce4-keyboard-settings"},
{"Desktop","Mouse and touchpad","Pointer preferences","xfce4-mouse-settings"},
{"Desktop","Window manager","Window behavior and compositor","xfwm4-settings"},
{"Desktop","Session and startup","Session and autostart preferences","xfce4-session-settings"},
{"Desktop","Desktop preferences","Wallpaper and desktop behavior","xfdesktop-settings"},
{"Desktop","Power manager","Power and sleep preferences","xfce4-power-manager-settings"},
{"Storage","Disk management","Inspect disks, partitions and SMART","gnome-disks"},
{"Storage","Partition editor","Partition changes can destroy data","gparted"},
{"Storage","Disk usage analyzer","Find large files and directories","baobab|qdirstat"},
{"Storage","File manager","Browse files and mounted drives","thunar|nautilus|dolphin"},
{"Recovery","Data recovery","Recover files to a different drive","qphotorec"},
{"Recovery","Disk health and imaging","Inspect disk health and create images","gnome-disks"},
{"Hardware","Driver manager","Distribution-supported driver selection","driver-manager|software-properties-gtk"},
{"Hardware","Hardware information","Inspect hardware and devices","hardinfo2|hardinfo"},
{"Hardware","Bluetooth","Bluetooth devices and pairing","blueman-manager"},
{"Accounts","Users and groups","Manage local user accounts and groups","users-admin|mintusers"},
{"Accounts","Login window","Configure supported login manager","lightdm-settings|mdmsetup"},
{"Accounts","Password and keys","Manage local keyrings","seahorse"},
{"Printing","Printers","Manage printers and print queues","system-config-printer"},
{"Performance","Task manager","Monitor and manage processes","xfce4-taskmanager|gnome-system-monitor|ksysguard"},
{"Performance","Power settings","Review performance and power tradeoffs","xfce4-power-manager-settings"},
{"Performance","Storage cleanup","Review reclaimable files before deleting","bleachbit"},
{"Performance","Memory and swap","Read-only system diagnostics","@health"},
{"Software","Update manager","Distribution-supported software updates","mintupdate|update-manager"},
{"Software","Software manager","Browse and manage applications","mintinstall|gnome-software|plasma-discover"},
{"Software","Package manager","Inspect and manage packages","synaptic"},
{"Software","Software sources","Review configured software repositories","mintsources|software-properties-gtk"},
{"Software","Refresh APT metadata","Administrator prompt; no package upgrade","@apt"},
{"Software","SysPulse updates","Open existing signed-update controls","@health"},
{"Network","Network connections","Configure network connections","nm-connection-editor"},
{"Network","Firewall","Review and configure firewall policy","gufw|firewall-config"},
{"Network","Network usage","Monitor system network activity","gnome-system-monitor"},
{"Display","Display settings","Resolution, layout and refresh rate","xfce4-display-settings"},
{"Display","Color management","Display color profiles","gcm-viewer"},
{"Logs","System logs","View local system logs; review before sharing","gnome-logs|ksystemlog"},
{"Logs","System information","Inspect OS, memory and storage","@health"},
{"Backups","Timeshift","Create and restore system snapshots","timeshift-gtk"},
{"Backups","File backups","Personal files require separate backup","deja-dup|mintbackup"}
};
inline std::string findCommand(const std::string& alternatives) {
    std::size_t pos=0;
    while(pos<alternatives.size()) {
        auto end=alternatives.find('|',pos); auto name=alternatives.substr(pos,end-pos);
        if(!name.empty() && name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")==std::string::npos) {
            for(const char* prefix:{"/usr/bin/","/usr/sbin/","/bin/","/sbin/"}) {
                std::string path=std::string(prefix)+name;
                if(access(path.c_str(),X_OK)==0) return path;
            }
        }
        if(end==std::string::npos) break;
        pos=end+1;
    }
    return {};
}
}
