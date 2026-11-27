// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include <optional>
namespace tux::admin {
// The GUI never receives a password. pkexec authenticates a private pipe broker.
bool unlock(QString& error);
bool lock();
bool active();
bool busy();
// Nullopt only when no session was active before dispatch; never replay uncertain actions.
std::optional<Result> execute(const QString& id);
std::optional<Result> executeTyped(const QStringList& request,int limit);
void beginDispatch();
void endDispatch();
}
