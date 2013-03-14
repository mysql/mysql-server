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

#include <string.h>

#include <node.h>

#include "adapter_global.h"
#include "Operation.h"
#include "JsWrapper.h"
#include "v8_binder.h"
#include "js_wrapper_macros.h"
#include "NdbWrappers.h"


/* DBOperationHelper() takes one argument, a fixed-length array.
   A new Operation is created.
   Keys in the array are used to set fields of the Operation.
   The Operation is returned.
*/

enum {
  HELPER_ROW_BUFFER = 0,
  HELPER_KEY_BUFFER,
  HELPER_ROW_RECORD,
  HELPER_KEY_RECORD,
  HELPER_LOCK_MODE,
  HELPER_COLUMN_MASK,
  // TODO: Options
  HELPER_ARRAY_SIZE    /* the last element */
};


Handle<Value> DBOperationHelper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;

  Operation *op = new Operation;

  const Local<Object> obj = args[0]->ToObject();
  Local<Object> v;
  
  if(obj->Has(HELPER_ROW_BUFFER)) {
    DEBUG_PRINT("Setting operation row_buffer");
    v = obj->Get(HELPER_ROW_BUFFER)->ToObject();
    op->row_buffer = V8BINDER_UNWRAP_BUFFER(v);
  }
  
  if(obj->Has(HELPER_KEY_BUFFER)) {
    DEBUG_PRINT("Setting operation key_buffer");
    v = obj->Get(HELPER_KEY_BUFFER)->ToObject();
    op->key_buffer = V8BINDER_UNWRAP_BUFFER(v);
  }
  
  if(obj->Has(HELPER_ROW_RECORD)) {
    DEBUG_PRINT("Setting operation row_record");
    v = obj->Get(HELPER_ROW_RECORD)->ToObject();
    op->row_record = unwrapPointer<Record *>(v);
  }
  
  if(obj->Has(HELPER_KEY_RECORD)) {
    DEBUG_PRINT("Setting operation key_record");
    v = obj->Get(HELPER_KEY_RECORD)->ToObject();
    op->key_record = unwrapPointer<Record *>(v);
  }
  
  if(obj->Has(HELPER_LOCK_MODE)) {
    DEBUG_PRINT("Setting operation lock_mode");
    op->lmode = static_cast<NdbOperation::LockMode> 
      (obj->Get(HELPER_LOCK_MODE)->Int32Value());
  }
  if(obj->Has(HELPER_COLUMN_MASK)) {
    DEBUG_PRINT("Setting operation column mask");
    Array *maskArray = Array::Cast( * (obj->Get(HELPER_COLUMN_MASK)));
    for(unsigned int m = 0 ; m < maskArray->Length() ; m++) {
      Local<Value> colId = maskArray->Get(m);
      op->useColumn(colId->Int32Value());
    }
  }
  
  return scope.Close(Operation_Wrapper(op));
}


void DBOperationHelper_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  DEBUG_MARKER(UDEB_DETAIL);
  DEFINE_JS_FUNCTION(target, "DBOperationHelper", DBOperationHelper);
  DEFINE_JS_CONSTANT(target, HELPER_ROW_BUFFER);
  DEFINE_JS_CONSTANT(target, HELPER_KEY_BUFFER);
  DEFINE_JS_CONSTANT(target, HELPER_ROW_RECORD);
  DEFINE_JS_CONSTANT(target, HELPER_KEY_RECORD);
  DEFINE_JS_CONSTANT(target, HELPER_LOCK_MODE);
  DEFINE_JS_CONSTANT(target, HELPER_COLUMN_MASK);
  DEFINE_JS_CONSTANT(target, HELPER_ARRAY_SIZE);
  Persistent<Object> LockModes = Persistent<Object>(Object::New());
  target->Set(Persistent<String>(String::NewSymbol("LockModes")), LockModes);
  DEFINE_JS_INT(LockModes, "EXCLUSIVE", NdbOperation::LM_Exclusive);
  DEFINE_JS_INT(LockModes, "SHARED", NdbOperation::LM_Read);
  DEFINE_JS_INT(LockModes, "SHARED_RELEASED", NdbOperation::LM_SimpleRead);
  DEFINE_JS_INT(LockModes, "COMMITTED", NdbOperation::LM_CommittedRead);
}

