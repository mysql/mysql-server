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

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "Record.h"
#include "NdbWrappers.h"
#include "PendingOperationSet.h"

using namespace v8;

Handle<Value> getOperationError(const Arguments &);

class PendingOperationSetEnvelopeClass : public Envelope {
public:
  PendingOperationSetEnvelopeClass() : Envelope("PendingOperationSet") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "getOperationError", getOperationError);
  }
};

PendingOperationSetEnvelopeClass PendingOperationSetEnvelope;

Handle<Value> PendingOperationSet_Wrapper(PendingOperationSet *set) {
  HandleScope scope;

  if(set) {
    Local<Object> jsobj = PendingOperationSetEnvelope.newWrapper();
    wrapPointerInObject(set, PendingOperationSetEnvelope, jsobj);
    freeFromGC(set, jsobj);
    return scope.Close(jsobj);
  }
  return Null();
}


Handle<Value> getOperationError(const Arguments & args) {
  HandleScope scope;

  PendingOperationSet * set = unwrapPointer<PendingOperationSet *>(args.This());
  int n = args[0]->Int32Value();

  const NdbError * err = set->getOperationError(n);

  if(err == 0) return True();
  if(err->code == 0) return Null();
  return scope.Close(NdbError_Wrapper(*err));
}




