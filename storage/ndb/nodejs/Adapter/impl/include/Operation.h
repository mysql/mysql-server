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

#ifndef OPERATION_H
#define OPERATION_H
#pragma once

#include <string.h>

#include <NdbApi.hpp>

#include "Record.h"

class Operation {
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
  NdbScanOperation::ScanOptions  *scan_options;
  
  // Constructor
  Operation();
  
  // Select columns
  void useSelectedColumns();
  void useAllColumns();
  void useColumn(int id);
  void setRowMask(uint32_t);
  
  /* NdbTransaction method wrappers */
  // startTransaction
  NdbTransaction *startTransaction(Ndb *) const;

  // read
  const NdbOperation *readTuple(NdbTransaction *tx);

  // delete
  const NdbOperation *deleteTuple(NdbTransaction *tx);
  const NdbOperation *deleteCurrentTuple(NdbScanOperation *, NdbTransaction *);

  // write
  const NdbOperation *writeTuple(NdbTransaction *tx);
  const NdbOperation *insertTuple(NdbTransaction *tx);
  const NdbOperation *updateTuple(NdbTransaction *tx);

  // scan
  NdbScanOperation *scanTable(NdbTransaction *tx);
  NdbIndexScanOperation *scanIndex(NdbTransaction *tx,
                                   NdbIndexScanOperation::IndexBound *bound);
 };
  

/* ================= Inline methods ================= */

inline Operation::Operation(): 
  row_buffer(0), key_buffer(0), row_record(0), key_record(0),
  read_mask_ptr(0), lmode(NdbOperation::LM_SimpleRead), options(0)
{
  u.maskvalue = 0;
}
 
/* Select columns for reading */
inline void Operation::useSelectedColumns() {
  read_mask_ptr = u.row_mask;
}

inline void Operation::useAllColumns() {
  read_mask_ptr = 0;
}

inline void Operation::useColumn(int col_id) {
  u.row_mask[col_id >> 3] |= (1 << (col_id & 7));
}

inline void Operation::setRowMask(const uint32_t newMaskValue) {
  u.maskvalue = newMaskValue;
}
/* NdbTransaction method wrappers */

inline const NdbOperation * 
  Operation::readTuple(NdbTransaction *tx) {
    return tx->readTuple(key_record->getNdbRecord(), key_buffer,
                         row_record->getNdbRecord(), row_buffer, lmode, read_mask_ptr);
}

inline const NdbOperation * 
  Operation::deleteTuple(NdbTransaction *tx) {
    return tx->deleteTuple(key_record->getNdbRecord(), key_buffer,
                           row_record->getNdbRecord(), 0, 0, options);
}                         

inline const NdbOperation * Operation::writeTuple(NdbTransaction *tx) { 
  return tx->writeTuple(key_record->getNdbRecord(), key_buffer,
                        row_record->getNdbRecord(), row_buffer, u.row_mask);
}

inline const NdbOperation * 
  Operation::insertTuple(NdbTransaction *tx) { 
    return tx->insertTuple(row_record->getNdbRecord(), row_buffer,
                           u.row_mask, options);
}

inline const NdbOperation * 
  Operation::updateTuple(NdbTransaction *tx) { 
    return tx->updateTuple(key_record->getNdbRecord(), key_buffer,
                           row_record->getNdbRecord(), row_buffer,
                           u.row_mask, options);
}

inline NdbScanOperation * 
  Operation::scanTable(NdbTransaction *tx) {
    return tx->scanTable(row_record->getNdbRecord(), lmode,
                         read_mask_ptr, scan_options, 0);
}

inline NdbIndexScanOperation * 
  Operation::scanIndex(NdbTransaction *tx,
                       NdbIndexScanOperation::IndexBound *bound = 0) {
    return tx->scanIndex(key_record->getNdbRecord(),    // scan key    
                         row_record->getNdbRecord(),    // row record  
                         lmode,                         // lock mode   
                         read_mask_ptr,                 // result mask 
                         bound,                         // bound       
                         scan_options, 0);
}

inline const NdbOperation * 
  Operation::deleteCurrentTuple(NdbScanOperation *scanop,
                                NdbTransaction *tx) {
    return scanop->deleteCurrentTuple(tx, row_record->getNdbRecord(), row_buffer, 
                                      read_mask_ptr, options);
}
#endif // OPERATION_H

