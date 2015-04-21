/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights
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
#ifndef NDBMEMCACHE_OPERATION_H
#define NDBMEMCACHE_OPERATION_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include <NdbApi.hpp>

#include "ndbmemcache_global.h"
#include "Record.h"
#include "QueryPlan.h"
#include "debug.h"
#include "workitem.h"

/* Use a QueryPlan to perform an operation. 
 
   A query plan owns three records: a key_record, a val_record, and a row_record.
   In general, the purpose of the Operation class is to hide this fact from 
   the user, and to perform one of Record's method using the correct record.
*/   

class Operation {
public:
  /* Pointers to buffers used in operations. 
   These are public because the caller owns the memory they point to.  */
  char *buffer;
  char *key_buffer;
  
private:
  /* Private instance variables */
  const QueryPlan *plan;
  const int op;
  const Record *record;
  Uint8 row_mask[4];
  Uint8 key_mask[4];
  Uint8 * read_mask_ptr;

  /* Private methods */
  void set_default_record();
  bool setFieldsInRow(int offset, const char *type, int, const char *, size_t);
  
public: 
  // Constructors
  Operation(QueryPlan *p, int o = 1, char *kbuf = 0);
  Operation(workitem *, Uint32 saved_row_mask = 0);
  Operation(QueryPlan *p, char * buffer);

  // Public Methods
  void save_row_mask(Uint32 * loc);
  
  // Select columns for reading
  void readSelectedColumns();
  void readAllColumns();
  void readColumn(int id);
    
  // Methods for writing to the key record
  size_t requiredKeyBuffer();
  void clearKeyNullBits();
  bool setKey(int nparts, const char *key_str, size_t key_str_len);
  bool setKeyPart(int id, const char *strval, size_t strlen);
  bool setKeyPartInt(int id, int value);
  void setKeyPartNull(int id);
  
  // Methods for writing to the row 
  size_t requiredBuffer();
  void setNullBits();
  void clearNullBits();
  bool setKeyFieldsInRow(int nparts, const char *key_str, size_t key_str_len);
  bool setValueFieldsInRow(int nparts, const char *val_str, size_t val_str_len);
  bool setColumn(int id, const char *strval, size_t strlen);
  bool setColumnInt(int id, int value);
  bool setColumnBigUnsigned(int id, Uint64 value);
  void setColumnNull(int id);
  void setColumnNotNull(int id);

  // Methods for reading columns from the response
  int nValues() const;
  bool isNull(int id) const;
  size_t getStringifiedLength() const;
  void * getPointer(int id) const;
  int getIntValue(int id) const;
  Uint64 getBigUnsignedValue(int id) const;
  bool getStringValueNoCopy(int id, char **dstptr, size_t *lenptr) const;
  bool appendCRLF(int id, size_t len) const;
  size_t copyValue(int id, char *dest) const;

  /* NdbTransaction method wrappers */
  // startTransaction
  NdbTransaction *startTransaction(Ndb *) const;

  // read
  const NdbOperation *readTuple(NdbTransaction *tx, 
                                NdbOperation::LockMode lmod = NdbOperation::LM_SimpleRead);

  // delete
  const NdbOperation *deleteTuple(NdbTransaction *tx,
                                  NdbOperation::OperationOptions *options = 0);
  const NdbOperation *deleteCurrentTuple(NdbScanOperation *, NdbTransaction *,
                                         NdbOperation::OperationOptions *opts = 0);

  // write
  const NdbOperation *writeTuple(NdbTransaction *tx);
  const NdbOperation *insertRow(NdbTransaction *tx);
  const NdbOperation *insertTuple(NdbTransaction *tx, 
                                  NdbOperation::OperationOptions *options = 0);
  const NdbOperation *updateTuple(NdbTransaction *tx,
                                  NdbOperation::OperationOptions *options = 0);

  // scan
  NdbScanOperation *scanTable(NdbTransaction *tx,
                              NdbOperation::LockMode lmod = NdbOperation::LM_Read,
                              NdbScanOperation::ScanOptions *options = 0);
  NdbIndexScanOperation *scanIndex(NdbTransaction *tx,
                                   NdbIndexScanOperation::IndexBound *bound);
 };
  

/* ================= Inline methods ================= */

inline void Operation::save_row_mask(Uint32 * loc) {
  memcpy(loc, row_mask, 4);
}

/* Select columns for reading */

inline void Operation::readSelectedColumns() {
  read_mask_ptr = row_mask;
}

inline void Operation::readAllColumns() {
  read_mask_ptr = 0;
}

inline void Operation::readColumn(int id) {
  record->maskActive(id, row_mask);
}

/* Methods for writing to the key record */

inline size_t Operation::requiredKeyBuffer() {
  /* Malloc checkers complain if this +1 is not present.  Not sure why.
     Theory: because the terminating null of a C-string may be written there. */
  return plan->key_record->rec_size + 1;
}

