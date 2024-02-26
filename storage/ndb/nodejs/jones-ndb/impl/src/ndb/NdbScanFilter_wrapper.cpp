/*
 Copyright (c) 2013, 2023, Oracle and/or its affiliates.
 
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
#include "NdbJsConverters.h"


V8WrapperFn begin;
V8WrapperFn end;
V8WrapperFn istrue;
V8WrapperFn isfalse;
V8WrapperFn cmp;
V8WrapperFn isnull;
V8WrapperFn isnotnull;
V8WrapperFn getInterpretedCode;
V8WrapperFn getNdbOperation;

#define WRAPPER_FUNCTION(A) addMethod(#A, A)

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
    addMethod("getNdbError", getNdbError<NdbScanFilter>);
  }
};

NdbScanFilterEnvelopeClass NdbScanFilterEnvelope;

void newNdbScanFilter(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  
  PROHIBIT_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(1);

  JsValueConverter<NdbInterpretedCode *> arg0(args[0]);
  
  NdbScanFilter * f = new NdbScanFilter(arg0.toC());
  
  Local<Value> jsObject = NdbScanFilterEnvelope.wrap(f);
  NdbScanFilterEnvelope.freeFromGC(f, jsObject);
  args.GetReturnValue().Set(scope.Escape(jsObject));
}


void begin(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<int, NdbScanFilter, NdbScanFilter::Group> NCALL;
  NCALL ncall(& NdbScanFilter::begin, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void end(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_0_<int, NdbScanFilter> NCALL;
  NCALL ncall(& NdbScanFilter::end, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void istrue(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_0_<int, NdbScanFilter> NCALL;
  NCALL ncall(& NdbScanFilter::istrue, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void isfalse(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_0_<int, NdbScanFilter> NCALL;
  NCALL ncall(& NdbScanFilter::isfalse, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}


/* cmp() 
   ARG0: BinaryCondition
   ARG1: Column ID
   ARG2: Buffer
   ARG3: Offset
   ARG4: Length
*/
void cmp(const Arguments &args) {
  NdbScanFilter * filter = unwrapPointer<NdbScanFilter *>(args.Holder());
  int condition   = GetInt32Arg(args, 0);
  int columnId    = GetUint32Arg(args, 1);
  char * buffer   = GetBufferData(ArgToObject(args, 2));
  uint32_t offset = GetUint32Arg(args, 3);
  uint32_t length = GetUint32Arg(args, 4);

  int rval = filter->cmp(NdbScanFilter::BinaryCondition(condition), 
                         columnId, buffer + offset, length);

  args.GetReturnValue().Set(rval);
}


void isnull(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<int, NdbScanFilter, int> NCALL;
  NCALL ncall(& NdbScanFilter::isnull, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void isnotnull(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<int, NdbScanFilter, int> NCALL;
  NCALL ncall(& NdbScanFilter::isnotnull, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void getInterpretedCode(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeConstMethodCall_0_<const NdbInterpretedCode *, NdbScanFilter> NCALL;
  NCALL ncall(& NdbScanFilter::getInterpretedCode, args);
  ncall.wrapReturnValueAs(getConstNdbInterpretedCodeEnvelope());
  ncall.run();  
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void getNdbOperation(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeConstMethodCall_0_<NdbOperation *, NdbScanFilter> NCALL;
  NCALL ncall(& NdbScanFilter::getNdbOperation, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}


#define WRAP_CONSTANT(X) DEFINE_JS_INT(sfObj, #X, NdbScanFilter::X)

void NdbScanFilter_initOnLoad(Local<Object> target) {
  Local<Object> sfObj = Object::New(target->GetIsolate());

  SetProp(target, "NdbScanFilter", sfObj);

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

