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

#include <node.h>
#include <node_buffer.h>
#include <NdbApi.hpp>

#include "adapter_global.h"
#include "unified_debug.h"
#include "JsWrapper.h"
#include "NdbRecordObject.h"


NdbRecordObject::NdbRecordObject(Record *_record, 
                                 ColumnHandlerSet * _handlers,
                                 Handle<Value> jsBuffer) : 
  record(_record), 
  handlers(_handlers),
  ncol(record->getNoOfColumns()),
  proxy(new ColumnProxy[record->getNoOfColumns()])
{
  DEBUG_PRINT("    ___Constructor___       [%d col, bufsz %d]", 
              ncol, record->getBufferSize());

  /* Retain a handler on the buffer for our whole lifetime */
  persistentBufferHandle = Persistent<Value>::New(jsBuffer);
  buffer = node::Buffer::Data(jsBuffer->ToObject());  
  // You could assert here that buffer size == record buffer size

  /* Initialize the list of masked-in columns */
  resetMask();
  
  /* Attach the column proxies to their handlers */
  for(unsigned int i = 0 ; i < ncol ; i++)
    proxy[i].setHandler(handlers->getHandler(i));
}


NdbRecordObject::~NdbRecordObject() {
  DEBUG_PRINT(" << Destructor");
  persistentBufferHandle.Dispose();
  delete[] proxy;
}


Handle<Value> NdbRecordObject::getField(int nField) {
  if(record->isNull(nField, buffer))
    return Null();
  else
    return proxy[nField].get(buffer);
}


Handle<Value> NdbRecordObject::prepare() {
  HandleScope scope;
  int n = 0;
  Handle<Value> writeStatus;
  Handle<Value> savedError = Undefined();
  for(unsigned int i = 0 ; i < ncol ; i++) {
    if(isMaskedIn(i)) {
      n++;
      if(proxy[i].isNull) {
        record->setNull(i, buffer);
      }
      else {
        writeStatus = proxy[i].write(buffer);
        if(! writeStatus->IsUndefined()) savedError = writeStatus;
      }
    }
  }
  DEBUG_PRINT("Prepared %d column%s", n, (n == 1 ? "" : "s"));
  return scope.Close(savedError);
}

