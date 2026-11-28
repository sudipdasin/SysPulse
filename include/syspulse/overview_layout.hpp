// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <initializer_list>

namespace syspulse {
// Six actions form complete rows at every breakpoint. Use measured Qt size hints,
// not fixed screen resolutions; account for the gaps between adjacent buttons.
constexpr int overviewActionColumns(int available, int buttonWidth, int gap) {
    if (available <= 0 || buttonWidth <= 0 || gap < 0) return 1;
    for (int columns : {6, 3, 2}) {
        if (static_cast<long long>(columns) * buttonWidth
            + static_cast<long long>(columns - 1) * gap <= available)
            return columns;
    }
    return 1;
}
}
