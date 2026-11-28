// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/backup.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
using namespace syspulse::native;
namespace fs=std::filesystem;
int main(){char dir[]="/tmp/syspulse-copy-test-XXXXXX";if(!mkdtemp(dir))return 1;struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}} cleanup{dir};auto s=cleanup.p/"source",d=cleanup.p/"backup",r=cleanup.p/"restored";std::ofstream(s)<<"abc";BackupReview review;
 if(getuid()==0){if(review.prepare(s,d)||review.apply().published)return 2;std::cout<<"Root refusal passed; ordinary-user action tests require non-root.\n";return 0;}
 if(review.prepare("relative",d)||review.prepare(s,s)||review.apply().published)return 3;
 if(!review.prepare(s,d))return 4;
 review.cancel();if(review.apply().published||fs::exists(d))return 5;
 if(!review.prepare(s,d))return 6;
 std::ofstream(s,std::ios::app)<<"x";if(review.apply().published||fs::exists(d))return 7;
 std::ofstream(s)<<"abc";if(!review.prepare(s,d))return 8;
 std::ofstream(d)<<"existing";if(review.apply().published)return 9;fs::remove(d);
 if(!review.prepare(s,d))return 10;
 auto out=review.apply();if(!out.published||!out.durable||out.bytes!=3||out.sha256!="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")return 11;
 if(review.apply().published||review.prepare(s,d))return 12;
 if(!review.prepare(d,r)||!review.apply().published)return 13;
 std::ifstream restored(r);std::string data;restored>>data;if(data!="abc"||!fs::exists(s))return 14;
 fs::create_symlink(s,cleanup.p/"link");if(review.prepare(cleanup.p/"link",cleanup.p/"another"))return 15;
 fs::create_directory_symlink(cleanup.p,cleanup.p/"alias");if(review.prepare(s,cleanup.p/"alias/new"))return 16;
 fs::create_hard_link(s,cleanup.p/"hard");if(review.prepare(s,cleanup.p/"new"))return 17;
 fs::remove(cleanup.p/"hard");if(!review.prepare(s,cleanup.p/"new"))return 18;
 fs::rename(s,cleanup.p/"old");std::ofstream(s)<<"abc";if(review.apply().published)return 19;
 std::cout<<"Actual verified backup and restore, no overwrite, cancel, replay, symlink, hardlink and changed identity passed.\n";
}
