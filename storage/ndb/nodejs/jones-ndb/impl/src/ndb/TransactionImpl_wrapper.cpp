/*
 Copyright (c) 2014, 2022, Oracle and/or its affiliates.
 
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
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"


V8WrapperFn getEmptyOperationSet;

class TransactionImplEnvelopeClass : public Envelope {
public:
  TransactionImplEnvelopeClass() : Envelope("TransactionImpl") {
    EscapableHandleScope scope(v8::Isolate::GetCurrent());
    addMethod("getEmptyOperationSet", getEmptyOperationSet);
    addMethod("getNdbError", getNdbError<TransactionImpl>);
  }
};

TransactionImplEnvelopeClass TransactionImplEnvelope;

void setJsWrapper(TransactionImpl *ctx) {
  Local<Object> localObj = ToObject(TransactionImplEnvelope.wrap(ctx));
  ctx->jsWrapper.Reset(ctx->isolate, localObj);
}


void getEmptyOperationSet(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  TransactionImpl * ctx = unwrapPointer<TransactionImpl *>(args.Holder());
  args.GetReturnValue().Set(ctx->getWrappedEmptyOperationSet());
}


void NdbTransaction_initOnLoad(Local<Object> target) {
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
