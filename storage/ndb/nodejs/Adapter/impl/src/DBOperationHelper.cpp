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
#include "NdbWrappers.h"
#include "v8_binder.h"
#include "js_wrapper_macros.h"
#include "NdbRecordObject.h"

enum {
  HELPER_ROW_BUFFER = 0,
  HELPER_KEY_BUFFER,
  HELPER_ROW_RECORD,
  HELPER_KEY_RECORD,
  HELPER_LOCK_MODE,
  HELPER_COLUMN_MASK,
  HELPER_VALUE_OBJECT
};


Handle<Value> DBOperationHelper_VO(const Arguments &);
Handle<Value> DBOperationHelper_NonVO(const Arguments &);
Handle<Value> buildNdbOperation(Operation &, int, NdbTransaction *);
void setKeysInOp(Handle<Object> spec, Operation & op);


/* DBOperationHelper() is the C++ companion to DBOperation.prepare
   in NdbOperation.js
   It takes a HelperSpec object, and returns a fully-prepared Operation
   arg0: DBOperation HelperSpec
   arg1: opcode
   arg2: NdbTransaction *
   arg3: boolean isValueObject
*/

Handle<Value> DBOperationHelper(const Arguments &args) {
  return
    args[3]->ToBoolean()->Value() ?
      DBOperationHelper_VO(args) : DBOperationHelper_NonVO(args);
}


void setKeysInOp(Handle<Object> spec, Operation & op) {
  HandleScope scope;

  Local<Value> v;
  Local<Object> o;

  v = spec->Get(HELPER_KEY_BUFFER);
  if(! v->IsNull()) {
    o = v->ToObject();
    op.key_buffer = V8BINDER_UNWRAP_BUFFER(o);
  }
  
  v = spec->Get(HELPER_KEY_RECORD);
  if(! v->IsNull()) {
    o = v->ToObject();
    op.key_record = unwrapPointer<Record *>(o);
  }
}


Handle<Value> buildNdbOperation(Operation &op, int opcode, NdbTransaction *tx) {
  const NdbOperation * ndbop;
    
  switch(opcode) {
    case 1:   // OP_READ:
      ndbop = op.readTuple(tx);
      break;
    case 2:  // OP_INSERT:
      ndbop = op.insertTuple(tx);
      break;
    case 4:  // OP_UPDATE:
      ndbop = op.updateTuple(tx);
      break;
    case 8:  // OP_WRITE:
      ndbop = op.writeTuple(tx);
      break;
    case 16:  // OP_DELETE:
      ndbop = op.deleteTuple(tx);
      break;
    default:
      assert("Unhandled opcode" == 0);
      return Undefined();
  }

  return NdbOperation_Wrapper(ndbop);
}


Handle<Value> DBOperationHelper_NonVO(const Arguments &args) {
  HandleScope scope;
  Operation op;

  const Local<Object> spec = args[0]->ToObject();
  Local<Value> v;
  Local<Object> o;

  setKeysInOp(spec, op);
  
  v = spec->Get(HELPER_ROW_BUFFER);
  if(! v->IsNull()) {
    o = v->ToObject();
    op.row_buffer = V8BINDER_UNWRAP_BUFFER(o);
  }
  
  v = spec->Get(HELPER_ROW_RECORD);
  if(! v->IsNull()) {
    o = v->ToObject();
    op.row_record = unwrapPointer<Record *>(o);
  }
  
  v = spec->Get(HELPER_LOCK_MODE);
  if(! v->IsNull()) {
    int intLockMode = v->Int32Value();
    op.lmode = static_cast<NdbOperation::LockMode>(intLockMode);
  }

  v = spec->Get(HELPER_COLUMN_MASK);
  if(! v->IsNull()) {
    Array *maskArray = Array::Cast(*v);
    for(unsigned int m = 0 ; m < maskArray->Length() ; m++) {
      Local<Value> colId = maskArray->Get(m);
      op.useColumn(colId->Int32Value());
    }
  }
  
  int opcode = args[1]->Int32Value();
  NdbTransaction *tx = unwrapPointer<NdbTransaction *>(args[2]->ToObject());
  DEBUG_PRINT("Non-VO opcode: %d mask: %u", opcode, op.u.maskvalue);

  return scope.Close(buildNdbOperation(op, opcode, tx));
}

Handle<Value> DBOperationHelper_VO(const Arguments &args) {
  HandleScope scope;
  Local<Value> v;
  Local<Object> o;
  Local<Object> valueObj;
  Handle<Value> returnVal;
  Operation op;

  const Local<Object> spec = args[0]->ToObject();
  int opcode = args[1]->Int32Value();
  NdbTransaction *tx = unwrapPointer<NdbTransaction *>(args[2]->ToObject());

  /* NdbOperation.prepare() just verified that this is really a VO */
  v = spec->Get(HELPER_VALUE_OBJECT);
  valueObj = v->ToObject();
  NdbRecordObject * nro = unwrapPointer<NdbRecordObject *>(valueObj);

  /* The VO may have values that are not yet encoded to its buffer. */
  returnVal = nro->prepare();  // do something with this?
  
  /* Set the key record and key buffer from the helper spec */
  setKeysInOp(spec, op);
  
  /* Set the row record, row buffer, and mask from the VO */
  op.row_record = nro->getRecord();
  op.row_buffer = nro->getBuffer();

  /* "write" and "persist" must write all columns. 
     Other operations only require the columns that have changed since read.
  */
  if(opcode == 2 || opcode == 8) 
    op.setRowMask(0xFFFFFFFF);
  else 
    op.setRowMask(nro->getMaskValue());

  DEBUG_PRINT("  VO   opcode: %d mask: %u", opcode, op.u.maskvalue);

  returnVal = buildNdbOperation(op, opcode, tx);
  
  nro->resetMask(); 

  return scope.Close(returnVal);
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
  DEFINE_JS_INT(OpHelper, "value_obj",   HELPER_VALUE_OBJECT);

  Persistent<Object> LockModes = Persistent<Object>(Object::New());
  target->Set(Persistent<String>(String::NewSymbol("LockModes")), LockModes);
  DEFINE_JS_INT(LockModes, "EXCLUSIVE", NdbOperation::LM_Exclusive);
  DEFINE_JS_INT(LockModes, "SHARED", NdbOperation::LM_Read);
  DEFINE_JS_INT(LockModes, "SHARED_RELEASED", NdbOperation::LM_SimpleRead);
  DEFINE_JS_INT(LockModes, "COMMITTED", NdbOperation::LM_CommittedRead);
}