inline void Operation::clearKeyNullBits() {
  plan->key_record->clearNullBits(key_buffer);
}

inline bool Operation::setKeyPart(int id, const char *strval, size_t str_len) {
  int s = plan->key_record->encode(id, strval, str_len, key_buffer, key_mask);
  return (s > 0);
}

inline bool Operation::setKeyPartInt(int id, int value) {
  return plan->key_record->setIntValue(id, value, key_buffer, key_mask);
}

inline void Operation::setKeyPartNull(int id) {
  plan->key_record->setNull(id, key_buffer, key_mask);
}


/*  Methods for writing to the row  */

inline size_t Operation::requiredBuffer() {
  return record->rec_size + 1;
}

inline void Operation::setNullBits() {
  record->setNullBits(buffer);
}

inline void Operation::clearNullBits() {
  record->clearNullBits(buffer);
}

inline bool Operation::setColumn(int id, const char *strval, size_t strlen) {
  int s = record->encode(id, strval, strlen, buffer, row_mask);
  return (s > 0);
}

inline bool Operation::setColumnInt(int id, int value) {
  return record->setIntValue(id, value, buffer, row_mask);
}

inline bool Operation::setColumnBigUnsigned(int id, Uint64 value) {
  return record->setUint64Value(id, value, buffer, row_mask);
}

inline void Operation::setColumnNull(int id) {
  record->setNull(id, buffer, row_mask);
}

inline void Operation::setColumnNotNull(int id) {
  record->setNotNull(id, buffer, row_mask);
}

inline bool Operation::setKeyFieldsInRow(int nparts, const char *dbkey, size_t key_len) {
  return setFieldsInRow(COL_STORE_KEY, "key", nparts, dbkey, key_len);
}

inline bool Operation::setValueFieldsInRow(int nparts, const char *dbval, size_t val_len) {
  return setFieldsInRow(COL_STORE_VALUE, "value", nparts, dbval, val_len);
}


/* Methods for reading columns from the response */

inline int Operation::nValues() const {
  return record->nvalues;
}

inline bool Operation::isNull(int id) const {
  return record->isNull(id, buffer);
}

inline size_t Operation::getStringifiedLength() const {
  return record->getStringifiedLength(buffer);
}

inline void * Operation::getPointer(int id) const {
  return record->getPointer(id, buffer);
}

inline int Operation::getIntValue(int id) const {
  return record->getIntValue(id, buffer);
}

inline Uint64 Operation::getBigUnsignedValue(int id) const {
  return record->getUint64Value(id, buffer);
}

inline bool Operation::appendCRLF(int id, size_t len) const {
  return record->appendCRLF(id, len, buffer);
}


/* NdbTransaction method wrappers */

inline const NdbOperation * 
  Operation::readTuple(NdbTransaction *tx, NdbOperation::LockMode lmode) {
  return tx->readTuple(plan->key_record->ndb_record, key_buffer,
                                 record->ndb_record, buffer, lmode, 
                                 read_mask_ptr);
}

inline const NdbOperation * 
  Operation::deleteTuple(NdbTransaction *tx, 
                         NdbOperation::OperationOptions * options) {
  return tx->deleteTuple(plan->key_record->ndb_record, key_buffer,
                         plan->val_record->ndb_record, 0, 0, options);
}                         

inline const NdbOperation * Operation::writeTuple(NdbTransaction *tx) { 
  return tx->writeTuple(plan->key_record->ndb_record, key_buffer,
                        plan->row_record->ndb_record, buffer, row_mask);
}

inline const NdbOperation * Operation::insertRow(NdbTransaction *tx) { 
  return tx->insertTuple(plan->row_record->ndb_record, buffer, row_mask);
}

inline const NdbOperation * 
  Operation::insertTuple(NdbTransaction *tx,
                         NdbOperation::OperationOptions * options) { 
  return tx->insertTuple(plan->key_record->ndb_record, key_buffer,
                         plan->row_record->ndb_record, buffer, 
                         row_mask, options);
}

inline const NdbOperation * 
  Operation::updateTuple(NdbTransaction *tx,
                         NdbOperation::OperationOptions * options) { 
  return tx->updateTuple(plan->key_record->ndb_record, key_buffer,
                         plan->row_record->ndb_record, buffer, 
                         row_mask, options);
}

inline NdbScanOperation * 
  Operation::scanTable(NdbTransaction *tx, NdbOperation::LockMode lmode,
                       NdbScanOperation::ScanOptions *opts) {
    return tx->scanTable(record->ndb_record, lmode,
                         read_mask_ptr, opts, 0);
}

inline const NdbOperation * 
  Operation::deleteCurrentTuple(NdbScanOperation *scanop,
                                NdbTransaction *tx,
                                NdbOperation::OperationOptions *opts) {
    return scanop->deleteCurrentTuple(tx, record->ndb_record, buffer, 
                                      read_mask_ptr, opts);
}


#endif
