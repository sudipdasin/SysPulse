#include "tux/core.hpp"
#include "tux/admin_session.hpp"
#include "tux/trusted_command.hpp"
#include "syspulse/system_memory.hpp"
#include <QJsonArray>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <stdexcept>

namespace tux {
const QVector<Action>& actions() {
    static const QVector<Action> catalog = {
        {"storage", "Storage", "Storage usage", "Review mounted filesystems and free space.", {"df", "-hT"}, false, false, false},
        {"devices", "Storage", "Disks & filesystems", "Inspect devices, rotation type, mount points and filesystem types.", {"lsblk", "-o", "NAME,SIZE,ROTA,FSTYPE,MOUNTPOINTS"}, false, false, false},
        {"mounts", "Storage", "Mount map", "Inspect loop devices and filesystem mounts before planning repairs.", {"findmnt"}, false, false, false},
        {"trim-check", "Storage", "SSD trim schedule", "Inspect the system SSD trim timer.", {"systemctl", "status", "fstrim.timer", "--no-pager"}, false, false, false},
        {"trim", "Storage", "Trim SSDs", "Reclaim unused blocks on supported mounted SSD filesystems. Requires administrator authentication.", {"fstrim", "--all", "--verbose"}, true, true, false},
        {"failed", "Diagnostics", "Failed services", "Inspect failed system services.", {"systemctl", "--failed", "--no-pager"}, false, false, false},
        {"kernel", "Diagnostics", "Kernel warnings", "Read the latest 150 kernel warnings from this boot. Access depends on local journal permissions.", {"journalctl", "-k", "-b", "-p", "warning", "-n", "150", "--no-pager"}, false, false, false},
        {"journal", "Diagnostics", "System warnings", "Read the latest 200 warnings and errors.", {"journalctl", "-b", "-p", "warning", "-n", "200", "--no-pager"}, false, false, false},
        {"journal-size", "Maintenance", "Journal disk usage", "Measure journal storage before deciding whether to prune.", {"journalctl", "--disk-usage"}, false, false, false},
        {"apt-preview", "Maintenance", "Preview unused packages", "Simulate APT autoremove. No packages are removed.", {"apt-get", "--simulate", "autoremove"}, false, false, false},
        {"apt-clean", "Maintenance", "Clean downloaded packages", "Delete cached APT package downloads. Installed applications remain available.", {"apt-get", "clean"}, true, true, false},
        {"journal-prune", "Maintenance", "Retain seven days of logs", "Permanently remove archived journal files older than seven days. Export needed diagnostics first.", {"journalctl", "--vacuum-time=7d"}, true, true, false},
        {"timers", "Schedules", "Scheduled system tasks", "Review timers including trim and package maintenance.", {"systemctl", "list-timers", "--all", "--no-pager"}, false, false, false},
        {"user-timers", "Schedules", "Your scheduled tasks", "Inspect timers in the current user session.", {"systemctl", "--user", "list-timers", "--all", "--no-pager"}, false, false, false},
        {"trim-enable", "Schedules", "Enable weekly SSD trim", "Enable and start the distribution-provided fstrim timer. Undo: use Disable weekly SSD trim", {"systemctl", "enable", "--now", "fstrim.timer"}, true, true, false},
        {"trim-disable", "Schedules", "Disable weekly SSD trim", "Disable and stop the distribution-provided fstrim timer. This stops scheduled maintenance; use Enable weekly SSD trim to restore it. Manual TRIM remains available.", {"systemctl", "disable", "--now", "fstrim.timer"}, true, true, false},
        {"memory", "Performance", "Memory & swap", "Inspect RAM and swap usage before tuning.", {"free", "-h"}, false, false, false},
        {"zram", "Performance", "Compressed swap status", "Inspect active zRAM devices.", {"zramctl"}, false, false, false},
        {"zswap-enable", "Performance", "Enable system-wide memory compression", "Enable kernel zswap for future swap writes during this boot. Requires existing ordinary swap, supported kernel and no zram devices. May trade CPU time for less disk I/O.", {"syspulse-memory-control", "enable"}, true, true, false},
        {"zswap-disable", "Performance", "Disable new zswap compression", "Stop admitting new pages to zswap. Does not disable swap or immediately empty existing compressed entries. Runtime only.", {"syspulse-memory-control", "disable"}, true, true, false},
        {"power", "Performance", "Power management status", "Inspect TLP if installed. No packages are installed automatically.", {"tlp-stat", "-s"}, false, false, false},
        {"compositor-on", "Performance", "Enable XFCE compositor", "Enable desktop compositing. Reversible using Disable compositor.", {"xfconf-query", "-c", "xfwm4", "-p", "/general/use_compositing", "-s", "true"}, true, false, false},
        {"compositor-off", "Performance", "Disable XFCE compositor", "May reduce rendering overhead; can cause tearing. Reversible using Enable compositor.", {"xfconf-query", "-c", "xfwm4", "-p", "/general/use_compositing", "-s", "false"}, true, false, false},
        {"input", "Input", "List X11 input devices", "Inspect exact device IDs. X11 only; permanent keyboard disabling is intentionally deferred.", {"xinput", "list"}, false, false, true},
        {"wine", "Gaming", "Wine version", "Check whether Wine is installed.", {"wine", "--version"}, false, false, false},
        {"lutris", "Gaming", "Lutris version", "Check whether Lutris is installed.", {"lutris", "--version"}, false, false, false},
        {"vulkan", "Gaming", "Vulkan capabilities", "Inspect graphics driver support.", {"vulkaninfo", "--summary"}, false, false, false},
        {"arch", "Gaming", "32-bit architectures", "Inspect enabled foreign package architectures.", {"dpkg", "--print-foreign-architectures"}, false, false, false},
        {"network-links", "Network", "Network links", "Inspect link state and capabilities. Details stay local unless explicitly exported.", {"ip", "-brief", "link"}, false, false, false},
        {"network-addresses", "Network", "Network addresses", "Inspect local interface addresses. Review private details before sharing.", {"ip", "-brief", "address"}, false, false, false},
        {"network-routes", "Network", "Network routes", "Inspect the local route table without changing network configuration.", {"ip", "route", "show"}, false, false, false},
        {"firewall-status", "Security", "Firewall status", "Read UFW status and rules with administrator authorization. No rules are changed.", {"ufw", "status", "verbose"}, false, true, false},
        {"available-updates", "Security", "Available package updates", "List updates from the existing APT cache. Does not contact repositories or install packages.", {"apt", "list", "--upgradable"}, false, false, false},
        {"package-audit", "Maintenance", "Package health", "Inspect incomplete package installations without modifying packages.", {"dpkg", "--audit"}, false, false, false},
        {"desktop-channels", "Desktop", "XFCE configuration channels", "List available XFCE configuration channels without changing settings.", {"xfconf-query", "--list"}, false, false, false},
        {"filesystem-config", "Storage", "Filesystem configuration check", "Read and verify the mount configuration. Does not repair or remount filesystems.", {"findmnt", "--verify", "--verbose"}, false, false, false},
        {"session-state", "Desktop", "Desktop session state", "Inspect the current login session. Output may contain private local details.", {"loginctl", "session-status", "--no-pager"}, false, false, false},
        {"boot-time", "Diagnostics", "Boot timing", "Inspect measured system boot timing when systemd provides it.", {"systemd-analyze", "time"}, false, false, false},
        {"cpu-topology", "Performance", "CPU topology", "Inspect CPU architecture, cores and virtualization flags locally.", {"lscpu"}, false, false, false},
        {"kernel-version", "Diagnostics", "Kernel and architecture", "Inspect kernel release and architecture without hostname.", {"uname", "-srmo"}, false, false, false},
        {"uptime-summary", "Diagnostics", "System uptime", "Inspect how long the system has been running.", {"uptime", "-p"}, false, false, false},
        {"systemd-state", "Diagnostics", "System manager state", "Inspect systemd health. A degraded or unavailable state may return a nonzero exit code.", {"systemctl", "is-system-running"}, false, false, false},
        {"boot-history", "Diagnostics", "Boot journal inventory", "List available journal boots. Boot identifiers remain local unless explicitly exported.", {"journalctl", "--list-boots", "--no-pager"}, false, false, false},
        {"discard-support", "Storage", "Device discard capabilities", "Inspect discard limits; does not issue TRIM.", {"lsblk", "--discard"}, false, false, false},
        {"disk-transport", "Storage", "Disk transport and rotation", "Inspect device transport and rotational flags; no disk changes.", {"lsblk", "-o", "NAME,TYPE,SIZE,ROTA,TRAN"}, false, false, false},
        {"package-holds", "Maintenance", "Held packages", "List APT package holds without changing them.", {"apt-mark", "showhold"}, false, false, false},
        {"package-origins", "Security", "Cached package origins", "Inspect local APT policy and repository origins; no network request.", {"apt-cache", "policy"}, false, false, false},
        {"native-architecture", "Gaming", "Native package architecture", "Inspect the distribution's native package architecture.", {"dpkg", "--print-architecture"}, false, false, false},
        {"pci-inventory", "Desktop", "PCI hardware inventory", "Inspect PCI hardware identifiers locally.", {"lspci", "-nn"}, false, false, false},
        {"usb-inventory", "Desktop", "USB hardware inventory", "Inspect USB device types and identifiers locally.", {"lsusb"}, false, false, false},
        {"display-modes", "Desktop", "X11 display modes", "Query connected displays and modes; does not change resolution.", {"xrandr", "--query"}, false, false, true},
        {"compositor-state", "Desktop", "XFCE compositor status", "Read the current XFCE window compositor setting; no modification.", {"xfconf-query", "-c", "xfwm4", "-p", "/general/use_compositing"}, false, false, false},
        {"graphics-renderer", "Gaming", "OpenGL renderer information", "Query OpenGL renderer details on X11 when the diagnostic is installed.", {"glxinfo", "-B"}, false, false, true},
        {"link-counters", "Network", "Network interface counters", "Inspect interface packet and byte counters; addresses remain local.", {"ip", "-s", "link"}, false, false, false},
        {"socket-summary", "Network", "Socket statistics", "Inspect aggregate local socket statistics without resolving remote names.", {"ss", "-s"}, false, false, false},
        {"network-manager-state", "Network", "NetworkManager state", "Read NetworkManager connection and radio state.", {"nmcli", "general", "status"}, false, false, false},
        {"system-timer-files", "Schedules", "System timer unit inventory", "Inspect installed system timer unit files without enabling them.", {"systemctl", "list-unit-files", "--type=timer", "--no-pager"}, false, false, false},
        {"user-timer-files", "Schedules", "User timer unit inventory", "Inspect installed user timer unit files without enabling them.", {"systemctl", "--user", "list-unit-files", "--type=timer", "--no-pager"}, false, false, false},
    };
    return catalog;
}
const Action& action(const QString& id) {
    for (const auto& tool : actions()) if (tool.id==id) return tool;
    throw std::invalid_argument("Unknown tool identifier");
}
QString unavailable(const Action& tool) {
    if (tool.x11 && qEnvironmentVariable("XDG_SESSION_TYPE")=="wayland")
        return "Requires X11; XWayland cannot control native Wayland devices.";
    const bool fixedPath=tool.admin||tool.changes||tool.id=="available-updates"||tool.id=="lutris";
    if(!tool.admin&&!tool.changes&&fixedPath&&trustedSystemExecutable(tool.command.first()).isEmpty()&&!QStandardPaths::findExecutable(tool.command.first()).isEmpty())
        return "Untrusted executable: "+tool.command.first()+"; use a verified distribution installation.";
    if ((fixedPath?trustedSystemExecutable(tool.command.first()):QStandardPaths::findExecutable(tool.command.first())).isEmpty())
        return "Missing dependency: "+tool.command.first();
    if (tool.admin && trustedSystemExecutable("pkexec").isEmpty())
        return "Missing dependency: pkexec (PolicyKit authentication agent required)";
    if(tool.id=="zswap-enable"&&!syspulse::system_memory::eligible())
        return "Requires disabled zswap, active ordinary swap, and verified absence of zram devices.";
    if(tool.id=="zswap-disable"&&syspulse::system_memory::readEnabled()!=syspulse::system_memory::State::Enabled)
        return "Kernel zswap is not verified enabled.";
    return {};
}
bool matches(const Action& tool, const QString& query) {
    const auto text=tool.title+" "+tool.group+" "+tool.description;
    for (const auto& word:query.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts))
        if (!text.contains(word,Qt::CaseInsensitive)) return false;
    return true;
}
QString preview(const QStringList& command) {
    QStringList quoted;
    for (auto arg:command) {
        arg.replace("'", "'\\''");
        quoted.append("'"+arg+"'");
    }
    return quoted.join(' '); // Display only. Never evaluated by a shell.
}
Result execute(const QString& id, bool confirmed, int timeoutMs, Output output, const std::atomic_bool* cancel) {
    const auto& tool=action(id);
    if ((tool.changes || tool.admin) && !confirmed) throw std::runtime_error("Review and confirm this system action first.");
    const auto problem=unavailable(tool);
    if (!problem.isEmpty()) throw std::runtime_error(problem.toStdString());
    if(tool.admin){if(auto session=admin::execute(id)){if(output)output(session->output);return *session;}}
    auto command=tool.command;
    if (tool.admin||tool.changes||tool.id=="available-updates"||tool.id=="lutris") {
        const auto executable=trustedSystemExecutable(command.first());
        if(executable.isEmpty())throw std::runtime_error("A trusted system executable is no longer available.");
        command[0]=executable;
        if(tool.admin){
            const auto authorization=trustedSystemExecutable("pkexec");
            if(authorization.isEmpty())throw std::runtime_error("Administrator authorization is no longer available.");
            command.prepend(authorization);
        }
    }
    // Cancellation is deliberately unavailable to settings-changing/admin tasks.
    auto result=runCommand(command,timeoutMs,std::move(output),diagnosticCancellation(tool,cancel));
    result.action=tool.title;
    // Do not block read-only health workflows based on a host filesystem
    // assumption. Explain missing sysfs only after a real command failure.
    if(tool.command.first()=="lsblk"&&result.code!=0&&!result.cancelled&&result.code!=124&&
       (!QFileInfo("/sys/dev/block").isDir()||!QFileInfo("/sys/dev/block").isReadable())){
        const QByteArray explanation="\nBlock inventory may be unavailable because /sys/dev/block is not readable. Retry on the desktop or a VM exposing block devices.\n";
        if(result.output.size()+explanation.size()>OutputLimit){result.output=result.output.right(OutputLimit-explanation.size());result.truncated=true;}
        result.output.append(explanation);
    }
    return result;
}
QJsonObject Result::json() const {
    return {{"action",action},{"command",QJsonArray::fromStringList(command)},
            {"code",code},{"output",QString::fromUtf8(output)},
            {"truncated",truncated},{"seconds",seconds},{"cancelled",cancelled}};
}
QJsonObject sessionReport(const QVector<Result>& results, bool includeDetails) {
    QJsonArray records;
    for (const auto& result:results) {
        if (includeDetails) { records.append(result.json()); continue; }
        QString title="Unrecognized task";
        for (const auto& tool:actions()) if (tool.title==result.action) { title=tool.title; break; }
        records.append(QJsonObject{{"action",title},{"code",result.code},
            {"seconds",result.seconds},{"truncated",result.truncated},{"cancelled",result.cancelled}});
    }
    return {{"version",Version},{"schema",1},{"mode",includeDetails?"detailed":"summary"},
        {"results",records}};
}
}
