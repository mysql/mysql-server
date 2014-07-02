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

#include <string.h>

#include <node.h>

#include "adapter_global.h"
#include "Operation.h"
#include "NdbWrappers.h"
#include "v8_binder.h"
#include "js_wrapper_macros.h"
#include "NdbRecordObject.h"
#include "PendingOperationSet.h"

enum {
  HELPER_ROW_BUFFER = 0,
  HELPER_KEY_BUFFER,
  HELPER_ROW_RECORD,
  HELPER_KEY_RECORD,
  HELPER_LOCK_MODE,
  HELPER_COLUMN_MASK,
  HELPER_VALUE_OBJECT,
  HELPER_OPCODE,
  HELPER_IS_VO,
  HELPER_IS_VALID
};

const NdbOperation * DBOperationHelper_VO(Handle<Object>, int, NdbTransaction *);
const NdbOperation * DBOperationHelper_NonVO(Handle<Object>, int, NdbTransaction *);
const NdbOperation * buildNdbOperation(Operation &, int, NdbTransaction *);

void setKeysInOp(Handle<Object> spec, Operation & op);


/* DBOperationHelper takes an array of HelperSpecs.
   arg0: Length of Array
   arg1: Array of HelperSpecs
   arg2: NdbTransaction *

   Returns: a wrapped PendingOperationSet
   The set has the same length as the array that came in
*/
Handle<Value> DBOperationHelper(const Arguments &args) {
  HandleScope scope;

  int length = args[0]->Int32Value();
  const Local<Object> array = args[1]->ToObject();
  NdbTransaction *tx = unwrapPointer<NdbTransaction *>(args[2]->ToObject());

  PendingOperationSet * opList = new PendingOperationSet(length);

  for(int i = 0 ; i < length ; i++) {
    Handle<Object> spec = array->Get(i)->ToObject();

    int opcode  = spec->Get(HELPER_OPCODE)->Int32Value();
    bool is_vo  = spec->Get(HELPER_IS_VO)->ToBoolean()->Value();
    bool op_ok  = spec->Get(HELPER_IS_VALID)->ToBoolean()->Value();

    const NdbOperation * op = NULL;

    if(op_ok) {
      op = is_vo ?
        DBOperationHelper_VO(spec, opcode, tx):
        DBOperationHelper_NonVO(spec, opcode, tx);

      if(op)    opList->setNdbOperation(i, op);
      else      opList->setError(i, tx->getNdbError());
    }
    else {
      opList->setNdbOperation(i, NULL);
    }
  }

  return PendingOperationSet_Wrapper(opList);
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
    op.key_record = unwrapPointer<const Record *>(o);
  }
}


const NdbOperation * buildNdbOperation(Operation &op,
                                       int opcode, NdbTransaction *tx) {
  const NdbOperation * ndbop;
    
  switch(opcode) {
    case 1:  // OP_READ:
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
    case 16: // OP_DELETE:
      ndbop = op.deleteTuple(tx);
      break;
    default:
      assert("Unhandled opcode" == 0);
      return NULL;
  }

  return ndbop;
}


const NdbOperation * DBOperationHelper_NonVO(Handle<Object> spec, int opcode,
                                             NdbTransaction *tx) {
  HandleScope scope;
  Operation op;

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
    op.row_record = unwrapPointer<const Record *>(o);
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
  
  DEBUG_PRINT("Non-VO opcode: %d mask: %u", opcode, op.u.maskvalue);

  return buildNdbOperation(op, opcode, tx);
}

const NdbOperation * DBOperationHelper_VO(Handle<Object> spec, int opcode,
                                          NdbTransaction *tx) {
  HandleScope scope;
  Local<Value> v;
  Local<Object> o;
  Local<Object> valueObj;
  const NdbOperation * ndbOp;
  Operation op;

  /* NdbOperation.prepare() just verified that this is really a VO */
  v = spec->Get(HELPER_VALUE_OBJECT);
  valueObj = v->ToObject();
  NdbRecordObject * nro = unwrapPointer<NdbRecordObject *>(valueObj);

  /* The VO may have values that are not yet encoded to its buffer. */
  nro->prepare();  // FIXME: prepare() could return an error
  
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

  ndbOp = buildNdbOperation(op, opcode, tx);
  
  nro->resetMask(); 

  return ndbOp;
}


void DBOperationHelper_initOnLoad(Handle<Object> target) {
  DEBUG_MARKER(UDEB_DETAIL);
  DEFINE_JS_FUNCTION(target, "DBOperationHelper", DBOperationHelper);

  Persistent<Object> OpHelper = Persistent<Object>(Object::New());
  target->Set(Persistent<String>(String::NewSymbol("OpHelper")), OpHelper);
  DEFINE_JS_INT(OpHelper, "row_buffer",   HELPER_ROW_BUFFER);
  DEFINE_JS_INT(OpHelper, "key_buffer",   HELPER_KEY_BUFFER);
  DEFINE_JS_INT(OpHelper, "row_record",   HELPER_ROW_RECORD);
  DEFINE_JS_INT(OpHelper, "key_record",   HELPER_KEY_RECORD);
  DEFINE_JS_INT(OpHelper, "lock_mode",    HELPER_LOCK_MODE);
  DEFINE_JS_INT(OpHelper, "column_mask",  HELPER_COLUMN_MASK);
  DEFINE_JS_INT(OpHelper, "value_obj",    HELPER_VALUE_OBJECT);
  DEFINE_JS_INT(OpHelper, "opcode",       HELPER_OPCODE);
  DEFINE_JS_INT(OpHelper, "is_value_obj", HELPER_IS_VO);
  DEFINE_JS_INT(OpHelper, "is_valid",     HELPER_IS_VALID);

  Persistent<Object> LockModes = Persistent<Object>(Object::New());
  target->Set(Persistent<String>(String::NewSymbol("LockModes")), LockModes);
  DEFINE_JS_INT(LockModes, "EXCLUSIVE", NdbOperation::LM_Exclusive);
  DEFINE_JS_INT(LockModes, "SHARED", NdbOperation::LM_Read);
  DEFINE_JS_INT(LockModes, "COMMITTED", NdbOperation::LM_CommittedRead);
}

