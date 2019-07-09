/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
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
