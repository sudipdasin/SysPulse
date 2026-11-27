// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <istream>
#include <string>
namespace syspulse {
enum class LineState { Line, End, Invalid };
// Read at most Limit characters before rejecting an oversized or binary line.
// A final line without a newline is valid; an incomplete or bad stream is not.
template<std::size_t Limit>
LineState boundedLine(std::istream& input, std::string& line) {
    static_assert(Limit > 0 && Limit <= 65536);
    std::array<char, Limit + 1> buffer{};
    input.getline(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = input.gcount();
    line.clear();
    if (input.bad() || (input.fail() && !input.eof())) return LineState::Invalid;
    if (input.eof() && count == 0) return LineState::End;
    const auto length = static_cast<std::size_t>(count - (input.eof() ? 0 : 1));
    if (length > Limit) return LineState::Invalid;
    for (std::size_t i = 0; i < length; ++i) if (buffer[i] == '\0') return LineState::Invalid;
    line.assign(buffer.data(), length);
    return LineState::Line;
}
}
