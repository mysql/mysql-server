/*
 Copyright (c) 2014, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef NODEJS_ADAPTER_NDB_INCLUDE_SCANOPERATION_H
#define NODEJS_ADAPTER_NDB_INCLUDE_SCANOPERATION_H

#include "JsWrapper.h"

// Members of ScanHelper
enum {
  SCAN_TABLE_RECORD = 0,
  SCAN_INDEX_RECORD,
  SCAN_LOCK_MODE,
  SCAN_BOUNDS,
  SCAN_OPTION_FLAGS,
  SCAN_OPTION_BATCH_SIZE,
  SCAN_OPTION_PARALLELISM,
  SCAN_FILTER_CODE
};

// Scan opcodes
enum { OP_SCAN_READ = 33, OP_SCAN_COUNT = 34, OP_SCAN_DELETE = 48 };

#include "KeyOperation.h"

class TransactionImpl;

class ScanOperation : public KeyOperation {
 public:
  ScanOperation(const Arguments &);
  ~ScanOperation();
  const NdbError &getNdbError();

  /*  Execute the scan. This call:
      (1) Prepares the scan operation.
      (2) Runs Execute + NoCommit so that the user can start reading results.

      The async wrapper for this call will getNdbError() on the NdbTransaction;
      after a TimeoutExpired error, the call can be run again to retry.

      The JavaScript wrapper for this function is Async.
  */
  int prepareAndExecute();

  int fetchResults(char *buffer, bool);
  int nextResult(char *buffer);
  void close();

 protected:
  friend class TransactionImpl;

  void prepareScan(NdbTransaction *);
  NdbScanOperation *scanTable(NdbTransaction *tx);
  NdbIndexScanOperation *scanIndex(NdbTransaction *tx,
                                   NdbIndexScanOperation::IndexBound *bound);
  const NdbOperation *deleteCurrentTuple(NdbScanOperation *, NdbTransaction *);

 private:
  TransactionImpl *ctx;
  NdbScanOperation *scan_op;
  NdbIndexScanOperation *index_scan_op;
  NdbIndexScanOperation::IndexBound **bounds;
  int nbounds;
  bool isIndexScan;
  NdbScanOperation::ScanOptions scan_options;
};

inline NdbScanOperation *ScanOperation::scanTable(NdbTransaction *tx) {
  return tx->scanTable(row_record->getNdbRecord(), lmode, read_mask_ptr,
                       &scan_options, 0);
}

inline NdbIndexScanOperation *ScanOperation::scanIndex(
    NdbTransaction *tx, NdbIndexScanOperation::IndexBound *bound = 0) {
  return tx->scanIndex(key_record->getNdbRecord(),  // scan key
                       row_record->getNdbRecord(),  // row record
                       lmode,                       // lock mode
                       read_mask_ptr,               // result mask
                       bound,                       // bound
                       &scan_options, 0);
}

inline const NdbOperation *ScanOperation::deleteCurrentTuple(
    NdbScanOperation *scanop, NdbTransaction *tx) {
  return scanop->deleteCurrentTuple(tx, row_record->getNdbRecord(), row_buffer,
                                    read_mask_ptr, options);
}

#endif
