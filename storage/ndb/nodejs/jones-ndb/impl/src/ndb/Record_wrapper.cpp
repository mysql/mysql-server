/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
 
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
#include "Record.h"
#include "NativeMethodCall.h"

#include "NdbTypeEncoders.h"

using namespace v8;

V8WrapperFn getColumnOffset_wrapper,
            getBufferSize_wrapper,
            setNull_wrapper,
            setNotNull_wrapper,
            isNull_wrapper,
            record_encoderRead,
            record_encoderWrite;

class RecordEnvelopeClass : public Envelope {
public:
  RecordEnvelopeClass() : Envelope("const Record") {
    addMethod("getColumnOffset", getColumnOffset_wrapper);
    addMethod("getBufferSize", getBufferSize_wrapper);
    addMethod("setNull", setNull_wrapper);
    addMethod("isNull", isNull_wrapper);
    addMethod("encoderRead", record_encoderRead);
    addMethod("encoderWrite", record_encoderWrite);
  }
};

RecordEnvelopeClass RecordEnvelope;


/****  CALL THIS FROM C++ CODE TO CREATE A WRAPPED RECORD OBJECT. 
*****/
Local<Value> Record_Wrapper(const Record *rec) {
  Local<Value> js_record = RecordEnvelope.wrap(rec);
  return js_record;
}


void getColumnOffset_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  
  REQUIRE_ARGS_LENGTH(1);

  typedef NativeConstMethodCall_1_<uint32_t, const Record, int> NCALL;

  NCALL ncall(& Record::getColumnOffset, args);
  ncall.run();

  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}


void getBufferSize_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  
  REQUIRE_ARGS_LENGTH(0);

  typedef NativeConstMethodCall_0_<uint32_t, const Record> NCALL;

  NCALL ncall(& Record::getBufferSize, args);
  ncall.run();
  
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void setNull_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  EscapableHandleScope scope(args.GetIsolate());
  
  REQUIRE_ARGS_LENGTH(2);

  typedef NativeVoidConstMethodCall_2_<const Record, int, char *> NCALL;

  NCALL ncall(& Record::setNull, args);
  ncall.run();
  
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void isNull_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  
  REQUIRE_ARGS_LENGTH(2);

  typedef NativeConstMethodCall_2_<uint32_t, const Record, int, char *> NCALL;

  NCALL ncall(& Record::isNull, args);
  ncall.run();
  
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}


/* read(columnNumber, buffer)
*/
void record_encoderRead(const Arguments & args) {
  EscapableHandleScope scope(args.GetIsolate());
  const Record * record = unwrapPointer<Record *>(args.Holder());
  int columnNumber = args[0]->Uint32Value();
  char * buffer = node::Buffer::Data(args[1]->ToObject());

  const NdbDictionary::Column * col = record->getColumn(columnNumber);
  uint32_t offset = record->getColumnOffset(columnNumber);

  const NdbTypeEncoder * encoder = getEncoderForColumn(col);
  Local<Value> read = encoder->read(col, buffer, offset);

  args.GetReturnValue().Set(scope.Escape(read));
}


/* write(columnNumber, buffer, value)
*/
void record_encoderWrite(const Arguments & args) {
  EscapableHandleScope scope(args.GetIsolate());

  const Record * record = unwrapPointer<const Record *>(args.Holder());
  int columnNumber = args[0]->Uint32Value();
  char * buffer = node::Buffer::Data(args[1]->ToObject());

  record->setNotNull(columnNumber, buffer);

  const NdbDictionary::Column * col = record->getColumn(columnNumber);
  uint32_t offset = record->getColumnOffset(columnNumber);

  const NdbTypeEncoder * encoder = getEncoderForColumn(col);
  Local<Value> error = encoder->write(col, args[2], buffer, offset);

  args.GetReturnValue().Set(scope.Escape(error));
}

