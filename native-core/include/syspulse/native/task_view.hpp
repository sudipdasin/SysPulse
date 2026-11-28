// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/native/runtime.hpp"
namespace syspulse::native {
enum class TaskSort { Pid, Cpu, Memory, Name };
struct TaskRow {Process process;std::optional<double> cpuPercent;std::optional<std::uint64_t> rssBytes;};
struct TaskView {std::vector<TaskRow> rows;bool partial{};};
TaskView buildTaskView(const Inspection& before,const Inspection& after,double seconds,std::uint64_t ticksPerSecond,std::uint64_t pageBytes);
std::vector<TaskRow> filterTasks(const TaskView&,std::string_view search,TaskSort sort);
std::optional<Process> retainTaskSelection(const TaskView&,const std::optional<Process>& selected);
}
