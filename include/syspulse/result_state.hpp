// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
namespace syspulse {
inline QString resultState(const tux::Result& result) {
    if(result.cancelled)return "Cancelled";
    if(result.code==124)return "Timed out — inspect state";
    if(result.action==tux::action("systemd-state").title&&result.code==1&&result.output.trimmed()=="degraded")return "System degraded — review failed units";
    if(result.code!=0)return QString("Needs attention (%1)").arg(result.code);
    for(const auto& a:tux::actions())if(a.title==result.action&&a.changes)return "Command completed — verify change";
    if(result.truncated)return "Incomplete output — review";
    return "Completed";
}
}
