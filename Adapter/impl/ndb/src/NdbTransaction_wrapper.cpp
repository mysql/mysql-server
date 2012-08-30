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
#include "NativeMethodCall.h"
#include "unified_debug.h"

#include "NdbJsConverters.h"
#include "NdbWrapperErrors.h"

using namespace v8;

Handle<Value> getTCNodeId(const Arguments &args);
Handle<Value> execute(const Arguments &args);


class NdbTransactionEnvelopeClass : public Envelope {
public:
  NdbTransactionEnvelopeClass() : Envelope("NdbTransaction") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "getConnectedNodeId", getTCNodeId); 
    DEFINE_JS_FUNCTION(Envelope::stencil, "execute", execute); 
  }
};

NdbTransactionEnvelopeClass NdbTransactionEnvelope;


/******* immediate wrapper template
Handle<Value>  _wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH( );

  typedef NativeMethodCall_ _< , NdbTransaction, , > NCALL;

  NCALL ncall(args);
  ncall.method = & NdbTransaction:: ;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}
*******/



//////////// IMMEDIATE METHOD WRAPPERS


/* Uint32 getConnectedNodeId(); 
   Get nodeId of TC for this transaction
   IMMEDIATE
*/
Handle<Value> getTCNodeId(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);

  NativeMethodCall_0_<uint32_t, NdbTransaction> ncall(args);
  ncall.method = & NdbTransaction::getConnectedNodeId;
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}


//////////// ASYNC METHOD WRAPPERS

Handle<Value> execute(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(4);

  typedef NativeMethodCall_3_<int, NdbTransaction, NdbTransaction::ExecType,
                              NdbOperation::AbortOption, int> NCALL;
  NCALL * ncallptr = new NCALL(args);
  ncallptr->method = & NdbTransaction::execute;
  ncallptr->envelope = & NdbTransactionEnvelope;
  ncallptr->errorHandler = getNdbErrorIfNonZero<int, NdbTransaction>;
  // todo: set error handler
  ncallptr->runAsync();

  return scope.Close(JS_VOID_RETURN);
}


void NdbTransaction_initOnLoad(Handle<Object> target) {
  DEFINE_JS_INT(target, "NoCommit", NdbTransaction::NoCommit);
  DEFINE_JS_INT(target, "Commit", NdbTransaction::Commit);
  DEFINE_JS_INT(target, "DefaultAbortOption", NdbOperation::DefaultAbortOption);
  DEFINE_JS_INT(target, "AbortOnError", NdbOperation::AbortOnError);
  DEFINE_JS_INT(target, "AO_IgnoreError", NdbOperation::AO_IgnoreError);
  DEFINE_JS_INT(target, "NotStarted", NdbTransaction::NotStarted);
  DEFINE_JS_INT(target, "Started", NdbTransaction::Started);
  DEFINE_JS_INT(target, "Committed", NdbTransaction::Committed);
  DEFINE_JS_INT(target, "Aborted", NdbTransaction::Aborted);
  DEFINE_JS_INT(target, "NeedAbort", NdbTransaction::NeedAbort);
}
