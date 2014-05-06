/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "DBTransactionContext.h"
#include "DBSessionImpl.h"
#include "NativeMethodCall.h"

using namespace v8;

Handle<Value> newDBSessionImpl(const Arguments &);
Handle<Value> seizeTransaction(const Arguments &);
Handle<Value> releaseTransaction(const Arguments &);
Handle<Value> DBSessionImplDestructor(const Arguments &);


class DBSessionImplEnvelopeClass : public Envelope {
public:
  DBSessionImplEnvelopeClass() : Envelope("DBSessionImpl") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "seizeTransaction", seizeTransaction);
    DEFINE_JS_FUNCTION(Envelope::stencil, "releaseTransaction", releaseTransaction);
    DEFINE_JS_FUNCTION(Envelope::stencil, "destroy", DBSessionImplDestructor);
  }
};

DBSessionImplEnvelopeClass DBSessionImplEnvelope;

Handle<Value> DBSessionImpl_Wrapper(DBSessionImpl *dbsi) {
  HandleScope scope;

  if(dbsi) {
    Local<Object> jsobj = DBSessionImplEnvelope.newWrapper();
    wrapPointerInObject(dbsi, DBSessionImplEnvelope, jsobj);
    freeFromGC(dbsi, jsobj);
    return scope.Close(jsobj);
  }
  return Null();
}

Handle<Value> newDBSessionImpl(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  PROHIBIT_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(4);

  JsValueConverter<Ndb_cluster_connection *> arg0(args[0]);
  JsValueConverter<const char *> arg1(args[1]);
  JsValueConverter<AsyncNdbContext *> arg2(args[2]);
  JsValueConverter<int> arg3(args[3]);
  
  DBSessionImpl * d = new DBSessionImpl(arg0.toC(), arg1.toC(), 
                                        arg2.toC(), arg3.toC());
  
  return scope.Close(DBSessionImpl_Wrapper(d));
}

/* The seizeTransaction() wrapper is unusual because a 
   DBTransactionContext holds a reference to its own JS wrapper
*/   
Handle<Value> seizeTransaction(const Arguments & args) {
  DBSessionImpl * session = unwrapPointer<DBSessionImpl *>(args.Holder());
  DBTransactionContext * ctx = session->seizeTransaction();
  if(ctx) return ctx->getJsWrapper();
  return Null();
}

Handle<Value> releaseTransaction(const Arguments & args) {
  HandleScope scope;
  typedef NativeMethodCall_1_<int, DBSessionImpl, DBTransactionContext *> MCALL;
  MCALL mcall(& DBSessionImpl::releaseTransaction, args);
  mcall.run();
  return scope.Close(mcall.jsReturnVal());
}


Handle<Value> DBSessionImplDestructor(const Arguments &args) {
  typedef NativeDestructorCall<DBSessionImpl> DCALL;
  DCALL dcall(args);
  dcall.run(); 
  return Undefined();
}

void DBSessionImpl_initOnLoad(Handle<Object> target) {
  HandleScope scope;

  Persistent<String> jsKey = Persistent<String>(String::NewSymbol("DBSession"));
  Persistent<Object> jsObj = Persistent<Object>(Object::New());

  target->Set(jsKey, jsObj);

  DEFINE_JS_FUNCTION(jsObj, "create", newDBSessionImpl);
}


