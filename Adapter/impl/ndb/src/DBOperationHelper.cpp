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
#include "KeyOperation.h"
#include "DBOperationSet.h"
#include "NdbWrappers.h"
#include "v8_binder.h"
#include "js_wrapper_macros.h"
#include "NdbRecordObject.h"
#include "DBTransactionContext.h"
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
   arg2: DBTransactionContext *
   arg3: Old DBOperationSet wrapper (for recycling)

   Returns: DBOperationSet
*/
Handle<Value> DBOperationHelper(const Arguments &args) {
  HandleScope scope;

  int length = args[0]->Int32Value();
  const Local<Object> array = args[1]->ToObject();
  DBTransactionContext *txc = unwrapPointer<DBTransactionContext *>(args[2]->ToObject());
  Handle<Value> oldWrapper = args[3];

  DBOperationSet * pendingOps = new DBOperationSet(txc, length);

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
    return DBOperationSet_Recycle(oldWrapper->ToObject(), pendingOps);
  } else {
    return DBOperationSet_Wrapper(pendingOps);
  }
}


void setKeysInOp(Handle<Object> spec, KeyOperation & op) {
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


int createBlobReadHandles(Handle<Object> blobsArray, const Record * rowRecord,
                          KeyOperation & op) {
  int ncreated = 0;
  int ncol = rowRecord->getNoOfColumns();
  for(int i = 0 ; i < ncol ; i++) {
    const NdbDictionary::Column * col = rowRecord->getColumn(i);
    if((col->getType() ==  NdbDictionary::Column::Blob) ||
       (col->getType() ==  NdbDictionary::Column::Text)) 
    {
      op.setBlobHandler(new BlobReadHandler(i, col->getColumnNo()));
      ncreated++;
    }
  }
  return ncreated;
}


int createBlobWriteHandles(Handle<Object> blobsArray, const Record * rowRecord,
                           KeyOperation & op) {
  int ncreated = 0;
  int ncol = rowRecord->getNoOfColumns();
  for(int i = 0 ; i < ncol ; i++) {
    if(blobsArray->Get(i)->IsObject()) {
      Local<Object> blobValue = blobsArray->Get(i)->ToObject();
      assert(node::Buffer::HasInstance(blobValue));
      const NdbDictionary::Column * col = rowRecord->getColumn(i);
      assert( (col->getType() ==  NdbDictionary::Column::Blob) ||
              (col->getType() ==  NdbDictionary::Column::Text));
      ncreated++;
      op.setBlobHandler(new BlobWriteHandler(i, col->getColumnNo(), blobValue));
    }
  }
  return ncreated;
}


void DBOperationHelper_NonVO(Handle<Object> spec, KeyOperation & op) {
  HandleScope scope;

  Local<Value> v;
  Local<Object> o;
  int nblobs = 0;

  setKeysInOp(spec, op);
  
  v = spec->Get(HELPER_ROW_BUFFER);
  if(! v->IsNull()) {
    o = v->ToObject();
    op.row_buffer = V8BINDER_UNWRAP_BUFFER(o);
  }
  
  v = spec->Get(HELPER_ROW_RECORD);
  if(! v->IsNull()) {
    o = v->ToObject();
    const Record * record = unwrapPointer<const Record *>(o);
    op.row_record = record;

    v = spec->Get(HELPER_BLOBS);
    if(v->IsObject()) {
      if(op.opcode == 1) {
        nblobs = createBlobReadHandles(v->ToObject(), record, op);
      } else {
        nblobs = createBlobWriteHandles(v->ToObject(), record, op);
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
              op.u.maskvalue, nblobs);
}


void DBOperationHelper_VO(Handle<Object> spec,  KeyOperation & op) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
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

  int nblobs = nro->createBlobWriteHandles(op);

  DEBUG_PRINT("  VO   %s -- mask: %u lobs: %d", op.getOperationName(), 
              op.u.maskvalue, nblobs);
  nro->resetMask(); 
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
  DEFINE_JS_INT(OpHelper, "blobs",        HELPER_BLOBS);
  DEFINE_JS_INT(OpHelper, "is_valid",     HELPER_IS_VALID);

  Persistent<Object> LockModes = Persistent<Object>(Object::New());
  target->Set(Persistent<String>(String::NewSymbol("LockModes")), LockModes);
  DEFINE_JS_INT(LockModes, "EXCLUSIVE", NdbOperation::LM_Exclusive);
  DEFINE_JS_INT(LockModes, "SHARED", NdbOperation::LM_Read);
  DEFINE_JS_INT(LockModes, "COMMITTED", NdbOperation::LM_CommittedRead);
}

