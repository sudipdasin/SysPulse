// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/admin_session.hpp"
#include "tux/async_command.hpp"
#include <QThread>
#include <memory>
namespace tux::admin {
// Only callers with an already-reviewed typed request can reuse authorization.
// If the existing session is lost, never silently fall back to another privileged run.
inline void runReviewed(QObject* owner,const QStringList& request,const QString& program,const QStringList& args,int limit,AsyncCommand::Completion done){
    if(!active()){AsyncCommand::runAction(owner,program,args,limit,std::move(done));return;}
    auto result=std::make_shared<AsyncCommandResult>();
    beginDispatch();
    auto* thread=QThread::create([request,limit,result]{
        try{
            auto r=executeTyped(request,limit);
            if(r){result->exitCode=r->code;result->output=r->output;result->truncated=r->truncated;}
            else result->error="Administrator session ended before execution. Review and retry explicitly.";
        }catch(...){result->error="Administrator operation interrupted. Inspect state before retrying.";}
        endDispatch();
    });
    QObject::connect(thread,&QThread::finished,owner,[result,done=std::move(done)]{done(*result);});
    QObject::connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();
}
}
