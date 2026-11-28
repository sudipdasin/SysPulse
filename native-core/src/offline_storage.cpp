// SPDX-License-Identifier: GPL-3.0-or-later
// Port of the original SysPulse offline_filesystem.hpp contract; native JSON parser.
#include "syspulse/native/offline_storage.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <json-c/json.h>
namespace syspulse::native {
namespace {
bool match(const std::string& s,const char* expression){return s.size()<=256&&std::regex_match(s,std::regex(expression));}
std::string lower(std::string s){for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");if(a==s.npos)return {};return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
json_object* get(json_object* obj,const char* key){json_object* p=nullptr;return json_object_object_get_ex(obj,key,&p)?p:nullptr;}
std::string field(json_object* obj,const char* key){auto* p=get(obj,key);if(!p||json_object_get_type(p)!=json_type_string)return {};auto n=json_object_get_string_len(p);if(n>256)return {};std::string s(json_object_get_string(p),static_cast<std::size_t>(n));return s.find('\0')==s.npos?s:std::string{};}
bool uuid(const std::string& s){return match(s,"[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}");}
}
bool offlinePath(const std::string& p){return match(p,"/dev/[A-Za-z0-9][A-Za-z0-9._+-]{0,100}");}
bool offlineObject(const std::string& p){return match(p,"/org/freedesktop/UDisks2/block_devices/[A-Za-z0-9_]{1,160}");}
std::optional<std::vector<OfflineDevice>> offlineDevices(const std::string& data){
    if(data.empty()||data.size()>65536||data.find('\0')!=data.npos)return {};
    std::unique_ptr<json_tokener,decltype(&json_tokener_free)> tok(json_tokener_new_ex(16),json_tokener_free);if(!tok)return {};
    json_tokener_set_flags(tok.get(),JSON_TOKENER_STRICT|JSON_TOKENER_VALIDATE_UTF8);
    std::unique_ptr<json_object,decltype(&json_object_put)> root(json_tokener_parse_ex(tok.get(),data.data(),static_cast<int>(data.size())),json_object_put);
    if(!root||json_tokener_get_error(tok.get())!=json_tokener_success||json_tokener_get_parse_end(tok.get())!=data.size()||json_object_get_type(root.get())!=json_type_object)return {};
    auto* rows=get(root.get(),"blockdevices");if(!rows||json_object_get_type(rows)!=json_type_array||json_object_array_length(rows)>1024)return {};
    std::map<std::string,unsigned> ids,paths,devnos;std::vector<OfflineDevice> result;
    for(std::size_t i=0;i<json_object_array_length(rows);++i){auto* r=json_object_array_get_idx(rows,i);if(!r||json_object_get_type(r)!=json_type_object)return {};
        auto name=field(r,"path"),id=lower(field(r,"uuid")),fs=field(r,"fstype"),number=field(r,"maj:min"),kind=field(r,"type"),size=field(r,"size");
        if(offlinePath(name))++paths[name];
        if(uuid(id))++ids[id];
        if(match(number,"[0-9]{1,10}:[0-9]{1,10}"))++devnos[number];
        auto* ro=get(r,"ro");auto* mounts=get(r,"mountpoints");
        if(!offlinePath(name)||!uuid(id)||(fs!="ext2"&&fs!="ext3"&&fs!="ext4")||!match(number,"[0-9]{1,10}:[0-9]{1,10}")||(kind!="part"&&kind!="disk"&&kind!="loop")||!match(size,"[0-9]+([.,][0-9]+)?[BKMGTPE]?"))continue;
        if(!ro||json_object_get_type(ro)!=json_type_boolean||json_object_get_boolean(ro)||!mounts||json_object_get_type(mounts)!=json_type_array)continue;
        bool mounted=false;for(std::size_t j=0;j<json_object_array_length(mounts);++j)if(json_object_array_get_idx(mounts,j)!=nullptr)mounted=true;
        if(!mounted)result.push_back({name,id,fs,number,size});
    }
    result.erase(std::remove_if(result.begin(),result.end(),[&](const auto& d){return paths[d.path]!=1||ids[d.uuid]!=1||devnos[d.majorMinor]!=1;}),result.end());
    return result;
}
std::optional<std::string> offlineObjectInfo(const std::string& info,const OfflineDevice& d){
    if(info.empty()||info.size()>65536||info.find('\0')!=info.npos)return {};
    std::istringstream in(info);std::string line;if(!std::getline(in,line))return {};auto object=trim(line);if(!object.empty()&&object.back()==':')object.pop_back();if(!offlineObject(object))return {};
    std::string path,uuidValue,type;bool block=false;
    while(std::getline(in,line)){if(line.size()>4096)return {};line=trim(line);if(line.rfind("org.freedesktop.UDisks2.",0)==0){block=line=="org.freedesktop.UDisks2.Block:";continue;}if(!block)continue;
        auto set=[&](const char* key,std::string& value){std::string k(key);if(line.rfind(k,0)!=0)return true;if(!value.empty())return false;value=trim(line.substr(k.size()));return !value.empty();};
        if(!set("Device:",path)||!set("IdUUID:",uuidValue)||!set("IdType:",type))return {};
    }
    if(path!=d.path||lower(uuidValue)!=d.uuid||type!=d.type)return {};
    return object;
}
int offlineReply(const std::string& s){const auto v=trim(s);return v=="(true,)"?1:v=="(false,)"?0:-1;}
}
