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
#include "Record.h"
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"
#include "ScanOperation.h"

using namespace v8;

Handle<Value> newScanOperation(const Arguments &);
Handle<Value> prepareAndExecute(const Arguments &);
Handle<Value> getOperationError(const Arguments &);
Handle<Value> scanNextResult(const Arguments &);
Handle<Value> scanFetchResults(const Arguments &);
Handle<Value> ScanOperation_close(const Arguments &);
Handle<Value> getNdbError(const Arguments &);

class ScanOperationEnvelopeClass : public Envelope {
public: 
  ScanOperationEnvelopeClass() : Envelope("ScanOperation") {
    DEFINE_JS_FUNCTION(Envelope::stencil, "getNdbError", 
                       getNdbError<ScanOperation>);
    DEFINE_JS_FUNCTION(Envelope::stencil, "prepareAndExecute", prepareAndExecute);
    DEFINE_JS_FUNCTION(Envelope::stencil, "fetchResults", scanFetchResults);
    DEFINE_JS_FUNCTION(Envelope::stencil, "nextResult", scanNextResult);
    DEFINE_JS_FUNCTION(Envelope::stencil, "close", ScanOperation_close);
  }
};

ScanOperationEnvelopeClass ScanOperationEnvelope;

Envelope * getScanOperationEnvelope() {
  return & ScanOperationEnvelope;
}


// Constructor wrapper
Handle<Value> newScanOperation(const Arguments &args) {
  HandleScope scope;
  ScanOperation * s = new ScanOperation(args);
  Local<Object> wrapper = ScanOperationEnvelope.newWrapper();
  wrapPointerInObject(s, ScanOperationEnvelope, wrapper);
  // freeFromGC: Disabled as it leads to segfaults during garbage collection
  // freeFromGC(helper, wrapper);
  return scope.Close(wrapper);
}

// void prepareAndExecute() 
// ASYNC
Handle<Value> prepareAndExecute(const Arguments &args) {
  HandleScope scope;
  DEBUG_MARKER(UDEB_DEBUG);
  REQUIRE_ARGS_LENGTH(1);
  typedef NativeMethodCall_0_<int, ScanOperation> MCALL;
  MCALL * mcallptr = new MCALL(& ScanOperation::prepareAndExecute, args);
  mcallptr->errorHandler = getNdbErrorIfLessThanZero;
  mcallptr->runAsync();
  
  return Undefined();
}

// void close()
// ASYNC
Handle<Value> ScanOperation_close(const Arguments & args) {
  typedef NativeVoidMethodCall_0_<ScanOperation> NCALL;
  NCALL * ncallptr = new NCALL(& ScanOperation::close, args);
  ncallptr->runAsync();
  return Undefined();
}

// int nextResult(buffer) 
// IMMEDIATE
Handle<Value> scanNextResult(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, ScanOperation, char *> MCALL;
  MCALL mcall(& ScanOperation::nextResult, args);
  mcall.run();
  return scope.Close(mcall.jsReturnVal());
}


// int fetchResults(buffer, forceSend, callback) 
// ASYNC; CALLBACK GETS (Null-Or-Error, Int) 
Handle<Value> scanFetchResults(const Arguments & args) { 
  DEBUG_MARKER(UDEB_DETAIL);
  REQUIRE_ARGS_LENGTH(3);
  typedef NativeMethodCall_2_<int, ScanOperation, char *, bool> MCALL;
  MCALL * ncallptr = new MCALL(& ScanOperation::fetchResults, args);
  ncallptr->errorHandler = getNdbErrorIfLessThanZero;
  ncallptr->runAsync();
  return Undefined();
}

#define WRAP_CONSTANT(TARGET, X) DEFINE_JS_INT(TARGET, #X, NdbScanOperation::X)

void ScanHelper_initOnLoad(Handle<Object> target) {
  Persistent<Object> scanObj = Persistent<Object>(Object::New());
  Persistent<String> scanKey = Persistent<String>(String::NewSymbol("Scan"));
  target->Set(scanKey, scanObj);

  DEFINE_JS_FUNCTION(scanObj, "create", newScanOperation);

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

