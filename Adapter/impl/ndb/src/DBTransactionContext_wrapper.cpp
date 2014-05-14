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
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"

using namespace v8;

Handle<Value> getEmptyOperationSet(const Arguments &);

class DBTransactionContextEnvelopeClass : public Envelope {
public:
  DBTransactionContextEnvelopeClass() : Envelope("DBTransactionContext") {
    DEFINE_JS_FUNCTION(Envelope::stencil, 
      "getEmptyOperationSet", getEmptyOperationSet);
  }
};

DBTransactionContextEnvelopeClass DBTransactionContextEnvelope;

void setJsWrapper(DBTransactionContext *ctx) {
  HandleScope scope;
  Local<Object> localObj = DBTransactionContextEnvelope.newWrapper();
  wrapPointerInObject(ctx, DBTransactionContextEnvelope, localObj);
  ctx->jsWrapper = Persistent<Value>::New(localObj);
}


Handle<Value> getEmptyOperationSet(const Arguments &args) {
  HandleScope scope;
  DEBUG_MARKER(UDEB_DEBUG);
  DBTransactionContext * ctx = unwrapPointer<DBTransactionContext *>(args.Holder());
  return ctx->getWrappedEmptyOperationSet();
}
