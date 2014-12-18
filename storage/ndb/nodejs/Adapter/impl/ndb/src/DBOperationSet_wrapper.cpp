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
#include "Record.h"
#include "NdbWrappers.h"
#include "DBOperationSet.h"
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"

using namespace v8;

Handle<Value> getOperationError(const Arguments &);
Handle<Value> tryImmediateStartTransaction(const Arguments &);
Handle<Value> execute(const Arguments &);
Handle<Value> executeAsynch(const Arguments &);
Handle<Value> readBlobResults(const Arguments &);
Handle<Value> DBOperationSet_freeImpl(const Arguments &);

class DBOperationSetEnvelopeClass : public Envelope {
public:
  DBOperationSetEnvelopeClass() : Envelope("DBOperationSet") {
    DEFINE_JS_FUNCTION(Envelope::stencil, 
      "tryImmediateStartTransaction", tryImmediateStartTransaction);
    DEFINE_JS_FUNCTION(Envelope::stencil, "getOperationError", getOperationError);
    DEFINE_JS_FUNCTION(Envelope::stencil, "execute", execute);
    DEFINE_JS_FUNCTION(Envelope::stencil, "executeAsynch", executeAsynch);
    DEFINE_JS_FUNCTION(Envelope::stencil, "readBlobResults", readBlobResults);
    DEFINE_JS_FUNCTION(Envelope::stencil, "free", DBOperationSet_freeImpl);
  }
};

DBOperationSetEnvelopeClass DBOperationSetEnvelope;

Handle<Value> DBOperationSet_Wrapper(DBOperationSet *set) {
  DEBUG_PRINT("DBOperationSet wrapper");
  HandleScope scope;

  if(set) {
    Local<Object> jsobj = DBOperationSetEnvelope.newWrapper();
    wrapPointerInObject(set, DBOperationSetEnvelope, jsobj);
    freeFromGC(set, jsobj);
    return scope.Close(jsobj);
  }
  return Null();
}

Handle<Value> DBOperationSet_Recycle(Handle<Object> oldWrapper, 
                                     DBOperationSet * newSet) {
  DEBUG_PRINT("DBOperationSet *Recycle*");
  assert(newSet);
  DBOperationSet * oldSet = unwrapPointer<DBOperationSet *>(oldWrapper);
  assert(oldSet == 0);
  wrapPointerInObject(newSet, DBOperationSetEnvelope, oldWrapper);
  return oldWrapper;
}

Persistent<Value> getWrappedObject(DBOperationSet *set) {
  HandleScope scope;
  Local<Object> localObj = DBOperationSetEnvelope.newWrapper();
  wrapPointerInObject(set, DBOperationSetEnvelope, localObj);
  return Persistent<Value>::New(localObj);
}

Handle<Value> getOperationError(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;

  DBOperationSet * set = unwrapPointer<DBOperationSet *>(args.Holder());
  int n = args[0]->Int32Value();

  const NdbError * err = set->getError(n);

  if(err == 0) return True();
  if(err->code == 0) return Null();
  return scope.Close(NdbError_Wrapper(*err));
}

Handle<Value> tryImmediateStartTransaction(const Arguments &args) {
  HandleScope scope;
  DBOperationSet * ctx = unwrapPointer<DBOperationSet *>(args.Holder());
  return ctx->tryImmediateStartTransaction() ? True() : False();
}



/* ASYNC.
*/
/* Execute NdbTransaction.
   DBOperationSet will close the transaction if exectype is not NoCommit;
   in this case, an extra call is made in the js main thread to register the
   transaction as closed.
*/
class TxExecuteAndCloseCall : 
  public NativeMethodCall_3_<int, DBOperationSet, int, int, int> {
public:
  /* Constructor */
  TxExecuteAndCloseCall(const Arguments &args) : 
    NativeMethodCall_3_<int, DBOperationSet, int, int, int>(
      & DBOperationSet::execute, args) 
  {
    errorHandler = getNdbErrorIfLessThanZero;
  }
  void doAsyncCallback(Local<Object>);  
};                               

void TxExecuteAndCloseCall::doAsyncCallback(Local<Object> context) {
  if(arg0 != NdbTransaction::NoCommit) {
    native_obj->registerClosedTransaction();
  }
  NativeMethodCall_3_<int, DBOperationSet, int, int, int>::doAsyncCallback(context);
}

Handle<Value> execute(const Arguments &args) {
  HandleScope scope;
  REQUIRE_ARGS_LENGTH(4);
  TxExecuteAndCloseCall * ncallptr = new TxExecuteAndCloseCall(args);
  ncallptr->runAsync();
  return Undefined();
}


/* IMMEDIATE.
*/
Handle<Value> executeAsynch(const Arguments &args) {
  HandleScope scope;
  /* TODO: The JsValueConverter constructor for arg3 creates a 
     Persistent<Function> from a Local<Value>, but is there 
     actually a chain of destructors that will call Dispose() on it? 
  */  
  typedef NativeMethodCall_4_<int, DBOperationSet, 
                              int, int, int, Persistent<Function> > MCALL;
  MCALL mcall(& DBOperationSet::executeAsynch, args);
  mcall.run();
  return scope.Close(mcall.jsReturnVal());
}


Handle<Value> readBlobResults(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  DBOperationSet * set = unwrapPointer<DBOperationSet *>(args.Holder());
  int n = args[0]->Int32Value();
  if(set->getKeyOperation(n)->isBlobReadOperation()) {
    Handle<Object> results = Array::New();
    BlobReadHandler * blobHandler = static_cast<BlobReadHandler *>(set->getBlobHandler(n));
    while(blobHandler) {
      results->Set(blobHandler->getFieldNumber(), blobHandler->getResultBuffer());
      blobHandler = static_cast<BlobReadHandler *>(blobHandler->getNext());
    }
    return scope.Close(results);
  }
  return Undefined();
}


Handle<Value> DBOperationSet_freeImpl(const Arguments &args) {
  HandleScope scope;
  DBOperationSet * set = unwrapPointer<DBOperationSet *>(args.Holder());
  delete set;
  set = 0;
  wrapPointerInObject(set, DBOperationSetEnvelope, args.Holder());
  return Undefined();
}


