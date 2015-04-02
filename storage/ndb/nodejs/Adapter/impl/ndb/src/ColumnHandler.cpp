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

#include "adapter_global.h"
#include "ColumnHandler.h"
#include "BlobHandler.h"

using namespace v8;

class Keys {
public:
  Persistent<String> toDB;
  Persistent<String> fromDB;
  Keys() {
    HandleScope scope;
    toDB = Persistent<String>::New(String::NewSymbol("toDB"));
    fromDB = Persistent<String>::New(String::NewSymbol("fromDB"));
  }
};

Keys keys;

ColumnHandler::ColumnHandler() :
  column(0), offset(0), 
  converterClass(), converterReader(), converterWriter(),
  hasConverterReader(false), hasConverterWriter(false),
  isLob(false), isText(false)
{
}


ColumnHandler::~ColumnHandler() {
  if(! converterClass.IsEmpty()) converterClass.Dispose();
  if(hasConverterReader) converterReader.Dispose();
  if(hasConverterWriter) converterWriter.Dispose();
}

void ColumnHandler::init(const NdbDictionary::Column *_column,
                         size_t _offset,
                         Handle<Value> typeConverter) {
  HandleScope scope;
  column = _column;
  encoder = getEncoderForColumn(column);
  offset = _offset;
  Local<Object> t;

  switch(column->getType()) {
    case NDB_TYPE_TEXT: 
      isText = true;   // fall through to also set isLob
    case NDB_TYPE_BLOB:
      isLob = true;
      break;
    default:
      break;
  }

  if(typeConverter->IsObject()) {
    converterClass = Persistent<Object>::New(typeConverter->ToObject());

    if(converterClass->Has(keys.toDB)) {
      t = converterClass->Get(keys.toDB)->ToObject();
      if(t->IsFunction()) {
        converterWriter = Persistent<Object>::New(t);
        hasConverterWriter = true;
      }
    }

    if(converterClass->Has(keys.fromDB)) {
      t = converterClass->Get(keys.fromDB)->ToObject();
      if(t->IsFunction()) {
        converterReader = Persistent<Object>::New(t);
        hasConverterReader = true;
      }
    }
  }
}


Handle<Value> ColumnHandler::read(char * rowBuffer, Handle<Object> blobBuffer) const {
  HandleScope scope;
  Handle<Value> val;

  if(isText) {
    DEBUG_PRINT("text read");
    val = getTextFromBuffer(column, blobBuffer);
  } else if(isLob) {
    DEBUG_PRINT("blob read");
    val = Handle<Value>(blobBuffer);
  } else {
    val = encoder->read(column, rowBuffer, offset);
  }

  if(hasConverterReader) {
    TryCatch tc;
    Handle<Value> arguments[1];
    arguments[0] = val;
    val = converterReader->CallAsFunction(converterClass, 1, arguments);
    if(tc.HasCaught()) tc.ReThrow();
  }
  return scope.Close(val);
}


Handle<Value> ColumnHandler::write(Handle<Value> val, char *buffer) const {
  HandleScope scope;
  Handle<Value> writeStatus;

  DEBUG_PRINT("write %s", column->getName());
  if(hasConverterWriter) {
    TryCatch tc;
    Handle<Value> arguments[1];
    arguments[0] = val;
    val = converterWriter->CallAsFunction(converterClass, 1, arguments);
    if(tc.HasCaught())
      return scope.Close(tc.Exception());
   }
  
  writeStatus = encoder->write(column, val, buffer, offset);
  return scope.Close(writeStatus);
}


BlobWriteHandler * ColumnHandler::createBlobWriteHandle(Handle<Value> val, 
                                                        int fieldNo) const {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  BlobWriteHandler * b = 0;
  Handle<Object> obj = val->ToObject();
  if(isLob) {
    if(isText && val->IsString()) {
      obj = getBufferForText(column, val->ToString());
    }
    b = new BlobWriteHandler(column->getColumnNo(), fieldNo, obj);
  }
  return b;
}

