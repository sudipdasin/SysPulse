// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/native/learning.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/file.h>
using namespace syspulse::native;
namespace fs=std::filesystem;
unsigned checks=0;void check(bool ok,const char* label){++checks;if(!ok){std::cerr<<label<<'\n';std::exit(1);}}
int main(int argc,char** argv){
 check(privateSignature("ERROR /home/SecretName/1024 on host-123") == "error *","redaction");
 check(privateSignature("Frobnicator 1234") == "unclassified error","unknown signature");
 check(privateSignature("GPU reset: device 01") == "gpu reset * device *","known words");
 check(validSignature("gpu reset * device *"),"valid template");check(!validSignature("gpu SecretName"),"raw secret refused");check(!validSignature("gpu * *"),"collapsed masks");check(!validSignature(" gpu"),"canonical spaces");
 auto a=diagnoseJournal("{\"MESSAGE\":\"GPU hang SecretMachine 42\"}\n{\"MESSAGE\":\"GPU hang PrivateHost 99\"}\n",true,false);
 check(a.patterns.size()==1&&a.patterns[0].signature=="gpu hang *"&&a.patterns[0].count==2,"repeat pattern mining");check(a.counts[5]==2,"category count");
 LearningHistory h;h.analyses={a,a};auto report=learningReport(h);check(report.find("gpu: 2 samples")!=report.npos&&report.find("gpu hang *: 2 samples")!=report.npos,"recurrence");check(report.find("SecretMachine")==report.npos,"privacy");
 auto c=LearningContext::Disk;auto action=LearningAction::Thumbnails;
 h.outcomes={{c,action,LearningResult::Failure,0},{c,action,LearningResult::NoEffect,0},{c,action,LearningResult::Applied,42}};
 auto r=actionHistory(h,c,action);check(r.attempts==3&&r.successes==1&&r.failures==1&&r.noEffect==1&&r.demoted(),"legacy demotion threshold");check(!actionHistory(h,LearningContext::Memory,action).demoted(),"context isolation");check(learningReport(h).find("manual review preferred")!=report.npos,"advice");
 check(!rememberOutcome({c,action,LearningResult::Applied,0,42,{}}),"incomplete observation refused");check(!rememberOutcome({c,action,LearningResult::Failure,123}),"failure cannot claim bytes");check(!rememberOutcome({c,LearningAction::OwnMemory,LearningResult::Applied,42}),"RAM cannot claim disk bytes");auto bad=a;bad.patterns[0].signature="SecretMachine";check(!rememberAnalysis(bad),"persist rejects arbitrary string");check(!rememberAnalysis({}),"unavailable sample not persisted");
 if(argc==2&&std::string(argv[1])=="--isolated-home"){
   check(getuid()!=0,"ordinary account required");const auto* pw=getpwuid(getuid());check(pw&&pw->pw_dir,"account home");fs::path home=pw->pw_dir;check(home.string().rfind("/tmp/sp-learning-fixture-",0)==0,"dedicated disposable test home required");auto dir=home/".syspulse-native-history",file=dir/"history-v1";check(!fs::exists(dir),"fresh isolated history");
   check(rememberAnalysis(a),"actual persisted analysis");check(rememberOutcome({c,action,LearningResult::Applied,42,1000,1042}),"actual persisted action");auto stored=readLearningHistory();check(stored&&stored->analyses.size()==1&&stored->outcomes.size()==1&&stored->outcomes[0].logicalBytes==42&&stored->outcomes[0].observedBefore==1000&&stored->outcomes[0].observedAfter==1042,"actual reload");struct stat st{};check(!stat(dir.c_str(),&st)&&(st.st_mode&0777)==0700,"private directory");check(!stat(file.c_str(),&st)&&(st.st_mode&0777)==0600,"private file");std::ifstream in(file);std::string raw((std::istreambuf_iterator<char>(in)),{});check(raw.find("SecretMachine")==raw.npos&&raw.find("PrivateHost")==raw.npos,"disk privacy");
   int lock=open((dir/"lock").c_str(),O_RDWR);check(lock>=0&&!flock(lock,LOCK_EX|LOCK_NB),"acquire fixture lock");check(!rememberAnalysis(a),"concurrent writer refusal");close(lock);check(readLearningHistory()->analyses.size()==1,"locked history unchanged");
   for(int i=0;i<182;++i){check(rememberAnalysis(a),"retention insertion");}check(readLearningHistory()->analyses.size()==180,"bounded retention");
   fs::rename(file,dir/"old");std::ofstream(home/"personal")<<"unchanged";fs::create_symlink(home/"personal",file);check(!readLearningHistory()&&!rememberAnalysis(a),"symlink refusal");fs::remove(file);fs::rename(dir/"old",file);std::ofstream(file,std::ios::app)<<"malformed\n";check(!readLearningHistory()&&!rememberOutcome({c,action,LearningResult::Applied,1}),"corruption refusal");fs::remove_all(dir);fs::create_directory_symlink(home,dir);check(!readLearningHistory()&&!rememberAnalysis(a),"directory symlink refusal");fs::remove(dir);check(fs::file_size(home/"personal")==9,"personal file preserved");
 }else if(!getuid())check(!readLearningHistory()&&!rememberAnalysis(a),"root persistence refused");
 std::cout<<checks<<" learning checks passed\n";
}
