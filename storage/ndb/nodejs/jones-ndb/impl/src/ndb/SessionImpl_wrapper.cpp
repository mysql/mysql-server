/*
 Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "TransactionImpl.h"
#include "QueryOperation.h"
#include "SessionImpl.h"
#include "NativeCFunctionCall.h"
#include "NativeMethodCall.h"
#include "NdbWrappers.h"

using namespace v8;

V8WrapperFn newSessionImpl;
V8WrapperFn seizeTransaction;
V8WrapperFn releaseTransaction;
V8WrapperFn freeTransactions;
V8WrapperFn SessionImplDestructor;

class SessionImplEnvelopeClass : public Envelope {
public:
  SessionImplEnvelopeClass() : Envelope("SessionImpl") {
    addMethod("seizeTransaction", seizeTransaction);
    addMethod("releaseTransaction", releaseTransaction);
    addMethod("freeTransactions", freeTransactions);
    addMethod("destroy", SessionImplDestructor);
  }
};

SessionImplEnvelopeClass SessionImplEnvelope;

Handle<Value> SessionImpl_Wrapper(SessionImpl *dbsi) {
  Local<Value> jsobj = SessionImplEnvelope.wrap(dbsi);
  SessionImplEnvelope.freeFromGC(dbsi, jsobj);
  return jsobj;
}

SessionImpl * asyncNewSessionImpl(Ndb_cluster_connection *conn,
                                  AsyncNdbContext *ctx,
                                  const char *db, int maxTx) {
  return new SessionImpl(conn, ctx, db, maxTx);
}


void newSessionImpl(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  
  PROHIBIT_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(5);

  typedef NativeCFunctionCall_4_<SessionImpl *, Ndb_cluster_connection *,
                                 AsyncNdbContext *, const char *, int> MCALL;
  MCALL * mcallptr = new MCALL(& asyncNewSessionImpl, args);
  mcallptr->wrapReturnValueAs(& SessionImplEnvelope);
  mcallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}

/* The seizeTransaction() wrapper is unusual because a 
   TransactionImpl holds a reference to its own JS wrapper
*/   
void seizeTransaction(const Arguments & args) {
  SessionImpl * session = unwrapPointer<SessionImpl *>(args.Holder());
  TransactionImpl * ctx = session->seizeTransaction();
  if(ctx)
    args.GetReturnValue().Set(ctx->getJsWrapper());
  else
    args.GetReturnValue().SetNull();
}

void releaseTransaction(const Arguments & args) {
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<bool, SessionImpl, TransactionImpl *> MCALL;
  MCALL mcall(& SessionImpl::releaseTransaction, args);
  mcall.run();
  args.GetReturnValue().Set(scope.Escape(mcall.jsReturnVal()));
}

void freeTransactions(const Arguments & args) {
  EscapableHandleScope scope(args.GetIsolate());
  SessionImpl * session = unwrapPointer<SessionImpl *>(args.Holder());
  session->freeTransactions();
  args.GetReturnValue().SetUndefined();
}

void SessionImplDestructor(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  typedef NativeDestructorCall<SessionImpl> DCALL;
  DCALL * dcall = new DCALL(args);
  dcall->runAsync();
  args.GetReturnValue().SetUndefined();
}

void SessionImpl_initOnLoad(Handle<Object> target) {
  Local<String> jsKey = NEW_SYMBOL("DBSession");
  Local<Object> jsObj = Object::New(Isolate::GetCurrent());

  target->Set(jsKey, jsObj);

  DEFINE_JS_FUNCTION(jsObj, "create", newSessionImpl);
}


