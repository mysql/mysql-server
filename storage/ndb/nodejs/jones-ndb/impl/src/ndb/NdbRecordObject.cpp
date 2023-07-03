/*
 Copyright (c) 2013, 2022, Oracle and/or its affiliates.
 
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

#include <node.h>
#include <NdbApi.hpp>

#include "adapter_global.h"
#include "JsWrapper.h"
#include "JsValueAccess.h"
#include "NdbRecordObject.h"
#include "BlobHandler.h"


NdbRecordObject::NdbRecordObject(const Record *_record, 
                                 ColumnHandlerSet * _handlers,
                                 const Arguments & args) :
  record(_record),
  handlers(_handlers),
  ncol(record->getNoOfColumns()),
  proxy(new ColumnProxy[record->getNoOfColumns()]),
  nWrites(0),
  isolate(args.GetIsolate())
{
  EscapableHandleScope scope(isolate);
  const Local<Object> jsBuffer = ToObject(isolate, args[0]);
  const Local<Value> & blobBuffers = args[1];

  unsigned int nblobs = 0;

  /* Retain a handle on the buffer for our whole lifetime */
  persistentBufferHandle.Reset(isolate, jsBuffer);
  buffer = GetBufferData(jsBuffer);

  /* Initialize the list of masked-in columns */
  resetMask();
  
  /* Attach the column proxies to their handlers */
  for(unsigned int i = 0 ; i < ncol ; i++)
    proxy[i].setHandler(handlers->getHandler(i));
  
  /* Attach BLOB buffers */
  if(blobBuffers->IsObject()) {
    Local<Object> blobBufferArray = ToObject(isolate, blobBuffers);
    for(unsigned int i = 0 ; i < ncol ; i++) {
      Local<Value> b = Get(isolate, blobBufferArray, i);
      if(b->IsObject()) {
        nblobs++;
        Local<Object> buf = ToObject(isolate, b);
        assert(IsJsBuffer(buf));
        proxy[i].setBlobBuffer(isolate, buf);
        record->setNotNull(i, buffer);
      } else if(b->IsNull()) {
       nblobs++;
       record->setNull(i, buffer);
      }
    }
  }
  DEBUG_PRINT("    ___Constructor___       [%d col, bufsz %d, %d blobs]", 
              ncol, record->getBufferSize(), nblobs);
  assert(nblobs == record->getNoOfBlobColumns());
  assert(GetBufferLength(jsBuffer) == record->getBufferSize());
}


NdbRecordObject::~NdbRecordObject() {
  DEBUG_PRINT(" << Destructor");
  persistentBufferHandle.Reset();
  delete[] proxy;
}


Local<Value> NdbRecordObject::getField(int nField) {
  if(record->isNull(nField, buffer))
    return Null(isolate);
  else
    return proxy[nField].get(isolate, buffer);
}


Local<Value> NdbRecordObject::prepare() {
  EscapableHandleScope scope(isolate);
  int n = 0;
  Local<Value> writeStatus;
  Local<Value> savedError = Undefined(isolate);
  for(unsigned int i = 0 ; i < ncol ; i++) {
    if(isMaskedIn(i)) {
      n++;
      if(proxy[i].valueIsNull()) {
        record->setNull(i, buffer);
      }
      else {
        writeStatus = proxy[i].write(isolate, buffer);
        if(! writeStatus->IsUndefined()) savedError = writeStatus;
      }
    }
  }
  DEBUG_PRINT("Prepared %d column%s. Mask %u.", n, (n == 1 ? "" : "s"), u.maskvalue);
  return scope.Escape(savedError);
}


int NdbRecordObject::createBlobWriteHandles(v8::Isolate *iso, KeyOperation &op)
{
  int ncreated = 0;
  for(unsigned int i = 0 ; i < ncol ; i++) {
    if(isMaskedIn(i)) {
      BlobWriteHandler * b = proxy[i].createBlobWriteHandle(iso, i);
      if(b) { 
        DEBUG_PRINT(" createBlobWriteHandles -- for column %d", i);
        op.setBlobHandler(b);
        ncreated++;
      }
    }
  }
  return ncreated;
}
