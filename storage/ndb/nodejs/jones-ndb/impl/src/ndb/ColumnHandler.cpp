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

#include "ColumnHandler.h"
#include "BlobHandler.h"
#include "JsWrapper.h"
#include "adapter_global.h"
#include "js_wrapper_macros.h"

ColumnHandler::ColumnHandler()
    : column(0), offset(0), isLob(false), isText(false) {}

ColumnHandler::~ColumnHandler() {
  // Persistent handles will be disposed by calling of their destructors
}

void ColumnHandler::init(v8::Isolate *_isolate,
                         const NdbDictionary::Column *_column,
                         uint32_t _offset) {
  column = _column;
  encoder = getEncoderForColumn(column);
  offset = _offset;
  isolate = _isolate;

  switch (column->getType()) {
    case NDB_TYPE_TEXT:
      isText = true;  // fall through to also set isLob
      [[fallthrough]];
    case NDB_TYPE_BLOB:
      isLob = true;
      break;
    default:
      break;
  }
}

Local<Value> ColumnHandler::read(char *rowBuffer,
                                 Local<Object> blobBuffer) const {
  Local<Value> val;  // HandleScope is in ValueObject.cpp nroGetter

  if (isText) {
    DEBUG_PRINT("text read");
    val = getTextFromBuffer(column, blobBuffer);
  } else if (isLob) {
    DEBUG_PRINT("blob read");
    val = Local<Value>(blobBuffer);
  } else {
    val = encoder->read(column, rowBuffer, offset);
  }
  return val;
}

// If column is a blob, val is the blob buffer
Local<Value> ColumnHandler::write(Local<Value> val, char *buffer) const {
  DEBUG_PRINT("write %s", column->getName());
  return encoder->write(column, val, buffer, offset);
}

inline Local<Object> asObject(Local<Value> val, v8::Isolate *isolate) {
  return val->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
}

inline Local<Object> asText(const NdbDictionary::Column *c, Local<Value> val,
                            v8::Isolate *isolate) {
  return getBufferForText(
      c, val->ToString(isolate->GetCurrentContext()).ToLocalChecked());
}

BlobWriteHandler *ColumnHandler::createBlobWriteHandle(Local<Value> val,
                                                       int fieldNo) const {
  DEBUG_MARKER(UDEB_DETAIL);
  BlobWriteHandler *b = 0;
  Local<Object> nodeBuffer;
  if (isLob) {
    nodeBuffer = (isText && val->IsString()) ? asText(column, val, isolate)
                                             : asObject(val, isolate);
    b = new BlobWriteHandler(column->getColumnNo(), fieldNo, nodeBuffer);
  }
  return b;
}
