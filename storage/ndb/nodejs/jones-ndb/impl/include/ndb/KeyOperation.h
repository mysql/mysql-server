/*
 Copyright (c) 2014, 2023, Oracle and/or its affiliates.
 
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

#ifndef NODEJS_ADAPTER_NDB_INCLUDE_KEYOPERATION_H
#define NODEJS_ADAPTER_NDB_INCLUDE_KEYOPERATION_H

#include <string.h>
#include "Record.h"
#include "node.h"
#include "JsWrapper.h"
#include "BlobHandler.h"

class KeyOperation {
public: 
  /* Instance variables */
  char *row_buffer;
  char *key_buffer;  
  const Record *row_record;
  const Record *key_record;
  union {
    uint8_t row_mask[4];
    uint32_t maskvalue;
  } u;
  uint8_t * read_mask_ptr;
  NdbOperation::LockMode lmode;
  NdbOperation::OperationOptions *options;
  int opcode;
  int nblobs;
  BlobHandler * blobHandler;
  
  // Constructor and Destructor
  KeyOperation();
  ~KeyOperation();
  
  // Select columns
  void useSelectedColumns();
  void useAllColumns();
  void useColumn(unsigned int id);
  void setRowMask(uint32_t);

  // Prepare operation
  void setBlobHandler(BlobHandler *);
  bool isBlobReadOperation();
  const NdbOperation *prepare(NdbTransaction *);
  int createBlobReadHandles(const Record *);
  int createBlobWriteHandles(v8::Local<v8::Object>, const Record *);

  // Get results
  void readBlobResults(const Arguments &);

  // Diagnostic 
  const char * getOperationName();

private:  
  // read
  const NdbOperation *readTuple(NdbTransaction *);

  // delete
  const NdbOperation *deleteTuple(NdbTransaction *);

  // write
  const NdbOperation *writeTuple(NdbTransaction *);
  const NdbOperation *insertTuple(NdbTransaction *);
  const NdbOperation *updateTuple(NdbTransaction *);
};
  

/* ================= Inline methods ================= */

inline KeyOperation::KeyOperation(): 
  row_buffer(0), key_buffer(0), row_record(0), key_record(0),
  read_mask_ptr(0), lmode(NdbOperation::LM_SimpleRead), options(0), opcode(0),
  nblobs(0), blobHandler(0)
{
  u.maskvalue = 0;
}

inline bool KeyOperation::isBlobReadOperation() {
  return (blobHandler && (opcode & 1));
}
 
/* Select columns for reading */
inline void KeyOperation::useSelectedColumns() {
  read_mask_ptr = u.row_mask;
}

inline void KeyOperation::useAllColumns() {
  read_mask_ptr = 0;
}

inline void KeyOperation::useColumn(unsigned int col_id) {
  u.row_mask[col_id >> 3] |= static_cast<uint8_t>(1U << (col_id & 7));
}

inline void KeyOperation::setRowMask(const uint32_t newMaskValue) {
  u.maskvalue = newMaskValue;
}
#endif
