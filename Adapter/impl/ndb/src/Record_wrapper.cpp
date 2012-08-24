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

#include "js_wrapper_macros.h"
#include "NativeMethodCall.h"
#include "unified_debug.h"
#include "JsWrapper.h"

#include "Record.h"

using namespace v8;

Handle<Value> getColumnOffset_wrapper(const Arguments &);
Handle<Value> getBufferSize_wrapper(const Arguments &);
Handle<Value> setNull_wrapper(const Arguments &);
Handle<Value> setNotNull_wrapper(const Arguments &);
Handle<Value> isNull_wrapper(const Arguments &);

class RecordEnvelopeClass : public Envelope {
public:
  RecordEnvelopeClass() : Envelope("Record") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "getColumnOffset", getColumnOffset_wrapper);
    DEFINE_JS_FUNCTION(Envelope::stencil, "getBufferSize", getBufferSize_wrapper);
    DEFINE_JS_FUNCTION(Envelope::stencil, "setNull", setNull_wrapper);
    DEFINE_JS_FUNCTION(Envelope::stencil, "setNotNull", setNotNull_wrapper);
    DEFINE_JS_FUNCTION(Envelope::stencil, "isNull", isNull_wrapper);
  }
};

RecordEnvelopeClass RecordEnvelope;


/****  CALL THIS FROM C++ CODE TO CREATE A WRAPPED RECORD OBJECT. 
*****/
Handle<Value> Record_Wrapper(Record *rec) {
  HandleScope scope;
  
  Local<Object> js_record = RecordEnvelope.newWrapper();
  wrapPointerInObject(rec, RecordEnvelope, js_record);
  return scope.Close(js_record);
}


Handle<Value> getColumnOffset_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);

  typedef NativeConstMethodCall_1_<size_t, Record, int> NCALL;

  NCALL ncall(args);
  ncall.method = & Record::getColumnOffset;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}


Handle<Value> getBufferSize_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);

  typedef NativeConstMethodCall_0_<size_t, Record> NCALL;

  NCALL ncall(args);
  ncall.method = & Record::getBufferSize;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}

// TODO:  the "char *" arg is a Buffer.  Verify that this works.
Handle<Value> setNull_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(2);

  typedef NativeConstVoidMethodCall_2_<Record, int, char *> NCALL;

  NCALL ncall(args);
  ncall.method = & Record::setNull;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> setNotNull_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(2);

  typedef NativeConstVoidMethodCall_2_<Record, int, char *> NCALL;

  NCALL ncall(args);
  ncall.method = & Record::setNotNull;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}


Handle<Value> isNull_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(2);

  typedef NativeConstMethodCall_2_<uint32_t, Record, int, char *> NCALL;

  NCALL ncall(args);
  ncall.method = & Record::isNull;
  ncall.run();
  
  return scope.Close(ncall.jsReturnVal());
}

