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

#include "adapter_global.h"
#include "unified_debug.h"
#include "ColumnProxy.h"
#include "JsWrapper.h"

using namespace v8;

Handle<Value> ColumnProxy::get(v8::Isolate *isolate, char *buffer) {
  Handle<Value> val = ToLocal(& jsValue);

  if(! isLoaded) {
    val = handler->read(buffer, ToLocal(& blobBuffer));
    jsValue.Reset(isolate, val);
    isLoaded = true;
  }
  return val;
}

void ColumnProxy::set(v8::Isolate *isolate, Handle<Value> newValue) {
  isNull = (newValue->IsNull());
  isLoaded = isDirty = true;
  jsValue.Reset(isolate, newValue);
  DEBUG_PRINT("set %s", handler->column->getName());
}

Handle<Value> ColumnProxy::write(v8::Isolate *isolate, char *buffer) {
  Handle<Value> rval = Undefined(isolate);

  if(isDirty && ! (handler->isBlob())) {
    rval = handler->write(ToLocal(& jsValue), buffer);
  }
  isDirty = false;

  return rval;
}

BlobWriteHandler * ColumnProxy::createBlobWriteHandle(int i) {
  return isNull ? 0 : handler->createBlobWriteHandle(ToLocal(& jsValue), i);
}

