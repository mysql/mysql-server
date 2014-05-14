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

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NdbWrapperErrors.h"
#include "NativeMethodCall.h"
#include "ScanOperation.h"
#include "DBTransactionContext.h"

using namespace v8;

ScanOperation::ScanOperation(const Arguments &args) : 
  scan_op(0),
  index_scan_op(0),
  nbounds(0),
  isIndexScan(false)
{
  DEBUG_MARKER(UDEB_DEBUG);

  Local<Value> v;

  const Local<Object> spec = args[0]->ToObject();
  int opcode = args[1]->Int32Value();
  ctx = unwrapPointer<DBTransactionContext *>(args[2]->ToObject());

  lmode = NdbOperation::LM_CommittedRead;
  scan_options.optionsPresent = 0ULL;

  v = spec->Get(SCAN_TABLE_RECORD);
  if(! v->IsNull()) {
    Local<Object> o = v->ToObject();
    row_record = unwrapPointer<const Record *>(o);
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
    lmode = static_cast<NdbOperation::LockMode>(intLockMode);
  }

  // SCAN_BOUNDS is an array of BoundHelpers  
  v = spec->Get(SCAN_BOUNDS);
  if(v->IsArray()) {
    Local<Object> o = v->ToObject();
    while(o->Has(nbounds)) {
      nbounds++; 
    }
    bounds = new NdbIndexScanOperation::IndexBound *[nbounds];
    for(int i = 0 ; i < nbounds ; i++) {
      Local<Object> b = o->Get(i)->ToObject();
      bounds[i] = unwrapPointer<NdbIndexScanOperation::IndexBound *>(b);
    }
  }

  v = spec->Get(SCAN_OPTION_FLAGS);
  if(! v->IsNull()) {
    scan_options.scan_flags = v->Uint32Value();
    scan_options.optionsPresent |= NdbScanOperation::ScanOptions::SO_SCANFLAGS;
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
    scan_options.optionsPresent |= NdbScanOperation::ScanOptions::SO_SCANFLAGS;    
  }
  
  /* Done defining the object */
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
      for(int i = 0 ; i < nbounds ; i++) {
        // SetBound could return an error
        index_scan_op->setBound(key_record->getNdbRecord(), * bounds[i]);
      }
    }
    else {
      scan_op = scanTable(tx);
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

