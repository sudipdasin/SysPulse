// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "syspulse/native/runtime.hpp"
namespace syspulse::native {
enum class DeviceKind { Disk, Network };
struct DeviceCounter { DeviceKind kind; std::string name, identity; std::uint64_t read{}, write{}; };
struct DeviceSample { Sample::Clock::time_point time; std::vector<DeviceCounter> counters; bool truncated{}; };
struct DeviceRate { DeviceKind kind; std::string name; double readBytesPerSecond{}, writeBytesPerSecond{}; };
DeviceSample captureDevices();
std::optional<std::vector<DeviceRate>> deviceRates(const DeviceSample& before,const DeviceSample& after);
}
