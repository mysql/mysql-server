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

#include <string.h>

#include <node.h>
#include <node_buffer.h>

#include "adapter_global.h"
#include "KeyOperation.h"
#include "BatchImpl.h"
#include "NdbWrappers.h"
#include "js_wrapper_macros.h"
#include "NdbRecordObject.h"
#include "TransactionImpl.h"
#include "BlobHandler.h"

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
  HELPER_BLOBS,
  HELPER_IS_VALID
};

void DBOperationHelper_VO(Handle<Object>, KeyOperation &);
void DBOperationHelper_NonVO(Handle<Object>, KeyOperation &);

void setKeysInOp(Handle<Object> spec, KeyOperation & op);


/* DBOperationHelper takes an array of HelperSpecs.
   arg0: Length of Array
   arg1: Array of HelperSpecs
   arg2: TransactionImpl *
   arg3: Old BatchImpl wrapper (for recycling)

   Returns: BatchImpl
*/
void DBOperationHelper(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());

  int length = args[0]->Int32Value();
  const Local<Object> array = args[1]->ToObject();
  TransactionImpl *txc = unwrapPointer<TransactionImpl *>(args[2]->ToObject());
  Handle<Value> oldWrapper = args[3];

  BatchImpl * pendingOps = new BatchImpl(txc, length);

  for(int i = 0 ; i < length ; i++) {
    Handle<Object> spec = array->Get(i)->ToObject();

    int opcode  = spec->Get(HELPER_OPCODE)->Int32Value();
    bool is_vo  = spec->Get(HELPER_IS_VO)->ToBoolean()->Value();
    bool op_ok  = spec->Get(HELPER_IS_VALID)->ToBoolean()->Value();

    KeyOperation * op = pendingOps->getKeyOperation(i);
    
    if(op_ok) {
      op->opcode = opcode;
      if(is_vo) DBOperationHelper_VO(spec, *op);
      else      DBOperationHelper_NonVO(spec, *op);
    }
  }
  
  if(oldWrapper->IsObject()) {
    args.GetReturnValue().Set(BatchImpl_Recycle(oldWrapper->ToObject(), pendingOps));
  } else {
    args.GetReturnValue().Set(BatchImpl_Wrapper(pendingOps));
  }
}


void setKeysInOp(Handle<Object> spec, KeyOperation & op) {
  Local<Value> v;
  Local<Object> o;

  v = spec->Get(HELPER_KEY_BUFFER);
  if(! v->IsNull()) {
    o = v->ToObject();
    op.key_buffer = node::Buffer::Data(o);
  }
  
  v = spec->Get(HELPER_KEY_RECORD);
  if(! v->IsNull()) {
    o = v->ToObject();
    op.key_record = unwrapPointer<const Record *>(o);
  }
}


void DBOperationHelper_NonVO(Handle<Object> spec, KeyOperation & op) {
  Local<Value> v;
  Local<Object> o;

  setKeysInOp(spec, op);
  
  v = spec->Get(HELPER_ROW_BUFFER);
  if(! v->IsNull()) {
    o = v->ToObject();
    op.row_buffer = node::Buffer::Data(o);
  }
  
  v = spec->Get(HELPER_ROW_RECORD);
  if(! v->IsNull()) {
    o = v->ToObject();
    const Record * record = unwrapPointer<const Record *>(o);
    op.row_record = record;

    v = spec->Get(HELPER_BLOBS);
    if(v->IsObject()) {
      if(op.opcode == 1) {
        op.nblobs = op.createBlobReadHandles(record);
      } else {
        op.nblobs = op.createBlobWriteHandles(v->ToObject(), record);
      }
    }
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

  DEBUG_PRINT("Non-VO %s -- mask: %u lobs: %d", op.getOperationName(), 
              op.u.maskvalue, op.nblobs);
}


void DBOperationHelper_VO(Handle<Object> spec,  KeyOperation & op) {
  DEBUG_MARKER(UDEB_DETAIL);
  Local<Value> v;
  Local<Object> o;
  Local<Object> valueObj;

  v = spec->Get(HELPER_VALUE_OBJECT);
  valueObj = v->ToObject();
  NdbRecordObject * nro = unwrapPointer<NdbRecordObject *>(valueObj);

  /* Set the key record and key buffer from the helper spec */
  setKeysInOp(spec, op);
  
  /* Set the row record, row buffer, and mask from the VO */
  op.row_record = nro->getRecord();
  op.row_buffer = nro->getBuffer();

  /* A persist operation must write all columns.
     A save operation must write all columns only if the PK has changed.
     Other operations only write columns that have changed since being read.
  */
  if(op.opcode == 2) 
    op.setRowMask(op.row_record->getAllColumnMask());
  else if(op.opcode == 8 && (nro->getMaskValue() & op.row_record->getPkColumnMask())) 
    op.setRowMask(op.row_record->getAllColumnMask());
  else 
    op.setRowMask(nro->getMaskValue());

  op.nblobs = nro->createBlobWriteHandles(op);

  DEBUG_PRINT("  VO   %s -- mask: %u lobs: %d", op.getOperationName(), 
              op.u.maskvalue, op.nblobs);
  nro->resetMask(); 
}


void DBOperationHelper_initOnLoad(Handle<Object> target) {
  DEBUG_MARKER(UDEB_DETAIL);
  DEFINE_JS_FUNCTION(target, "DBOperationHelper", DBOperationHelper);
  Local<Object> OpHelper = Object::New(Isolate::GetCurrent());
  Local<Object> LockModes = Object::New(Isolate::GetCurrent());

  target->Set(NEW_SYMBOL("OpHelper"), OpHelper);
  DEFINE_JS_INT(OpHelper, "row_buffer",   HELPER_ROW_BUFFER);
  DEFINE_JS_INT(OpHelper, "key_buffer",   HELPER_KEY_BUFFER);
  DEFINE_JS_INT(OpHelper, "row_record",   HELPER_ROW_RECORD);
  DEFINE_JS_INT(OpHelper, "key_record",   HELPER_KEY_RECORD);
  DEFINE_JS_INT(OpHelper, "lock_mode",    HELPER_LOCK_MODE);
  DEFINE_JS_INT(OpHelper, "column_mask",  HELPER_COLUMN_MASK);
  DEFINE_JS_INT(OpHelper, "value_obj",    HELPER_VALUE_OBJECT);
  DEFINE_JS_INT(OpHelper, "opcode",       HELPER_OPCODE);
  DEFINE_JS_INT(OpHelper, "is_value_obj", HELPER_IS_VO);
  DEFINE_JS_INT(OpHelper, "blobs",        HELPER_BLOBS);
  DEFINE_JS_INT(OpHelper, "is_valid",     HELPER_IS_VALID);

  target->Set(NEW_SYMBOL("LockModes"), LockModes);
  DEFINE_JS_INT(LockModes, "EXCLUSIVE", NdbOperation::LM_Exclusive);
  DEFINE_JS_INT(LockModes, "SHARED", NdbOperation::LM_Read);
  DEFINE_JS_INT(LockModes, "COMMITTED", NdbOperation::LM_CommittedRead);
}

