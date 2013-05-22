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

#include <string.h>

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"

#include "NdbWrapperErrors.h"
#include "NativeMethodCall.h"

using namespace v8;

class FetchResultsCall;  // forward declaration 

/* int nextResult(buffer) 
 * IMMEDIATE
 */
Handle<Value> scanNextResult(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;

  NdbScanOperation * scanop = unwrapPointer<NdbScanOperation *>(args.Holder());
  char * buffer = node::Buffer::Data(args[0]->ToObject());
  
  int rval = scanop->nextResultCopyOut(buffer, false, false);
  return scope.Close(Integer::New(rval));
}


/* int fetchResults(buffer, forceSend, callback) 
 * ASYNC
 * CALLBACK GETS (Null-Or-Error, Int) 
 */
class FetchResultsCall : 
  public NativeMethodCall_2_<int, NdbScanOperation, char *, bool> {
public:
  /* Constructor */
  FetchResultsCall(const Arguments & args) :
    NativeMethodCall_2_<int, NdbScanOperation, char *, bool>(NULL, args)
  {
    errorHandler = getNdbErrorIfLessThanZero<int, NdbScanOperation>;
  };
  
  /* Methods */
  void run() {
    return_val = native_obj->nextResultCopyOut(arg0, true, arg1);  
  }
};

Handle<Value> scanFetchResults(const Arguments & args) { 
  DEBUG_MARKER(UDEB_DETAIL);
  REQUIRE_ARGS_LENGTH(3);
  
  FetchResultsCall * ncallptr = new FetchResultsCall(args);
  ncallptr->runAsync();
  return Undefined();
}


