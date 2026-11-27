#include "syspulse/offline_filesystem.hpp"
#include <iostream>
int main(){
    using namespace syspulse::offline;
    const QByteArray fixture=R"({"blockdevices":[
      {"path":"/dev/loop0","uuid":"11111111-1111-1111-1111-111111111111","fstype":"ext4","maj:min":"7:0","type":"loop","ro":false,"mountpoints":[null],"size":"10G"},
      {"path":"/dev/sda1","uuid":"22222222-2222-2222-2222-222222222222","fstype":"ext4","maj:min":"8:1","type":"part","ro":false,"mountpoints":["/"],"size":"30G"},
      {"path":"/dev/sda2","uuid":"33333333-3333-3333-3333-333333333333","fstype":"ext4","maj:min":"8:2","type":"part","ro":true,"mountpoints":[null],"size":"30G"},
      {"path":"/dev/sdb1","uuid":"44444444-4444-4444-4444-444444444444","fstype":"ntfs","maj:min":"8:17","type":"part","ro":false,"mountpoints":[],"size":"20G"},
      {"path":"/dev/sdc1","uuid":"55555555-5555-5555-5555-555555555555","fstype":"ext4","maj:min":"8:33","type":"part","ro":false,"mountpoints":[],"size":"20G"},
      {"path":"/dev/sdd1","uuid":"55555555-5555-5555-5555-555555555555","fstype":"ext4","maj:min":"8:49","type":"part","ro":false,"mountpoints":[],"size":"20G"}]})";
    const auto found=candidates(fixture);
    if(found.size()!=1||found[0].path!="/dev/loop0"||!candidates("broken").isEmpty())return 1;
    const auto object=objectPath("/org/freedesktop/UDisks2/block_devices/loop0:\n  x");
    if(object!="/org/freedesktop/UDisks2/block_devices/loop0"||!objectPath("/unexpected").isEmpty())return 2;
    if(!arguments("/bad",true).isEmpty()||!arguments(object,true).contains("org.freedesktop.UDisks2.Filesystem.Repair")||!arguments(object,false).contains("org.freedesktop.UDisks2.Filesystem.Check"))return 3;
    if(boolReply("(true,)\n")!=1||boolReply("(false,)")!=0||boolReply("error")!=-1)return 4;
    std::cout<<"Offline inventory, mount/RO/duplicate guards, exact UDisks commands and reply parsing passed\n";
}
