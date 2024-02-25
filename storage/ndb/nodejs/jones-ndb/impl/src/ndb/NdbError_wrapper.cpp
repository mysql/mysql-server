/*
 Copyright (c) 2013, 2023, Oracle and/or its affiliates.
 
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

/* NOTE

  JavaScript accessor methods are used to get properties from an NdbError;
  the NdbError must remain valid while accessed from JavaScript.

  After an NdbTransaction is closed, any reference to an NdbError from the
  NdbTransaction or its NdbOperations becomes invalid.

*/

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "Record.h"
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"


void get_status(Local<String>, const AccessorInfo &);
void get_classification(Local<String>, const AccessorInfo &);
void get_code(Local<String>, const AccessorInfo &);
void get_mysql_code(Local<String>, const AccessorInfo &);
void get_message(Local<String>, const AccessorInfo &);


class NdbErrorEnvelopeClass : public Envelope {
public:
  NdbErrorEnvelopeClass() : Envelope("NdbError") {
    addAccessor("status", get_status);
    addAccessor("classification", get_classification);
    addAccessor("code", get_code);
    addAccessor("handler_error_code", get_mysql_code);
    addAccessor("message", get_message);
  }
};

NdbErrorEnvelopeClass NdbErrorEnvelope;

Local<Value> NdbError_Wrapper(const NdbError &err) {
  return NdbErrorEnvelope.wrap(& err);
}

#define V8STRING(MSG) scope.Escape(NewUtf8String(info.GetIsolate(), MSG))

#define V8INTEGER(V) scope.Escape(Integer::New(info.GetIsolate(), V))

#define MAP_CODE(CODE) \
  case NdbError::CODE: \
    info.GetReturnValue().Set(V8STRING(#CODE)); \
    return;


void get_status(Local<String> property, const AccessorInfo &info) {
  EscapableHandleScope scope(info.GetIsolate());
  const NdbError *err = unwrapPointer<const NdbError *>(info.Holder());
  
  switch(err->status) {
    MAP_CODE(Success);
    MAP_CODE(TemporaryError);
    MAP_CODE(PermanentError);
    MAP_CODE(UnknownResult);  
  }
  info.GetReturnValue().Set(V8STRING("-unknown-"));
}


void get_classification(Local<String> property, const AccessorInfo &info) {
  EscapableHandleScope scope(info.GetIsolate());
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
  info.GetReturnValue().Set(V8STRING("-unknown-"));
}

void get_code(Local<String> property, const AccessorInfo &info) {
  EscapableHandleScope scope(info.GetIsolate());
  const NdbError *err = unwrapPointer<const NdbError *>(info.Holder());

  info.GetReturnValue().Set(V8INTEGER(err->code));
}

void get_mysql_code(Local<String> property, const AccessorInfo &info) {
  EscapableHandleScope scope(info.GetIsolate());
  const NdbError *err = unwrapPointer<const NdbError *>(info.Holder());
  
  info.GetReturnValue().Set(V8INTEGER(err->mysql_code));
}

void get_message(Local<String> property, const AccessorInfo &info) {
  EscapableHandleScope scope(info.GetIsolate());
  const NdbError *err = unwrapPointer<const NdbError *>(info.Holder());

  info.GetReturnValue().Set(V8STRING(err->message));
}

