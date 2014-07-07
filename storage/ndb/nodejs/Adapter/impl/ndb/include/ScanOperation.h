/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

#ifndef NODEJS_ADAPTER_NDB_INCLUDE_SCANOPERATION_H
#define NODEJS_ADAPTER_NDB_INCLUDE_SCANOPERATION_H

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
enum { 
  OP_SCAN_READ   = 33,
  OP_SCAN_COUNT  = 34,
  OP_SCAN_DELETE = 48
};

#include "KeyOperation.h"

class  DBTransactionContext;

class ScanOperation : public KeyOperation {
public:
  ScanOperation(const v8::Arguments &);
  ~ScanOperation();
  const NdbError & getNdbError();

  /*  Execute the scan. This call: 
      (1) Prepares the scan operation.
      (2) Runs Execute + NoCommit so that the user can start reading results.

      The async wrapper for this call will getNdbError() on the NdbTransaction;
      after a TimeoutExpired error, the call can be run again to retry.
      
      The JavaScript wrapper for this function is Async.
  */  
  int prepareAndExecute();
  
  int fetchResults(char * buffer, bool);
  int nextResult(char * buffer);
  void close();
  
protected:
  friend class DBTransactionContext;

  void prepareScan(NdbTransaction *);
  NdbScanOperation *scanTable(NdbTransaction *tx);
  NdbIndexScanOperation *scanIndex(NdbTransaction *tx,
                                   NdbIndexScanOperation::IndexBound *bound);
  const NdbOperation *deleteCurrentTuple(NdbScanOperation *, NdbTransaction *);

private:
  DBTransactionContext *ctx;
  NdbScanOperation * scan_op;
  NdbIndexScanOperation * index_scan_op;
  NdbIndexScanOperation::IndexBound **bounds;
  int nbounds;
  bool isIndexScan;
  NdbScanOperation::ScanOptions scan_options;
};

inline NdbScanOperation * 
  ScanOperation::scanTable(NdbTransaction *tx) {
    return tx->scanTable(row_record->getNdbRecord(), lmode,
                         read_mask_ptr, & scan_options, 0);
}

inline NdbIndexScanOperation * 
  ScanOperation::scanIndex(NdbTransaction *tx,
                           NdbIndexScanOperation::IndexBound *bound = 0) {
    return tx->scanIndex(key_record->getNdbRecord(),    // scan key    
                         row_record->getNdbRecord(),    // row record  
                         lmode,                         // lock mode   
                         read_mask_ptr,                 // result mask 
                         bound,                         // bound       
                         & scan_options, 0);
}

inline const NdbOperation * 
  ScanOperation::deleteCurrentTuple(NdbScanOperation *scanop,
                                    NdbTransaction *tx) {
    return scanop->deleteCurrentTuple(tx, row_record->getNdbRecord(), 
                                      row_buffer, read_mask_ptr, options);
}


#endif
