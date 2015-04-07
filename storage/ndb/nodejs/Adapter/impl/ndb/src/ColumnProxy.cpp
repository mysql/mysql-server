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
#include "unified_debug.h"
#include "ColumnProxy.h"

using namespace v8;

ColumnProxy::~ColumnProxy() {
  Dispose();
}

/* Drop our claim on the old value */
void ColumnProxy::Dispose() {
  if(! jsValue.IsEmpty()) jsValue.Dispose();
  if(! blobBuffer.IsEmpty()) blobBuffer.Dispose();
}

Handle<Value> ColumnProxy::get(char *buffer) {
  HandleScope scope;
  
  if(! isLoaded) {
    Handle<Value> val = handler->read(buffer, blobBuffer);
    jsValue = Persistent<Value>::New(val);
    isLoaded = true;
  }
  return scope.Close(jsValue);
}

void ColumnProxy::set(Handle<Value> newValue) {
  Dispose();
  isNull = (newValue->IsNull());
  isLoaded = isDirty = true;
  jsValue = Persistent<Value>::New(newValue);
  DEBUG_PRINT("set %s", handler->column->getName());
}


Handle<Value> ColumnProxy::write(char *buffer) {
  HandleScope scope;
  Handle<Value> rval = Undefined();

  /* Write dirty, non-blob values */
  if(isDirty && blobBuffer.IsEmpty()) {
    rval = handler->write(jsValue, buffer);
    DEBUG_PRINT("write %s", handler->column->getName());
    isDirty = false;
  }
  return scope.Close(rval);
}


BlobWriteHandler * ColumnProxy::createBlobWriteHandle(int i) {
  BlobWriteHandler * b = 0;
  if(isDirty && ! isNull) {
    DEBUG_PRINT("createBlobWriteHandle %s", handler->column->getName());
    b = handler->createBlobWriteHandle(blobBuffer, i);
  }
  isDirty = false;
  return b;
}

void ColumnProxy::setBlobBuffer(Handle<Object> buffer) {
  blobBuffer = Persistent<Object>::New(buffer);
}
