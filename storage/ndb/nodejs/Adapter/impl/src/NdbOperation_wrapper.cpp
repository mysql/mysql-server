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


/* Wrapper for four classes: 
    NdbOperation
    NdbIndexOperation
    NdbScanOperation 
    NdbIndexScanOperation
*/

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NativeMethodCall.h"
#include "unified_debug.h"
#include "JsWrapper.h"
#include "NdbWrapperErrors.h"

using namespace v8;

class NdbOperationEnvelopeClass : public Envelope {
public:
  NdbOperationEnvelopeClass() : Envelope("NdbOperation") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "getNdbError", 
                       getNdbError<NdbOperation>);
  }
};

NdbOperationEnvelopeClass NdbOperationEnvelope;
Envelope NdbScanOperationEnvelope("NdbScanOperation");
Envelope NdbIndexScanOperationEnvelope("NdbIndexScanOperation");


Handle<Value> NdbOperation_Wrapper(const NdbOperation *op) {
  HandleScope scope;
  if(op) {
    Local<Object> jsobj = NdbOperationEnvelope.newWrapper();
    wrapPointerInObject(op, NdbOperationEnvelope, jsobj);
    return scope.Close(jsobj);
  }
  return Null();
}


Handle<Value> NdbScanOperation_Wrapper(NdbScanOperation *op) {
  HandleScope scope;
  Local<Object> jsobj = NdbScanOperationEnvelope.newWrapper();
  wrapPointerInObject(op, NdbScanOperationEnvelope, jsobj);
  return scope.Close(jsobj);
}


Handle<Value> NdbIndexScanOperation_Wrapper(NdbIndexScanOperation *op) {
  HandleScope scope;
  Local<Object> jsobj = NdbIndexScanOperationEnvelope.newWrapper();
  wrapPointerInObject(op, NdbIndexScanOperationEnvelope, jsobj);
  return scope.Close(jsobj);
}



