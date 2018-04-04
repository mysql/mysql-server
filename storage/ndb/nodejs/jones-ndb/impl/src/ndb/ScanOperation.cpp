/*
 Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.
 
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

#include <NdbApi.hpp>

#include "NdbQueryBuilder.hpp"
#include "NdbQueryOperation.hpp"

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NdbWrapperErrors.h"
#include "NativeMethodCall.h"
#include "ScanOperation.h"
#include "TransactionImpl.h"

using namespace v8;

void debug_print_flags_and_options(const NdbScanOperation::ScanOptions & opts) {
  char flags[128];
  char optstring[80];
  snprintf(flags, sizeof(flags), "%s%s%s%s%s%s%s%s%s",
    opts.scan_flags & NdbScanOperation::SF_TupScan     ? " TupScan"       : "",
    opts.scan_flags & NdbScanOperation::SF_DiskScan    ? " DiskScan"      : "",
    opts.scan_flags & NdbScanOperation::SF_OrderBy     ? " OrderBy"       : "",
    opts.scan_flags & NdbScanOperation::SF_OrderByFull ? " OrderByFull"   : "",
    opts.scan_flags & NdbScanOperation::SF_Descending  ? " Descending"    : "",
    opts.scan_flags & NdbScanOperation::SF_ReadRangeNo ? " ReadRangeNo"   : "",
    opts.scan_flags & NdbScanOperation::SF_MultiRange  ? " MultiRange"    : "",
    opts.scan_flags & NdbScanOperation::SF_KeyInfo     ? " KeyInfo"       : "",
    opts.scan_flags ? "" : " [None]");
  snprintf(optstring, sizeof(optstring), "%s%s%s%s",
    opts.optionsPresent & NdbScanOperation::ScanOptions::SO_SCANFLAGS   ? " HasScanFlags": "",
    opts.optionsPresent & NdbScanOperation::ScanOptions::SO_BATCH       ? " Batch"       : "",
    opts.optionsPresent & NdbScanOperation::ScanOptions::SO_INTERPRETED ? " Interpreted" : "",
    opts.optionsPresent & NdbScanOperation::ScanOptions::SO_PARALLEL    ? " Parallel"    : "");

  DEBUG_PRINT("Scan flags:%s  options:%s", flags, optstring);
}

ScanOperation::ScanOperation(const Arguments &args) : 
  KeyOperation(),
  scan_op(0),
  index_scan_op(0),
  nbounds(0),
  isIndexScan(false)
{
  DEBUG_MARKER(UDEB_DEBUG);

  Local<Value> v;

  const Local<Object> spec = args[0]->ToObject();
  opcode = args[1]->Int32Value();
  ctx = unwrapPointer<TransactionImpl *>(args[2]->ToObject());

  lmode = NdbOperation::LM_CommittedRead;
  scan_options.scan_flags = 0;
  scan_options.optionsPresent = 0ULL;

  v = spec->Get(SCAN_TABLE_RECORD);
  if(! v->IsNull()) {
    Local<Object> o = v->ToObject();
    row_record = unwrapPointer<const Record *>(o);
    createBlobReadHandles(row_record);
  }

  v = spec->Get(SCAN_INDEX_RECORD);
  if(! v->IsNull()) {
    Local<Object> o = v->ToObject();
    isIndexScan = true;
    key_record = unwrapPointer<const Record *>(o);
  }
  
  v = spec->Get(SCAN_LOCK_MODE);
  if(! v->IsNull()) {
    int intLockMode = v->Int32Value();
    DEBUG_PRINT("Scan lock mode %d", intLockMode);
    lmode = static_cast<NdbOperation::LockMode>(intLockMode);
  }

  // SCAN_BOUNDS is an array of BoundHelpers  
  v = spec->Get(SCAN_BOUNDS);
  if(v->IsArray()) {
    Local<Object> o = v->ToObject();
    while(o->Has(nbounds)) {
      nbounds++; 
    }
    DEBUG_PRINT("Index Scan with %d IndexBounds", nbounds);
    bounds = new NdbIndexScanOperation::IndexBound *[nbounds];
    for(int i = 0 ; i < nbounds ; i++) {
      Local<Object> b = o->Get(i)->ToObject();
      bounds[i] = unwrapPointer<NdbIndexScanOperation::IndexBound *>(b);
    }
  }

  v = spec->Get(SCAN_OPTION_FLAGS);
  if(! v->IsNull()) {
    scan_options.scan_flags = v->Uint32Value();
  }
  
  v = spec->Get(SCAN_OPTION_BATCH_SIZE);
  if(! v->IsNull()) {
    scan_options.batch = v->Uint32Value();
    scan_options.optionsPresent |= NdbScanOperation::ScanOptions::SO_BATCH;
  }
  
  v = spec->Get(SCAN_OPTION_PARALLELISM);
  if(! v->IsNull()) {
    scan_options.parallel = v->Uint32Value();
    scan_options.optionsPresent |= NdbScanOperation::ScanOptions::SO_PARALLEL;
  }
  
  v = spec->Get(SCAN_FILTER_CODE);
  if(! v->IsNull()) {
    Local<Object> o = v->ToObject();
    scan_options.interpretedCode = unwrapPointer<NdbInterpretedCode *>(o);
    scan_options.optionsPresent |= NdbScanOperation::ScanOptions::SO_INTERPRETED;
  }

  /* Scanning delete requires key info */
  if(opcode == OP_SCAN_DELETE) {
    scan_options.scan_flags |= NdbScanOperation::SF_KeyInfo;
  }

  /* If any flags were set, also set SO_SCANFLAGS options */
  if(scan_options.scan_flags != 0) {
    scan_options.optionsPresent |= NdbScanOperation::ScanOptions::SO_SCANFLAGS;
  }

  /* Done defining the object */
  debug_print_flags_and_options(scan_options);
}

ScanOperation::~ScanOperation() {
  if(bounds) delete[] bounds;
}

int ScanOperation::prepareAndExecute() {
  return ctx->prepareAndExecuteScan(this);
}

void ScanOperation::prepareScan(NdbTransaction *tx) {
  DEBUG_MARKER(UDEB_DEBUG);
  if(! scan_op) {  // don't re-prepare if retrying
    if(isIndexScan) {
      scan_op = index_scan_op = scanIndex(tx);
      if(index_scan_op) {
        for(int i = 0 ; i < nbounds ; i++) {
         index_scan_op->setBound(key_record->getNdbRecord(), * bounds[i]);
        }
      } else {
        DEBUG_PRINT("Error code: %d %s", tx->getNdbError().code, tx->getNdbError().message);
      }
    }
    else {
      scan_op = scanTable(tx);
    }
    if(blobHandler) {
      blobHandler->prepare(scan_op);
    }
  }
}

int ScanOperation::fetchResults(char * buffer, bool forceSend) {
  int r = scan_op->nextResultCopyOut(buffer, true, forceSend);
  DEBUG_PRINT("fetchResults: %d", r);
  return r;
}

int ScanOperation::nextResult(char * buffer) {
  return scan_op->nextResultCopyOut(buffer, false, false);
}

void ScanOperation::close() {
  scan_op->close();
  scan_op = index_scan_op = 0;
}

const NdbError & ScanOperation::getNdbError() {
  return scan_op ? scan_op->getNdbError() : ctx->getNdbError();
}

