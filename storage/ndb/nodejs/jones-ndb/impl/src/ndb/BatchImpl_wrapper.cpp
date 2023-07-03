/*
 Copyright (c) 2014, 2023, Oracle and/or its affiliates.
 
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
#include "Record.h"
#include "NdbWrappers.h"
#include "BatchImpl.h"
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"

V8WrapperFn getOperationError,
            tryImmediateStartTransaction,
            execute,
            executeAsynch,
            readBlobResults,
            BatchImpl_freeImpl;

class BatchImplEnvelopeClass : public Envelope {
public:
  BatchImplEnvelopeClass() : Envelope("BatchImpl") {
    addMethod("tryImmediateStartTransaction", tryImmediateStartTransaction);
    addMethod("getOperationError", getOperationError);
    addMethod("execute", execute);
    addMethod("executeAsynch", executeAsynch);
    addMethod("readBlobResults", readBlobResults);
    addMethod("free", BatchImpl_freeImpl);
  }
};

BatchImplEnvelopeClass BatchImplEnvelope;

Local<Value> BatchImpl_Wrapper(BatchImpl *set) {
  Local<Value> jsobj = BatchImplEnvelope.wrap(set);
  BatchImplEnvelope.freeFromGC(set, jsobj);
  return jsobj;
}

// This version is *not* freed from GC
Local<Value> getWrappedObject(BatchImpl *set) {
  return BatchImplEnvelope.wrap(set);
}

Local<Value> BatchImpl_Recycle(Local<Object> oldWrapper,
                               BatchImpl * newSet) {
  DEBUG_PRINT("BatchImpl *Recycle*");
  BatchImpl * oldSet = unwrapPointer<BatchImpl *>(oldWrapper);
  assert(oldSet == 0);
  assert(newSet != 0);
  wrapPointerInObject(newSet, BatchImplEnvelope, oldWrapper);
  return oldWrapper;
}


void getOperationError(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());

  BatchImpl * set = unwrapPointer<BatchImpl *>(args.Holder());
  int n = GetInt32Arg(args, 0);

  const NdbError * err = set->getError(n);

  Local<Value> opErrHandle;
  if(err == 0) opErrHandle = True(args.GetIsolate());
  else if(err->code == 0) opErrHandle = Null(args.GetIsolate());
  else opErrHandle = NdbError_Wrapper(*err);

  args.GetReturnValue().Set(scope.Escape(opErrHandle));
}

void tryImmediateStartTransaction(const Arguments &args) {
  BatchImpl * ctx = unwrapPointer<BatchImpl *>(args.Holder());
  args.GetReturnValue().Set((bool) ctx->tryImmediateStartTransaction());
}



/* ASYNC.
*/
/* Execute NdbTransaction.
   BatchImpl will close the transaction if exectype is not NoCommit;
   in this case, an extra call is made in the js main thread to register the
   transaction as closed.
*/
class TxExecuteAndCloseCall : 
  public NativeMethodCall_3_<int, BatchImpl, int, int, int> {
public:
  /* Constructor */
  TxExecuteAndCloseCall(const Arguments &args) : 
    NativeMethodCall_3_<int, BatchImpl, int, int, int>(
      & BatchImpl::execute, args) 
  {
    errorHandler = getNdbErrorIfLessThanZero;
  }
  void doAsyncCallback(Local<Object>) override;  
};                               

void TxExecuteAndCloseCall::doAsyncCallback(Local<Object> context) {
  if(arg0 != NdbTransaction::NoCommit) {
    native_obj->registerClosedTransaction();
  }
  NativeMethodCall_3_<int, BatchImpl, int, int, int>::doAsyncCallback(context);
}

void execute(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());
  REQUIRE_ARGS_LENGTH(4);
  TxExecuteAndCloseCall * ncallptr = new TxExecuteAndCloseCall(args);
  ncallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}


/* IMMEDIATE.
*/
void executeAsynch(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_4_<int, BatchImpl,
                              int, int, int, Local<Function> > MCALL;
  MCALL mcall(& BatchImpl::executeAsynch, args);
  mcall.run();
  args.GetReturnValue().Set(mcall.jsReturnVal());
}


void readBlobResults(const Arguments &args) {
  BatchImpl * set = unwrapPointer<BatchImpl *>(args.Holder());
  int n = GetInt32Arg(args, 0);
  set->getKeyOperation(n)->readBlobResults(args);
//  args.GetReturnValue().Set(set->getKeyOperation(n)->readBlobResults());
}


void BatchImpl_freeImpl(const Arguments &args) {
  BatchImpl * set = unwrapPointer<BatchImpl *>(args.Holder());
  delete set;
  set = 0;
  wrapPointerInObject(set, BatchImplEnvelope, args.Holder());
  args.GetReturnValue().SetUndefined();
}


