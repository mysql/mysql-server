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

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "Record.h"
#include "NativeMethodCall.h"
#include "NdbWrapperErrors.h"
#include "NdbJsConverters.h"

using namespace v8;

typedef Handle<Value> _Wrapper_(const Arguments &);

_Wrapper_ begin;
_Wrapper_ end;
_Wrapper_ istrue;
_Wrapper_ isfalse;
_Wrapper_ cmp;
_Wrapper_ isnull;
_Wrapper_ isnotnull;
_Wrapper_ getInterpretedCode;
_Wrapper_ getNdbOperation;

#define WRAPPER_FUNCTION(A) DEFINE_JS_FUNCTION(Envelope::stencil, #A, A)

class NdbScanFilterEnvelopeClass : public Envelope {
public:
  NdbScanFilterEnvelopeClass() : Envelope("NdbScanFilter") {
    WRAPPER_FUNCTION( begin);
    WRAPPER_FUNCTION( end);
    WRAPPER_FUNCTION( istrue);
    WRAPPER_FUNCTION( isfalse);
    WRAPPER_FUNCTION( cmp);
    WRAPPER_FUNCTION( isnull);
    WRAPPER_FUNCTION( isnotnull);
    WRAPPER_FUNCTION( getInterpretedCode);
    WRAPPER_FUNCTION( getNdbOperation);
    DEFINE_JS_FUNCTION(Envelope::stencil, "getNdbError", getNdbError<NdbScanFilter>);
  }
};

NdbScanFilterEnvelopeClass NdbScanFilterEnvelope;

Handle<Value> newNdbScanFilter(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  PROHIBIT_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(1);

  JsValueConverter<NdbInterpretedCode *> arg0(args[0]);
  
  NdbScanFilter * f = new NdbScanFilter(arg0.toC());
  
  Local<Object> jsObject = NdbScanFilterEnvelope.newWrapper();
  wrapPointerInObject(f, NdbScanFilterEnvelope, jsObject);
  freeFromGC(f, jsObject);
  return scope.Close(jsObject);
}


Handle<Value> begin(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, NdbScanFilter, NdbScanFilter::Group> NCALL;
  NCALL ncall(& NdbScanFilter::begin, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> end(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_0_<int, NdbScanFilter> NCALL;
  NCALL ncall(& NdbScanFilter::end, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> istrue(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_0_<int, NdbScanFilter> NCALL;
  NCALL ncall(& NdbScanFilter::istrue, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> isfalse(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_0_<int, NdbScanFilter> NCALL;
  NCALL ncall(& NdbScanFilter::isfalse, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}


/* cmp() 
   ARG0: BinaryCondition
   ARG1: Column ID
   ARG2: Buffer
   ARG3: Offset
   ARG4: Length
*/
Handle<Value> cmp(const Arguments &args) {
  HandleScope scope;

  NdbScanFilter * filter = unwrapPointer<NdbScanFilter *>(args.Holder());
  int condition = args[0]->Int32Value();
  int columnId  = args[1]->Uint32Value();
  char * buffer = node::Buffer::Data(args[2]->ToObject());
  size_t offset = args[3]->Uint32Value();
  size_t length = args[4]->Uint32Value();

  int rval = filter->cmp(NdbScanFilter::BinaryCondition(condition), 
                         columnId, buffer + offset, length);

  return scope.Close(toJS(rval));
}


Handle<Value> isnull(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, NdbScanFilter, int> NCALL;
  NCALL ncall(& NdbScanFilter::isnull, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> isnotnull(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, NdbScanFilter, int> NCALL;
  NCALL ncall(& NdbScanFilter::isnotnull, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> getInterpretedCode(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeConstMethodCall_0_<const NdbInterpretedCode *, NdbScanFilter> NCALL;
  NCALL ncall(& NdbScanFilter::getInterpretedCode, args);
  ncall.wrapReturnValueAs(getConstNdbInterpretedCodeEnvelope());
  ncall.run();  
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> getNdbOperation(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeConstMethodCall_0_<NdbOperation *, NdbScanFilter> NCALL;
  NCALL ncall(& NdbScanFilter::getNdbOperation, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}


#define WRAP_CONSTANT(X) DEFINE_JS_INT(sfObj, #X, NdbScanFilter::X)
void NdbScanFilter_initOnLoad(Handle<Object> target) {  
  HandleScope scope;

  Persistent<String> sfKey = Persistent<String>(String::NewSymbol("NdbScanFilter"));
  Persistent<Object> sfObj = Persistent<Object>(Object::New());

  target->Set(sfKey, sfObj);

  DEFINE_JS_FUNCTION(sfObj, "create", newNdbScanFilter);
  WRAP_CONSTANT(AND);
  WRAP_CONSTANT(OR);
  WRAP_CONSTANT(NAND);
  WRAP_CONSTANT(NOR);
  WRAP_CONSTANT(COND_LE);
  WRAP_CONSTANT(COND_LT);
  WRAP_CONSTANT(COND_GE);
  WRAP_CONSTANT(COND_GT);
  WRAP_CONSTANT(COND_EQ);
  WRAP_CONSTANT(COND_NE);
  WRAP_CONSTANT(COND_LIKE);
  WRAP_CONSTANT(COND_NOT_LIKE);
  WRAP_CONSTANT(COND_AND_EQ_MASK);
  WRAP_CONSTANT(COND_AND_NE_MASK);
  WRAP_CONSTANT(COND_AND_EQ_ZERO);
  WRAP_CONSTANT(COND_AND_NE_ZERO);
  WRAP_CONSTANT(FilterTooLarge);
}

