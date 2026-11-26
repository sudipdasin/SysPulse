#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
namespace sp {
struct Browser { std::uint64_t count=0, rss=0, unreadable=0; bool partial=false; };
inline bool browserName(const std::string& s) { return s=="brave" || s=="brave-browser" || s=="brave-browser-s"; }
inline void addBrowser(Browser& b, std::istream& comm, std::istream& status) {
    std::string name; if(!std::getline(comm,name)) { ++b.unreadable; return; }
    if(!browserName(name)) return;
    ++b.count; std::string line;
    while(std::getline(status,line)) {
        if(line.compare(0,6,"VmRSS:")!=0) continue;
        std::istringstream in(line.substr(6)); std::uint64_t value=0; std::string unit,extra;
        in>>std::ws;
        if(in.peek()>='0' && in.peek()<='9' && in>>value>>unit && unit=="kB" && !(in>>extra) && value<=UINT64_MAX-b.rss) { b.rss+=value; return; }
        break;
    }
    ++b.unreadable;
}
inline Browser browserSnapshot(const std::filesystem::path& proc="/proc") {
    Browser b; std::error_code ec;
    auto end=std::filesystem::directory_iterator();
    auto it=std::filesystem::directory_iterator(proc,ec);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(250);
    std::size_t seen=0;
    for(; !ec && it!=end; it.increment(ec)) {
        if(++seen>8192 || std::chrono::steady_clock::now()>deadline) { b.partial=true; break; }
        auto name=it->path().filename().string();
        if(name.empty() || name.find_first_not_of("0123456789")!=std::string::npos) continue;
        std::ifstream comm(it->path()/"comm"); std::ifstream status(it->path()/"status");
        addBrowser(b,comm,status);
    }
    if(ec) b.partial=true;
    return b;
}
inline std::vector<std::string> browserRows(const Browser& b) {
    return {"Matching Brave processes: "+std::to_string(b.count),"Aggregate RSS: "+std::to_string(b.rss/1024)+" MiB (shared pages may be counted twice)","Unreadable or incomplete process records: "+std::to_string(b.unreadable),b.partial?"Partial scan: deadline, entry cap or directory error":"Scan completed; process visibility depends on permissions"};
}
inline bool commandPresent(const std::string& name) {
    for(const auto* dir:{"/usr/bin/","/usr/sbin/","/bin/","/sbin/","/usr/local/bin/","/usr/local/sbin/"}) {
        const auto path=std::string(dir)+name;
        std::error_code ec; if(std::filesystem::is_regular_file(path,ec) && access(path.c_str(),X_OK)==0) return true;
    }
    return false;
}
inline std::vector<std::string> capabilityRows() {
    std::vector<std::string> rows;
    for(const auto& group:std::array<std::array<const char*,4>,5>{{{{"systemctl","journalctl","lsblk","findmnt"}},{{"smartctl","hdparm","fstrim","e2fsck"}},{{"zramctl","tlp","xfconf-query","xinput"}},{{"wine","lutris","vulkaninfo","brave-browser"}},{{"apt-get","flatpak","pkexec","openssl"}}}}) {
        std::string row; for(const auto* cmd:group) { if(!row.empty()) row+=" | "; row+=std::string(cmd)+(commandPresent(cmd)?": yes":": no"); } rows.push_back(row);
    }
    return rows;
}
}
