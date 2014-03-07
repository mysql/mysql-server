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
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"

using namespace v8;


Handle<Value> get_status(Local<String>, const AccessorInfo &);
Handle<Value> get_classification(Local<String>, const AccessorInfo &);
Handle<Value> get_code(Local<String>, const AccessorInfo &);
Handle<Value> get_mysql_code(Local<String>, const AccessorInfo &);
Handle<Value> get_message(Local<String>, const AccessorInfo &);


class NdbErrorEnvelopeClass : public Envelope {
public:
  NdbErrorEnvelopeClass() : Envelope("NdbError") {
    DEFINE_JS_ACCESSOR(Envelope::stencil, "status", get_status);
    DEFINE_JS_ACCESSOR(Envelope::stencil, "classification", get_classification);
    DEFINE_JS_ACCESSOR(Envelope::stencil, "code", get_code);
    DEFINE_JS_ACCESSOR(Envelope::stencil, "handler_error_code", get_mysql_code);
    DEFINE_JS_ACCESSOR(Envelope::stencil, "message", get_message);
}

  Local<Object> wrap(const NdbError * err) {
    HandleScope scope;    
    Local<Object> wrapper = Envelope::stencil->NewInstance();
    wrapPointerInObject(err, *this, wrapper);
    return scope.Close(wrapper);
  }
};

NdbErrorEnvelopeClass NdbErrorEnvelope;


Handle<Value> NdbError_Wrapper(const NdbError &err) {
  return NdbErrorEnvelope.wrap(& err);
}

#define MAP_CODE(CODE) \
  case NdbError::CODE: \
    return String::New(#CODE)


Handle<Value> get_status(Local<String> property, const AccessorInfo &info) {
  const NdbError *err = unwrapPointer<const NdbError *>(info.Holder());
  
  switch(err->status) {
    MAP_CODE(Success);
    MAP_CODE(TemporaryError);
    MAP_CODE(PermanentError);
    MAP_CODE(UnknownResult);  
  }
  return String::New("-unknown-");
}


Handle<Value> get_classification(Local<String> property, const AccessorInfo &info) {
  const NdbError *err = unwrapPointer<const NdbError *>(info.Holder());

  switch(err->classification) {
    MAP_CODE(NoError);
    MAP_CODE(ApplicationError);
    MAP_CODE(NoDataFound);
    MAP_CODE(ConstraintViolation);
    MAP_CODE(SchemaError);
    MAP_CODE(UserDefinedError);
    MAP_CODE(InsufficientSpace);
    MAP_CODE(TemporaryResourceError);
    MAP_CODE(NodeRecoveryError);
    MAP_CODE(OverloadError);
    MAP_CODE(TimeoutExpired);
    MAP_CODE(UnknownResultError);
    MAP_CODE(InternalError);
    MAP_CODE(FunctionNotImplemented);
    MAP_CODE(UnknownErrorCode);
    MAP_CODE(NodeShutdown);
    MAP_CODE(SchemaObjectExists);
    MAP_CODE(InternalTemporary);
  }
  return String::New("-unknown-");
}



Handle<Value> get_code(Local<String> property, const AccessorInfo &info) {
  const NdbError *err = unwrapPointer<const NdbError *>(info.Holder());
  
  return Integer::New(err->code);  
}


Handle<Value> get_mysql_code(Local<String> property, const AccessorInfo &info) {
  const NdbError *err = unwrapPointer<const NdbError *>(info.Holder());
  
  return Integer::New(err->mysql_code);
}


Handle<Value> get_message(Local<String> property, const AccessorInfo &info) {
  const NdbError *err = unwrapPointer<const NdbError *>(info.Holder());
  
  return String::New(err->message);
}

