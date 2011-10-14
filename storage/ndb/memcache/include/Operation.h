/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
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
  
public: 
  /* Public Methods: */
  Operation(QueryPlan *p, int o = 1, char *kbuf = 0); 
    
  // Methods for writing to the key record
  size_t requiredKeyBuffer();
  void clearKeyNullBits();
  bool setKeyPart(int idx, const char *strval, size_t strlen);
  bool setKeyPartInt(int idx, int value);
  void setKeyPartNull(int idx);

  // Methods for writing to the row   
  size_t requiredBuffer();
  void clearNullBits();
  bool setColumn(int idx, const char *strval, size_t strlen);
  bool setColumnInt(int idx, int value);
  bool setColumnBigUnsigned(int idx, Uint64 value);
  void setColumnNull(int idx);

  // Methods for reading columns from the response
  int nValues() const;
  bool isNull(int idx) const;
  size_t getStringifiedLength() const;
  void * getPointer(int idx) const;
  int getIntValue(int idx) const;
  Uint64 getBigUnsignedValue(int idx) const;
  bool getStringValueNoCopy(int idx, char **dstptr, size_t *lenptr) const;
  bool appendCRLF(int idx, size_t len) const;
  size_t copyValue(int idx, char *dest) const;

  /* NdbTransaction method wrappers */
  // startTransaction
  NdbTransaction *startTransaction(Ndb *) const;

  // read
  const NdbOperation *readTuple(NdbTransaction *tx, 
                                NdbOperation::LockMode lmod = NdbOperation::LM_SimpleRead);
  const NdbOperation *readMasked(NdbTransaction *, const unsigned char *, 
                                 NdbOperation::LockMode);

  // delete
  const NdbOperation *deleteTuple(NdbTransaction *tx);
  const NdbOperation *deleteTupleCAS(NdbTransaction *tx, NdbOperation::OperationOptions *);

  // write
  const NdbOperation *writeTuple(NdbTransaction *tx);
  const NdbOperation *insertTuple(NdbTransaction *tx);
  const NdbOperation *insertMasked(NdbTransaction *tx, const unsigned char * mask,
                                   NdbOperation::OperationOptions *options = 0);
  const NdbOperation *updateTuple(NdbTransaction *tx);  
  const NdbOperation *updateInterpreted(NdbTransaction *tx, 
                                        NdbOperation::OperationOptions *,
                                        const unsigned char * mask = 0);

  // scan
  NdbScanOperation *scanTable(NdbTransaction *tx);
  NdbIndexScanOperation *scanIndex(NdbTransaction *tx,
                                   NdbIndexScanOperation::IndexBound *bound);

 };
  

/* ================= Inline methods ================= */


/* Methods for writing to the key record */

inline size_t Operation::requiredKeyBuffer() {
  return plan->key_record->rec_size;
}

inline void Operation::clearKeyNullBits() {
  plan->key_record->clearNullBits(key_buffer);
}

inline bool Operation::setKeyPart(int idx, const char *strval, size_t strlen) {
  int s = plan->key_record->encode(idx, strval, strlen, key_buffer);
  return (s > 0);
}

inline bool Operation::setKeyPartInt(int idx, int value) {
  return plan->key_record->setIntValue(idx, value, key_buffer);
}

inline void Operation::setKeyPartNull(int idx) {
  plan->key_record->setNull(idx, key_buffer);
}


/*  Methods for writing to the row  */

inline size_t Operation::requiredBuffer() {
  return record->rec_size;
}

inline void Operation::clearNullBits() {
  record->clearNullBits(buffer);
}

inline bool Operation::setColumn(int idx, const char *strval, size_t strlen) {
  int s = record->encode(idx, strval, strlen, buffer);
  return (s > 0);
}

inline bool Operation::setColumnInt(int idx, int value) {
  return record->setIntValue(idx, value, buffer);
}

inline bool Operation::setColumnBigUnsigned(int idx, Uint64 value) {
  return record->setUint64Value(idx, value, buffer);
}

inline void Operation::setColumnNull(int idx) {
  record->setNull(idx, buffer);
}


/* Methods for reading columns from the response */

inline int Operation::nValues() const {
  return record->nvalues;
}

inline bool Operation::isNull(int idx) const {
  return record->isNull(idx, buffer);
}

inline size_t Operation::getStringifiedLength() const {
  return record->getStringifiedLength(buffer);
}

inline void * Operation::getPointer(int idx) const {
  return record->getPointer(idx, buffer);
}

inline int Operation::getIntValue(int idx) const {
  return record->getIntValue(idx, buffer);
}

inline Uint64 Operation::getBigUnsignedValue(int idx) const {
  return record->getUint64Value(idx, buffer);
}

inline bool Operation::appendCRLF(int idx, size_t len) const {
  return record->appendCRLF(idx, len, buffer);
}


/* NdbTransaction method wrappers */

inline const NdbOperation * 
  Operation::readTuple(NdbTransaction *tx, NdbOperation::LockMode lmod) 
{
  return readMasked(tx, 0, lmod);
}

inline const NdbOperation * 
  Operation::readMasked(NdbTransaction *tx,
                        const unsigned char * mask,
                        NdbOperation::LockMode lmod) 
{
  return tx->readTuple(plan->key_record->ndb_record, key_buffer,
                                 record->ndb_record, buffer, lmod, mask);
}

inline const NdbOperation * Operation::deleteTuple(NdbTransaction *tx) { 
  return tx->deleteTuple(plan->key_record->ndb_record, key_buffer,
                         plan->val_record->ndb_record);
}

inline const NdbOperation * 
  Operation::deleteTupleCAS(NdbTransaction *tx, 
                            NdbOperation::OperationOptions * options) 
{
  return tx->deleteTuple(plan->key_record->ndb_record, key_buffer,
                         plan->val_record->ndb_record, 0, 0, options);
}                         

inline const NdbOperation * Operation::writeTuple(NdbTransaction *tx) { 
  return tx->writeTuple(plan->key_record->ndb_record, key_buffer,
                        plan->row_record->ndb_record, buffer);
}

inline const NdbOperation * Operation::insertTuple(NdbTransaction *tx) {
  return insertMasked(tx, 0);
}

inline const NdbOperation * 
  Operation::insertMasked(NdbTransaction *tx,
                          const unsigned char * mask,
                          NdbOperation::OperationOptions * options) 
{ 
  return tx->insertTuple(plan->key_record->ndb_record, key_buffer,
                         plan->row_record->ndb_record, buffer, mask, options);
}

inline const NdbOperation * Operation::updateTuple(NdbTransaction *tx) { 
  return tx->updateTuple(plan->key_record->ndb_record, key_buffer,
                         plan->row_record->ndb_record, buffer);
}

inline const NdbOperation * 
  Operation::updateInterpreted(NdbTransaction *tx, 
                               NdbOperation::OperationOptions * options,
                               const unsigned char * mask) 
{ 
  return tx->updateTuple(plan->key_record->ndb_record, key_buffer,
                         plan->row_record->ndb_record, buffer, mask, options);
}

inline NdbScanOperation * Operation::scanTable(NdbTransaction *tx) {
  return tx->scanTable(record->ndb_record);
}

#endif
