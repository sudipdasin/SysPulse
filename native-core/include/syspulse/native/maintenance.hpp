// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
#include <string>
#include <vector>
namespace syspulse::native {
enum class Operation { Audit, PreviewClean, Refresh, Clean, ListServices, StartService, StopService, CheckPackages, ConfigurePackages, PreviewUpgrade, ApplyUpgrade, PreviewRepair, ApplyRepair, DiskInventory, FailedServices, PreviewTrim, TrimRoot, EnableCompression, DisableCompression, ListSystemServices, StartSystemService, StopSystemService, RestartSystemService, RestartUserService, NetworkStatus, NetworkRoutes, SocketStatus, WifiStatus, WifiEnable, WifiDisable, PowerStatus, PowerBalanced, PowerSaver, PowerPerformance, JournalErrors, KernelErrors, FirewallStatus, TimerStatus, OfflineInventory, OfflineInfo, OfflineCheck, OfflineRepair, DiagnoseJournal };
struct Command { std::string exe; std::vector<std::string> args; bool admin{}; };
std::optional<Command> commandFor(Operation, const std::string& unit={});
struct Service { std::string name,state; };
std::vector<Service> parseServices(const std::string&);
struct Result { int code{-1}; bool truncated{}; std::string output; };
class Job { int pid_=-1, fd_=-1; bool exited_=false; Result result_; public: Job()=default; ~Job(); Job(const Job&)=delete; Job& operator=(const Job&)=delete; bool start(const Command&); bool active()const{return pid_>0;} std::optional<Result> poll(); };
}
