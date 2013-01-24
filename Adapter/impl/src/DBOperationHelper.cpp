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

#include "v8.h"

#include "adapter_global.h"
#include "Operation.h"
#include "JsWrapper.h"
#include "v8_binder.h"
#include "js_wrapper_macros.h"
#include "NdbWrappers.h"


/* DBOperationHelper() takes any number of Object arguments.
   A new Operation is created.
   Keys in the arguments are used to set fields of the Operation.
   The Operation is returned.
*/
   
Handle<Value> DBOperationHelper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;

  Operation *op = new Operation;

  Local<String> K_row_buffer = String::New("row_buffer");
  Local<String> K_key_buffer = String::New("key_buffer");
  Local<String> K_row_record = String::New("row_record");
  Local<String> K_key_record = String::New("key_record");
  Local<String> K_lock_mode  = String::New("lock_mode");
  Local<String> K_mask       = String::New("mask");

// TODO: Options


  for(int i = 0 ; i < args.Length() ; i++) {
    if(args[i]->IsObject()) {
      const Local<Object> obj = args[i]->ToObject();
      Local<Object> v;
      
      if(obj->Has(K_row_buffer)) {
        DEBUG_PRINT("Setting operation row_buffer");
        v = obj->Get(K_row_buffer)->ToObject();
        op->row_buffer = V8BINDER_UNWRAP_BUFFER(v);
      }
      
      if(obj->Has(K_key_buffer)) {
        DEBUG_PRINT("Setting operation key_buffer");
        v = obj->Get(K_key_buffer)->ToObject();
        op->key_buffer = V8BINDER_UNWRAP_BUFFER(v);
      }
      
      if(obj->Has(K_row_record)) {
        DEBUG_PRINT("Setting operation row_record");
        v = obj->Get(K_row_record)->ToObject();
        op->row_record = unwrapPointer<Record *>(v);
      }
      
      if(obj->Has(K_key_record)) {
        DEBUG_PRINT("Setting operation key_record");
        v = obj->Get(K_key_record)->ToObject();
        op->key_record = unwrapPointer<Record *>(v);
      }
      
      if(obj->Has(K_lock_mode)) {
        DEBUG_PRINT("Setting operation lock_mode");
        char lock_mode_string[40];
        NdbOperation::LockMode lmode = NdbOperation::LM_SimpleRead;

        Local<String> m = obj->Get(K_lock_mode)->ToString();
        m->WriteAscii(lock_mode_string, 0, 40);

        if(strcasecmp(lock_mode_string, "EXCLUSIVE") == 0) 
          lmode = NdbOperation::LM_Exclusive;
        else if(strcasecmp(lock_mode_string, "SHARED") == 0)
          lmode = NdbOperation::LM_Read;
        else if(strcasecmp(lock_mode_string, "COMMITTED") == 0)
          lmode = NdbOperation::LM_CommittedRead;
        
        op->lmode = lmode;
      }      
      if(obj->Has(K_mask)) {
        DEBUG_PRINT("Setting operation column mask");
        Array *maskArray = Array::Cast( * (obj->Get(K_mask)));
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
}

