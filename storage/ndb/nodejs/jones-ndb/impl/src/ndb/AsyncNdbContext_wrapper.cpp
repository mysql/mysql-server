/*
 Copyright (c) 2013, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "AsyncNdbContext.h"

#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"
#include "Record.h"
#include "adapter_global.h"
#include "js_wrapper_macros.h"

V8WrapperFn createAsyncNdbContext;
V8WrapperFn shutdown;
V8WrapperFn destroy;

/* Envelope
 */

class AsyncNdbContextEnvelopeClass : public Envelope {
 public:
  AsyncNdbContextEnvelopeClass() : Envelope("AsyncNdbContext") {
    EscapableHandleScope scope(Isolate::GetCurrent());
    addMethod("AsyncNdbContext", createAsyncNdbContext);
    addMethod("shutdown", shutdown);
    addMethod("delete", destroy);
  }
};

AsyncNdbContextEnvelopeClass AsyncNdbContextEnvelope;

/* Constructor
 */
void createAsyncNdbContext(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);

  REQUIRE_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(1);

  JsValueConverter<Ndb_cluster_connection *> arg0(args[0]);
  AsyncNdbContext *ctx = new AsyncNdbContext(arg0.toC());
  Local<Value> wrapper = AsyncNdbContextEnvelope.wrap(ctx);
  args.GetReturnValue().Set(wrapper);
}

/* shutdown()
   IMMEDIATE
*/
void shutdown(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(0);

  typedef NativeVoidMethodCall_0_<AsyncNdbContext> NCALL;
  NCALL ncall(&AsyncNdbContext::shutdown, args);
  ncall.run();
  args.GetReturnValue().SetUndefined();
}

/* Call destructor
 */
void destroy(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(0);

  AsyncNdbContext *c = unwrapPointer<AsyncNdbContext *>(args.Holder());
  delete c;
  args.GetReturnValue().SetUndefined();
}

void AsyncNdbContext_initOnLoad(Local<Object> target) {
  DEFINE_JS_FUNCTION(target, "AsyncNdbContext", createAsyncNdbContext);
}
