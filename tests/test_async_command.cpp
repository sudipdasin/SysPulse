// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/async_command.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QPointer>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
namespace {
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
bool await(const std::function<bool()>& ready,int ms=3000){
    QElapsedTimer timer;timer.start();
    while(!ready()&&timer.elapsed()<ms){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QThread::msleep(2);}
    return ready();
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc==2){
        const QString mode=QString::fromLocal8Bit(argv[1]);
        if(mode=="output"){std::cout<<"fixture"<<std::flush;return 0;}
        if(mode=="flood"){std::cout<<std::string(8192,'x')<<std::flush;return 0;}
        if(mode=="sleep"){QThread::msleep(800);return 0;}
        if(mode=="failure")return 7;
    }
    try{
        QObject owner;const auto self=app.applicationFilePath();int calls=0;AsyncCommandResult result;
        const auto done=[&](AsyncCommandResult r){++calls;result=std::move(r);};
        const auto run=[&](const QString& mode,int timeout=2000,int limit=1024){
            calls=0;result={};AsyncCommand::run(&owner,self,{mode},timeout,limit,done);
            require(await([&]{return calls!=0;}),"query completion");
            require(calls==1,"one callback");
        };
        run("output");require(result.ok()&&result.output=="fixture","success and captured output");
        run("failure");require(!result.ok()&&result.exitCode==7,"nonzero exit");
        run("flood",2000,128);require(result.truncated&&result.output.size()==128&&!result.ok(),"bounded output");
        run("sleep",40);require(result.timedOut&&!result.ok(),"deadline");
        calls=0;auto* query=AsyncCommand::run(&owner,self,{"sleep"},2000,1024,done);query->cancel();
        require(await([&]{return calls!=0;})&&calls==1&&result.cancelled&&!result.ok(),"explicit cancel");
        calls=0;AsyncCommand::run(&owner,"/nonexistent-syspulse-test-command",{},2000,1024,done);
        require(await([&]{return calls!=0;})&&calls==1&&!result.ok()&&!result.error.isEmpty(),"missing executable");
        calls=0;auto* temporary=new QObject;QPointer<AsyncCommand> owned=AsyncCommand::run(temporary,self,{"sleep"},2000,1024,done);
        delete temporary;require(owned.isNull(),"owner destroys query");
        QElapsedTimer delay;delay.start();await([&]{return delay.elapsed()>=100;},200);
        require(calls==0,"destroyed owner receives no callback");
        calls=0;auto* action=AsyncCommand::runAction(&owner,self,{"sleep"},1024,done);
        action->cancel();require(action->active(),"reviewed action cannot be cancelled");
        require(await([&]{return calls!=0;})&&calls==1&&result.ok()&&!result.cancelled,"action reports actual exit");
        std::cout<<"Async command success, failure, output cap, timeout, cancel, missing dependency, owner lifetime and action admission passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
