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

#ifndef NODEJS_ADAPTER_NDB_INCLUDE_KEYOPERATION_H
#define NODEJS_ADAPTER_NDB_INCLUDE_KEYOPERATION_H

#include <string.h>
#include "Record.h"
#include "node.h"

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
  char * opcode_str;
  
  // Constructor
  KeyOperation();
  
  // Select columns
  void useSelectedColumns();
  void useAllColumns();
  void useColumn(int id);
  void setRowMask(uint32_t);
  
  /* NdbTransaction method wrappers */
  // startTransaction
  NdbTransaction *startTransaction(Ndb *) const;

  // read
  const NdbOperation *readTuple(NdbTransaction *);

  // delete
  const NdbOperation *deleteTuple(NdbTransaction *);

  // write
  const NdbOperation *writeTuple(NdbTransaction *);
  const NdbOperation *insertTuple(NdbTransaction *);
  const NdbOperation *updateTuple(NdbTransaction *);

  // generic
  const NdbOperation *prepare(NdbTransaction *);
 };
  

/* ================= Inline methods ================= */

inline KeyOperation::KeyOperation(): 
  row_buffer(0), key_buffer(0), row_record(0), key_record(0),
  read_mask_ptr(0), lmode(NdbOperation::LM_SimpleRead), options(0), opcode(0)
{
  u.maskvalue = 0;
}
 
/* Select columns for reading */
inline void KeyOperation::useSelectedColumns() {
  read_mask_ptr = u.row_mask;
}

inline void KeyOperation::useAllColumns() {
  read_mask_ptr = 0;
}

inline void KeyOperation::useColumn(int col_id) {
  u.row_mask[col_id >> 3] |= (1 << (col_id & 7));
}

inline void KeyOperation::setRowMask(const uint32_t newMaskValue) {
  u.maskvalue = newMaskValue;
}
/* NdbTransaction method wrappers */

inline const NdbOperation * 
  KeyOperation::readTuple(NdbTransaction *tx) {
    return tx->readTuple(key_record->getNdbRecord(), key_buffer,
                         row_record->getNdbRecord(), row_buffer, lmode, read_mask_ptr);
}

inline const NdbOperation * KeyOperation::deleteTuple(NdbTransaction *tx) {
    return tx->deleteTuple(key_record->getNdbRecord(), key_buffer,
                           row_record->getNdbRecord(), 0, 0, options);
}                         

inline const NdbOperation * KeyOperation::writeTuple(NdbTransaction *tx) { 
  return tx->writeTuple(key_record->getNdbRecord(), key_buffer,
                        row_record->getNdbRecord(), row_buffer, u.row_mask);
}

inline const NdbOperation * KeyOperation::insertTuple(NdbTransaction *tx) { 
    return tx->insertTuple(row_record->getNdbRecord(), row_buffer,
                           u.row_mask, options);
}

inline const NdbOperation * KeyOperation::updateTuple(NdbTransaction *tx) { 
    return tx->updateTuple(key_record->getNdbRecord(), key_buffer,
                           row_record->getNdbRecord(), row_buffer,
                           u.row_mask, options);
}

inline const NdbOperation * KeyOperation::prepare(NdbTransaction *tx) {
  switch(opcode) {
    case 1:  // OP_READ:
      opcode_str = "read  ";
      return readTuple(tx);
    case 2:  // OP_INSERT:
      opcode_str = "insert";
      return insertTuple(tx);
    case 4:  // OP_UPDATE:
      opcode_str = "update";
      return updateTuple(tx);
    case 8:  // OP_WRITE:
      opcode_str = "write ";
      return writeTuple(tx);
    case 16: // OP_DELETE:
      opcode_str = "delete";
      return deleteTuple(tx);
    default:
      opcode_str = "-XXX-";
      return NULL;
  }
}

#endif
