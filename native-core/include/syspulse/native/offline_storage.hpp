// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
#include <string>
#include <vector>
namespace syspulse::native {
struct OfflineDevice {
    std::string path,uuid,type,majorMinor,size;
    bool operator==(const OfflineDevice& b)const{return path==b.path&&uuid==b.uuid&&type==b.type&&majorMinor==b.majorMinor&&size==b.size;}
};
bool offlinePath(const std::string& path);
bool offlineObject(const std::string& path);
std::optional<std::vector<OfflineDevice>> offlineDevices(const std::string& json);
std::optional<std::string> offlineObjectInfo(const std::string& info,const OfflineDevice& selected);
int offlineReply(const std::string& output);
}
