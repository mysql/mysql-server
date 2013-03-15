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

enum {
  HELPER_ROW_BUFFER = 0,
  HELPER_KEY_BUFFER,
  HELPER_ROW_RECORD,
  HELPER_KEY_RECORD,
  HELPER_LOCK_MODE,
  HELPER_COLUMN_MASK,
};

/* DBOperationHelper() is the C++ companion to DBOperation.prepare
   in NdbOperation.js
   It takes a HelperSpec object, and returns a fully-prepared Operation
*/
   
Handle<Value> DBOperationHelper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;

  Operation *op = new Operation;

  for(int i = 0 ; i < args.Length() ; i++) {
    if(args[i]->IsObject()) {
      const Local<Object> obj = args[i]->ToObject();
      Local<Value> v;
      Local<Object> o;
      
      v = obj->Get(HELPER_ROW_BUFFER);
      if(! v->IsNull()) {
        o = v->ToObject();
        DEBUG_PRINT("Setting operation row_buffer");
        op->row_buffer = V8BINDER_UNWRAP_BUFFER(o);
      }
      
      v = obj->Get(HELPER_KEY_BUFFER);
      if(! v->IsNull()) {
        o = v->ToObject();
        DEBUG_PRINT("Setting operation key_buffer");
        op->key_buffer = V8BINDER_UNWRAP_BUFFER(o);
      }
      
      v = obj->Get(HELPER_ROW_RECORD);
      if(! v->IsNull()) {
        o = v->ToObject();
        DEBUG_PRINT("Setting operation row_record");
        op->row_record = unwrapPointer<Record *>(o);
      }
      
      v = obj->Get(HELPER_KEY_RECORD);
      if(! v->IsNull()) {
        o = v->ToObject();
        DEBUG_PRINT("Setting operation key_record");
        op->key_record = unwrapPointer<Record *>(o);
      }

      v = obj->Get(HELPER_LOCK_MODE);
      if(! v->IsNull()) {
        int intLockMode = v->Int32Value();
        DEBUG_PRINT("Setting operation lock_mode");
        op->lmode = static_cast<NdbOperation::LockMode>(intLockMode);
      }

      v = obj->Get(HELPER_COLUMN_MASK);
      if(! v->IsNull()) {
        Array *maskArray = Array::Cast(*v);
        DEBUG_PRINT("Setting operation column mask");
        for(unsigned int m = 0 ; m < maskArray->Length() ; m++) {
          Local<Value> colId = maskArray->Get(m);
          op->useColumn(colId->Int32Value());
        }
      }
    }    
  }
  
  return scope.Close(Operation_Wrapper(op));
}


void DBOperationHelper_initOnLoad(Handle<Object> target) {
  DEBUG_MARKER(UDEB_DETAIL);
  DEFINE_JS_FUNCTION(target, "DBOperationHelper", DBOperationHelper);

  Persistent<Object> OpHelper = Persistent<Object>(Object::New());
  target->Set(Persistent<String>(String::NewSymbol("OpHelper")), OpHelper);
  DEFINE_JS_INT(OpHelper, "row_buffer",  HELPER_ROW_BUFFER);
  DEFINE_JS_INT(OpHelper, "key_buffer",  HELPER_KEY_BUFFER);
  DEFINE_JS_INT(OpHelper, "row_record",  HELPER_ROW_RECORD);
  DEFINE_JS_INT(OpHelper, "key_record",  HELPER_KEY_RECORD);
  DEFINE_JS_INT(OpHelper, "lock_mode",   HELPER_LOCK_MODE);
  DEFINE_JS_INT(OpHelper, "column_mask", HELPER_COLUMN_MASK);

  Persistent<Object> LockModes = Persistent<Object>(Object::New());
  target->Set(Persistent<String>(String::NewSymbol("LockModes")), LockModes);
  DEFINE_JS_INT(LockModes, "EXCLUSIVE", NdbOperation::LM_Exclusive);
  DEFINE_JS_INT(LockModes, "SHARED", NdbOperation::LM_Read);
  DEFINE_JS_INT(LockModes, "SHARED_RELEASED", NdbOperation::LM_SimpleRead);
  DEFINE_JS_INT(LockModes, "COMMITTED", NdbOperation::LM_CommittedRead);
}

