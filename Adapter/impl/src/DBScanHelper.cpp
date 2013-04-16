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


#include <NdbApi.hpp>

#include <node.h>
#include <node_buffer.h>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "unified_debug.h"
#include "JsWrapper.h"
#include "Operation.h"
#include "NdbWrappers.h"
#include "NdbWrapperErrors.h"

using namespace v8;

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

enum { 
  OP_SCAN_READ   = 33,
  OP_SCAN_COUNT  = 34,
  OP_SCAN_DELETE = 48
};

/* 
   arg0: DBScan HelperSpec
   arg1: opcode
   arg2: NdbTransaction *
*/
Handle<Value> DBScanHelper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;

  Local<Value> v;

  const Local<Object> spec = args[0]->ToObject();
  int opcode = args[1]->Int32Value();
  NdbTransaction *tx = unwrapPointer<NdbTransaction *>(args[2]->ToObject());

  NdbScanOperation::ScanOptions options;
  Operation op;
  op.lmode = NdbOperation::LM_CommittedRead;
  op.scan_options = & options;
  options.optionsPresent = 0ULL;

  NdbScanOperation * scan_op = 0;
  NdbIndexScanOperation::IndexBound *bound = 0;
  bool isIndexScan = false;

  v = spec->Get(SCAN_TABLE_RECORD);
  if(! v->IsNull()) {
    Local<Object> o = v->ToObject();
    op.row_record = unwrapPointer<Record *>(o);
  }

  v = spec->Get(SCAN_INDEX_RECORD);
  if(! v->IsNull()) {
    Local<Object> o = v->ToObject();
    isIndexScan = true;
    op.key_record = unwrapPointer<Record *>(o);
  }
  
  v = spec->Get(SCAN_LOCK_MODE);
  if(! v->IsNull()) {
    int intLockMode = v->Int32Value();
    op.lmode = static_cast<NdbOperation::LockMode>(intLockMode);
  }

  v = spec->Get(SCAN_BOUNDS);
  if(! v->IsNull()) {
    Local<Object> o = v->ToObject();
    bound = unwrapPointer<NdbIndexScanOperation::IndexBound *>(o);
  }

  v = spec->Get(SCAN_OPTION_FLAGS);
  if(! v->IsNull()) {
    options.scan_flags = v->Uint32Value();
    options.optionsPresent |= NdbScanOperation::ScanOptions::SO_SCANFLAGS;
  }
  
  v = spec->Get(SCAN_OPTION_BATCH_SIZE);
  if(! v->IsNull()) {
    options.batch = v->Uint32Value();
    options.optionsPresent |= NdbScanOperation::ScanOptions::SO_BATCH;
  }
  
  v = spec->Get(SCAN_OPTION_PARALLELISM);
  if(! v->IsNull()) {
    options.parallel = v->Uint32Value();
    options.optionsPresent |= NdbScanOperation::ScanOptions::SO_PARALLEL;
  }
  
  v = spec->Get(SCAN_FILTER_CODE);
  if(! v->IsNull()) {
    Local<Object> o = v->ToObject();
    options.interpretedCode = unwrapPointer<NdbInterpretedCode *>(o);
    options.optionsPresent |= NdbScanOperation::ScanOptions::SO_INTERPRETED;
  }

  /* Scanning delete requires key info */
  if(opcode == OP_SCAN_DELETE) {
    options.scan_flags |= NdbScanOperation::SF_KeyInfo;
    options.optionsPresent |= NdbScanOperation::ScanOptions::SO_SCANFLAGS;    
  }

  if(isIndexScan) {
    scan_op = op.scanIndex(tx, bound);
  }
  else {
    scan_op = op.scanTable(tx);
  }
  
  if(scan_op) return scope.Close(NdbScanOperation_Wrapper(scan_op));
  else return Null();
}


#define WRAP_CONSTANT(TARGET, X) DEFINE_JS_INT(TARGET, #X, NdbScanOperation::X)

void ScanHelper_initOnLoad(Handle<Object> target) {
  Persistent<Object> scanObj = Persistent<Object>(Object::New());
  Persistent<String> scanKey = Persistent<String>(String::NewSymbol("Scan"));
  target->Set(scanKey, scanObj);

  DEFINE_JS_FUNCTION(scanObj, "create", DBScanHelper);

  Persistent<Object> ScanHelper = Persistent<Object>(Object::New());
  Persistent<Object> ScanFlags = Persistent<Object>(Object::New());
  
  scanObj->Set(Persistent<String>(String::NewSymbol("helper")), ScanHelper);
  scanObj->Set(Persistent<String>(String::NewSymbol("flags")), ScanFlags);

  WRAP_CONSTANT(ScanFlags, SF_TupScan);
  WRAP_CONSTANT(ScanFlags, SF_DiskScan);
  WRAP_CONSTANT(ScanFlags, SF_OrderBy);
  WRAP_CONSTANT(ScanFlags, SF_OrderByFull);
  WRAP_CONSTANT(ScanFlags, SF_Descending);
  WRAP_CONSTANT(ScanFlags, SF_ReadRangeNo);
  WRAP_CONSTANT(ScanFlags, SF_MultiRange);
  WRAP_CONSTANT(ScanFlags, SF_KeyInfo);
  
  DEFINE_JS_INT(ScanHelper, "table_record", SCAN_TABLE_RECORD);
  DEFINE_JS_INT(ScanHelper, "index_record", SCAN_INDEX_RECORD);
  DEFINE_JS_INT(ScanHelper, "lock_mode", SCAN_LOCK_MODE);
  DEFINE_JS_INT(ScanHelper, "bounds", SCAN_BOUNDS);
  DEFINE_JS_INT(ScanHelper, "flags", SCAN_OPTION_FLAGS);
  DEFINE_JS_INT(ScanHelper, "batch_size", SCAN_OPTION_BATCH_SIZE);
  DEFINE_JS_INT(ScanHelper, "parallel", SCAN_OPTION_PARALLELISM);
  DEFINE_JS_INT(ScanHelper, "filter_code", SCAN_FILTER_CODE);
}

