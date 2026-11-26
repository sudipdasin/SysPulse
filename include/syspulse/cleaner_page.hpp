// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <functional>
#include <QString>
class QWidget;
namespace syspulse {
QWidget* cleanerPage(std::function<void(const QString&)> route);
}
