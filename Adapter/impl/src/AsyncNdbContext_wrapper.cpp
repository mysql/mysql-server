/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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
#include "AsyncNdbContext.h"

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NativeMethodCall.h"
#include "unified_debug.h"
#include "JsWrapper.h"
#include "NdbWrapperErrors.h"

using namespace v8;

Handle<Value> createAsyncNdbContext(const Arguments &args);
Handle<Value> startListenerThread(const Arguments &args);
Handle<Value> executeAsynch(const Arguments &args);


/* Envelope
*/
class AsyncNdbContextEnvelopeClass : public Envelope {
public:
  AsyncNdbContextEnvelopeClass() : Envelope("AsyncNdbContext") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "startListenerThread", startListenerThread);
    DEFINE_JS_FUNCTION(Envelope::stencil, "executeAsynch", executeAsynch);
  }
};

AsyncNdbContextEnvelopeClass AsyncNdbContextEnvelope;


/* Constructor 
*/
Handle<Value> createAsyncNdbContext(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(1);

  JsValueConverter<Ndb_cluster_connection *> arg0(args[0]);
  AsyncNdbContext * ctx = new AsyncNdbContext(arg0.toC());
  
  wrapPointerInObject(ctx, AsyncNdbContextEnvelope, args.This());
  return args.This();
}


/* startListenerThread
   ASYNC
*/
Handle<Value> startListenerThread(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);

  typedef NativeVoidMethodCall_0_<AsyncNdbContext> NCALL;
  NCALL * ncallptr = new NCALL(& AsyncNdbContext::startListenerThread, args);
  ncallptr->runAsync();
  return scope.Close(JS_VOID_RETURN);
}


/* executeAsynch(NdbTransaction *,
                 execType,
                 abortOption,
                 forceSend,
                 execCompleteCallback,
                 execSentCallback)
   ASYNC.
   The sending of the transaction to Ndb will happen in a UV worker thread.
*/
Handle<Value> executeAsynch(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(6);
  
  typedef NativeMethodCall_5_<int, AsyncNdbContext, NdbTransaction *,
                              int, int, int, Local<Value> > NCALL;
  NCALL * ncallptr = new NCALL(& AsyncNdbContext::executeAsynch, args);
  ncallptr->runAsync();
  return scope.Close(JS_VOID_RETURN);
}

void AsyncNdbContext_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  DEFINE_JS_FUNCTION(target, "AsyncNdbConext", createAsyncNdbContext);
  
}
