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

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "Record.h"
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"
#include "ScanOperation.h"


V8WrapperFn newScanOperation;
V8WrapperFn prepareAndExecute;
V8WrapperFn getOperationError;
V8WrapperFn scanNextResult;
V8WrapperFn scanFetchResults;
V8WrapperFn ScanOperation_close;
V8WrapperFn getNdbError;
V8WrapperFn ScanOp_readBlobResults;

class ScanOperationEnvelopeClass : public Envelope {
public: 
  ScanOperationEnvelopeClass() : Envelope("ScanOperation") {
    addMethod("getNdbError", getNdbError<ScanOperation>);
    addMethod("prepareAndExecute", prepareAndExecute);
    addMethod("fetchResults", scanFetchResults);
    addMethod("nextResult", scanNextResult);
    addMethod("close", ScanOperation_close);
    addMethod("readBlobResults", ScanOp_readBlobResults);
  }
};

ScanOperationEnvelopeClass ScanOperationEnvelope;

Envelope * getScanOperationEnvelope() {
  return & ScanOperationEnvelope;
}


// Constructor wrapper
void newScanOperation(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());
  ScanOperation * s = new ScanOperation(args);
  Local<Value> wrapper = ScanOperationEnvelope.wrap(s);
  // freeFromGC: Disabled as it leads to segfaults during garbage collection
  // ScanOperationEnvelope.freeFromGC(helper, wrapper);
  args.GetReturnValue().Set(scope.Escape(wrapper));
}

// void prepareAndExecute() 
// ASYNC
void prepareAndExecute(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(1);
  typedef NativeMethodCall_0_<int, ScanOperation> MCALL;
  MCALL * mcallptr = new MCALL(& ScanOperation::prepareAndExecute, args);
  mcallptr->errorHandler = getNdbErrorIfLessThanZero;
  mcallptr->runAsync();
  
  args.GetReturnValue().SetUndefined();
}

// void close()
// ASYNC
void ScanOperation_close(const Arguments & args) {
  typedef NativeVoidMethodCall_0_<ScanOperation> NCALL;
  NCALL * ncallptr = new NCALL(& ScanOperation::close, args);
  ncallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}

// int nextResult(buffer) 
// IMMEDIATE
void scanNextResult(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<int, ScanOperation, char *> MCALL;
  MCALL mcall(& ScanOperation::nextResult, args);
  mcall.run();
  args.GetReturnValue().Set(scope.Escape(mcall.jsReturnVal()));
}


// int fetchResults(buffer, forceSend, callback) 
// ASYNC; CALLBACK GETS (Null-Or-Error, Int) 
void scanFetchResults(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  REQUIRE_ARGS_LENGTH(3);
  typedef NativeMethodCall_2_<int, ScanOperation, char *, bool> MCALL;
  MCALL * ncallptr = new MCALL(& ScanOperation::fetchResults, args);
  ncallptr->errorHandler = getNdbErrorIfLessThanZero;
  ncallptr->runAsync();
  args.GetReturnValue().SetUndefined();
}

void ScanOp_readBlobResults(const Arguments & args) {
  ScanOperation * op = unwrapPointer<ScanOperation *>(args.Holder());
  op->readBlobResults(args);
}

#define WRAP_CONSTANT(TARGET, X) DEFINE_JS_INT(TARGET, #X, NdbScanOperation::X)

void ScanHelper_initOnLoad(Local<Object> target) {
  Local<Object> scanObj = Object::New(Isolate::GetCurrent());
  SetProp(target, "Scan", scanObj);

  DEFINE_JS_FUNCTION(scanObj, "create", newScanOperation);

  Local<Object> ScanHelper = Object::New(Isolate::GetCurrent());
  Local<Object> ScanFlags =  Object::New(Isolate::GetCurrent());

  SetProp(scanObj, "helper", ScanHelper);
  SetProp(scanObj, "flags", ScanFlags);

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

