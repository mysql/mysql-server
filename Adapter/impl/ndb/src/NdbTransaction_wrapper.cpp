/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"
#include "NdbJsConverters.h"

using namespace v8;

Handle<Value> getTCNodeId(const Arguments &args);
Handle<Value> executeAndClose(const Arguments &args);
Handle<Value> commitStatus(const Arguments &args);

class NdbTransactionEnvelopeClass : public Envelope {
public:
  NdbTransactionEnvelopeClass() : Envelope("NdbTransaction") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "getConnectedNodeId", getTCNodeId); 
    DEFINE_JS_FUNCTION(Envelope::stencil, "executeAndClose", executeAndClose);
    DEFINE_JS_FUNCTION(Envelope::stencil, "commitStatus", commitStatus);
    DEFINE_JS_FUNCTION(Envelope::stencil, "getNdbError", getNdbError<NdbTransaction>);
  }
};

NdbTransactionEnvelopeClass NdbTransactionEnvelope;

Envelope * getNdbTransactionEnvelope() {
  return & NdbTransactionEnvelope;
}

//////////// IMMEDIATE METHOD WRAPPERS


/* Uint32 getConnectedNodeId(); 
   Get nodeId of TC for this transaction
   IMMEDIATE
*/
Handle<Value> getTCNodeId(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);

  typedef NativeMethodCall_0_<uint32_t, NdbTransaction> NCALL;
  NCALL ncall(& NdbTransaction::getConnectedNodeId, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}


Handle<Value> commitStatus(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);
  typedef NativeMethodCall_0_<NdbTransaction::CommitStatusType, NdbTransaction> 
    NCALL;
  NCALL ncall(& NdbTransaction::commitStatus, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

/* Execute NdbTransaction.
   If commit type is Commit or Rollback, also close the transaction.
   If NdbTransaction::execute() returns non-zero, get the NdbError condition.
*/
class TxExecuteAndCloseCall : 
  public NativeMethodCall_3_<int, NdbTransaction, NdbTransaction::ExecType,
                            NdbOperation::AbortOption, int> {
public:
  /* Constructor */
  TxExecuteAndCloseCall(const Arguments &args) : 
    NativeMethodCall_3_<int, NdbTransaction, NdbTransaction::ExecType,
                        NdbOperation::AbortOption, int>(NULL, args) {}
  void run();
};                               

void TxExecuteAndCloseCall::run() {
  return_val = native_obj->execute(arg0, arg1, arg2);
  if(return_val != 0) {
    error = new NdbNativeCodeError(native_obj->getNdbError());
  }
  if(arg0 != NdbTransaction::NoCommit) {
    native_obj->close();
  }
}


Handle<Value> executeAndClose(const Arguments &args) {
  REQUIRE_ARGS_LENGTH(4);
  TxExecuteAndCloseCall * ncallptr = new TxExecuteAndCloseCall(args);
  ncallptr->runAsync();
  return Undefined();
}


void NdbTransaction_initOnLoad(Handle<Object> target) {
  DEFINE_JS_INT(target, "NoCommit", NdbTransaction::NoCommit);
  DEFINE_JS_INT(target, "Commit", NdbTransaction::Commit);
  DEFINE_JS_INT(target, "Rollback", NdbTransaction::Rollback);
  DEFINE_JS_INT(target, "DefaultAbortOption", NdbOperation::DefaultAbortOption);
  DEFINE_JS_INT(target, "AbortOnError", NdbOperation::AbortOnError);
  DEFINE_JS_INT(target, "AO_IgnoreError", NdbOperation::AO_IgnoreError);
  DEFINE_JS_INT(target, "NotStarted", NdbTransaction::NotStarted);
  DEFINE_JS_INT(target, "Started", NdbTransaction::Started);
  DEFINE_JS_INT(target, "Committed", NdbTransaction::Committed);
  DEFINE_JS_INT(target, "Aborted", NdbTransaction::Aborted);
  DEFINE_JS_INT(target, "NeedAbort", NdbTransaction::NeedAbort);
}
