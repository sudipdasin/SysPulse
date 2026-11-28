// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/maintenance.hpp"
#include "syspulse/native/offline_storage.hpp"
#include <iostream>
#include <thread>
#include <cstdlib>
using namespace syspulse::native;
namespace {int count=0;void check(bool b,const char* s){++count;if(!b){std::cerr<<s<<'\n';std::exit(1);}}
Result run(Operation op){Job job;auto c=commandFor(op);check(c&&job.start(*c),"start fixed read-only command");for(int i=0;i<1000;++i){if(auto r=job.poll())return *r;std::this_thread::sleep_for(std::chrono::milliseconds(10));}std::exit(2);}
std::string entry(const std::string& path="/dev/sda1",const std::string& id="01234567-89ab-cdef-0123-456789abcdef",const std::string& mounted="[null]",const std::string& ro="false",const std::string& dev="8:1",const std::string& fs="ext4"){return "{\"path\":\""+path+"\",\"uuid\":\""+id+"\",\"fstype\":\""+fs+"\",\"maj:min\":\""+dev+"\",\"type\":\"part\",\"ro\":"+ro+",\"mountpoints\":"+mounted+",\"size\":\"10G\"}";}
std::string wrap(const std::string& rows){return "{\"blockdevices\":["+rows+"]}\n";}}
int main(){
    auto c=commandFor(Operation::ListSystemServices);check(c&&c->args.front()=="--system"&&!c->admin,"system list");
    c=commandFor(Operation::StartSystemService,"example.service");check(c&&c->admin&&c->args.back()=="example.service","system start");
    c=commandFor(Operation::RestartSystemService,"example.service");check(c&&c->args[c->args.size()-2]=="restart","system restart");
    c=commandFor(Operation::StopSystemService,"example.service");check(c&&c->admin,"system stop");
    c=commandFor(Operation::RestartUserService,"example.service");check(c&&!c->admin&&c->args.front()=="--user","user restart");
    check(!commandFor(Operation::StopSystemService,"NetworkManager.service"),"network protected");check(!commandFor(Operation::RestartSystemService,"systemd-logind.service"),"logind protected");
    check(!commandFor(Operation::StopSystemService,"dbus.service"),"bus protected");check(!commandFor(Operation::StopSystemService,"--bad.service"),"options rejected");
    check(!commandFor(Operation::RestartSystemService,"foo;bar.service"),"shell metacharacters rejected");
    c=commandFor(Operation::WifiDisable);check(c&&c->args==std::vector<std::string>{"radio","wifi","off"},"wifi off exact");
    c=commandFor(Operation::WifiEnable);check(c&&c->args.back()=="on","wifi on exact");
    c=commandFor(Operation::PowerSaver);check(c&&c->args==std::vector<std::string>{"set","power-saver"},"power saver fixed");
    c=commandFor(Operation::PowerPerformance);check(c&&c->args.back()=="performance","performance fixed");
    c=commandFor(Operation::FirewallStatus);check(c&&c->admin&&c->args.back()=="verbose","firewall read only");
    check(!commandFor(Operation::WifiEnable,"eth0"),"extra argument rejected");
    Job j;check(!j.start({"/usr/bin/pkexec",{"/usr/bin/systemctl","--system","stop","dbus.service"},true}),"protected bypass rejected");
    check(!j.start({"/usr/bin/ip",{"link","delete","eth0"},false}),"network mutation injection rejected");
    check(!j.start({"/usr/bin/nmcli",{"connection","delete","example"},false}),"arbitrary network operation rejected");
    check(!j.start({"/usr/bin/powerprofilesctl",{"set","arbitrary"},false}),"unknown profile rejected");
    check(!j.start({"/usr/bin/pkexec",{"/usr/sbin/ufw","disable"},true}),"firewall disable rejected");
    auto a=offlineDevices(wrap(entry()));check(a&&a->size()==1&&a->front().path=="/dev/sda1","eligible ext4");auto selected=a->front();
    check(offlineDevices(wrap(entry("/dev/sda1","01234567-89ab-cdef-0123-456789abcdef","[]")))->size()==1,"empty mount list");
    check(offlineDevices(wrap(entry("/dev/sda1","01234567-89ab-cdef-0123-456789abcdef","[\"/\"]")))->empty(),"mounted rejected");
    check(offlineDevices(wrap(entry("/dev/sda1","01234567-89ab-cdef-0123-456789abcdef","[null]","true")))->empty(),"read-only rejected");
    check(offlineDevices(wrap(entry("/dev/../sda1")))->empty(),"path traversal rejected");
    check(offlineDevices(wrap(entry("/dev/sda1","garbage")))->empty(),"unknown UUID rejected");
    check(offlineDevices(wrap(entry()+","+entry("/dev/sdb1","01234567-89AB-cdef-0123-456789abcdef","[\"/mnt\"]","false","8:17")))->empty(),"duplicate UUID even mounted rejected");
    check(offlineDevices(wrap(entry()+","+entry("/dev/sdb1","11234567-89ab-cdef-0123-456789abcdef")))->empty(),"duplicate device number rejected");
    check(!offlineDevices("{\"blockdevices\":42}"),"malformed inventory");check(!offlineDevices(std::string(65537,'x')),"bounded inventory");
    check(offlineDevices(wrap(entry("/dev/sda1","01234567-89ab-cdef-0123-456789abcdef","[null]","false","8:1","ntfs")))->empty(),"unsupported filesystem rejected");
    const std::string object="/org/freedesktop/UDisks2/block_devices/sda1";
    const std::string info=object+":\n  org.freedesktop.UDisks2.Block:\n    Device: /dev/sda1\n    IdUUID: 01234567-89ab-cdef-0123-456789abcdef\n    IdType: ext4\n  org.freedesktop.UDisks2.Filesystem:\n    MountPoints:\n";
    check(offlineObjectInfo(info,selected)==object,"identity verified");auto changed=selected;changed.uuid="11234567-89ab-cdef-0123-456789abcdef";check(!offlineObjectInfo(info,changed),"changed UUID rejected");changed=selected;changed.path="/dev/sdb1";check(!offlineObjectInfo(info,changed),"changed path rejected");
    check(!offlineObjectInfo(object+":\n",selected),"object alone insufficient");check(!offlineObject(object+"/extra"),"object path scope");check(!offlinePath("/dev/../sda1"),"device scope");
    check(offlineReply("(true,)\n")==1&&offlineReply("(false,)\n")==0&&offlineReply("failure")==-1,"exact reply");
    c=commandFor(Operation::OfflineCheck,object);check(c&&c->args[7]=="org.freedesktop.UDisks2.Filesystem.Check","check exact method");
    c=commandFor(Operation::OfflineRepair,object);check(c&&c->args[7]=="org.freedesktop.UDisks2.Filesystem.Repair","repair exact method");
    check(!commandFor(Operation::OfflineRepair,"/other"),"invalid object");check(!commandFor(Operation::OfflineInfo,"/dev/../sda"),"invalid block path");
    check(!j.start({"/usr/bin/gdbus",{"call","--system","--dest","org.freedesktop.UDisks2","--object-path",object,"--method","org.freedesktop.UDisks2.Filesystem.Format","--timeout","1800","{}"},false}),"format injection rejected");
    auto r=run(Operation::NetworkStatus);check(r.code==0&&!r.output.empty()&&!r.truncated,"actual network address inventory");
    r=run(Operation::NetworkRoutes);check(r.code==0&&!r.truncated,"actual route inventory");
    r=run(Operation::SocketStatus);check(r.code==0&&!r.truncated,"actual socket inventory");
    r=run(Operation::OfflineInventory);check(r.code==0&&!r.truncated&&offlineDevices(r.output).has_value(),"actual block inventory parse");
    std::cout<<count<<" administration and offline storage checks passed\n";
}
