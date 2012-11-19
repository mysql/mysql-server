/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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
#include <v8.h>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NativeMethodCall.h"
#include "unified_debug.h"
#include "JsWrapper.h"
#include "NdbWrappers.h"
#include "Operation.h"


using namespace v8;

Handle<Value> readTuple(const Arguments &);
Handle<Value> readCurrentTuple(const Arguments &);
Handle<Value> writeTuple(const Arguments &);
Handle<Value> insertTuple(const Arguments &);
Handle<Value> updateTuple(const Arguments &);
Handle<Value> deleteTuple(const Arguments &);
Handle<Value> scanTable(const Arguments &);
Handle<Value> scanIndex(const Arguments &);


class OperationEnvelopeClass : public Envelope {
public:
  OperationEnvelopeClass() : Envelope("Operation") {
//   DEFINE_JS_FUNCTION(Envelope::stencil, "useColumn", useColumn);
   DEFINE_JS_FUNCTION(Envelope::stencil, "readTuple", readTuple);
//   DEFINE_JS_FUNCTION(Envelope::stencil, "readCurrentTuple", readCurrentTuple);
   DEFINE_JS_FUNCTION(Envelope::stencil, "writeTuple", writeTuple);
   DEFINE_JS_FUNCTION(Envelope::stencil, "insertTuple", insertTuple);
   DEFINE_JS_FUNCTION(Envelope::stencil, "deleteTuple", deleteTuple);
   DEFINE_JS_FUNCTION(Envelope::stencil, "updateTuple", updateTuple);
//   DEFINE_JS_FUNCTION(Envelope::stencil, "scanTable", scanTable);
//   DEFINE_JS_FUNCTION(Envelope::stencil, "scanIndex", scanIndex);   
  }
};

OperationEnvelopeClass OperationEnvelope;

Handle<Value> Operation_Wrapper(Operation *op) {
  HandleScope scope;
  
  Local<Object> jsobj = OperationEnvelope.newWrapper();
  wrapPointerInObject(op, OperationEnvelope, jsobj);
  return scope.Close(jsobj);
}


Handle<Value> updateTuple(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
    
  REQUIRE_ARGS_LENGTH(1);
  Operation * op = unwrapPointer<Operation *>(args.Holder());

  JsValueConverter<NdbTransaction *> arg0c(args[0]);

  NdbTransaction * tx = arg0c.toC();

  const NdbOperation * ndbop = op->updateTuple(tx);
  return scope.Close(NdbOperation_Wrapper(ndbop));
}


Handle<Value> writeTuple(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
    
  REQUIRE_ARGS_LENGTH(1);
  Operation * op = unwrapPointer<Operation *>(args.Holder());

  JsValueConverter<NdbTransaction *> arg0c(args[0]);

  NdbTransaction * tx = arg0c.toC();

  const NdbOperation * ndbop = op->writeTuple(tx);
  return scope.Close(NdbOperation_Wrapper(ndbop));
}

Handle<Value> readTuple(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
    
  REQUIRE_ARGS_LENGTH(1);
  Operation * op = unwrapPointer<Operation *>(args.Holder());

  JsValueConverter<NdbTransaction *> arg0c(args[0]);

  NdbTransaction * tx = arg0c.toC();

  const NdbOperation * ndbop = op->readTuple(tx);
  return scope.Close(NdbOperation_Wrapper(ndbop));
}


Handle<Value> insertTuple(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
    
  REQUIRE_ARGS_LENGTH(1);
  Operation * op = unwrapPointer<Operation *>(args.Holder());

  JsValueConverter<NdbTransaction *> arg0c(args[0]);

  NdbTransaction * tx = arg0c.toC();

  const NdbOperation * ndbop = op->insertTuple(tx);
  return scope.Close(NdbOperation_Wrapper(ndbop));
}


Handle<Value> deleteTuple(const Arguments &args) {
  DEBUG_MARKER(UDEB_DEBUG);
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);
  Operation * op = unwrapPointer<Operation *>(args.Holder());
  JsValueConverter<NdbTransaction *> arg0c(args[0]);
  NdbTransaction * tx = arg0c.toC();
  const NdbOperation * ndbop = op->deleteTuple(tx);
  return scope.Close(NdbOperation_Wrapper(ndbop));
}


  

