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
#include "NdbWrapperErrors.h"
#include "NativeMethodCall.h"
#include "NdbJsConverters.h"

using namespace v8;

typedef Handle<Value> _Wrapper_(const Arguments &);

_Wrapper_ load_const_null;
_Wrapper_ load_const_u16;
_Wrapper_ load_const_u32;
// _Wrapper_ load_const_u64;   // not wrapped
_Wrapper_ read_attr;
_Wrapper_ write_attr;
_Wrapper_ add_reg;
_Wrapper_ sub_reg;
_Wrapper_ def_label;
_Wrapper_ branch_label;
_Wrapper_ branch_ge;
_Wrapper_ branch_gt;
_Wrapper_ branch_le;
_Wrapper_ branch_lt;
_Wrapper_ branch_eq;
_Wrapper_ branch_ne;
_Wrapper_ branch_ne_null;
_Wrapper_ branch_eq_null;
_Wrapper_ branch_col_eq;
_Wrapper_ branch_col_ne;
_Wrapper_ branch_col_lt;
_Wrapper_ branch_col_le;
_Wrapper_ branch_col_gt;
_Wrapper_ branch_col_ge;
_Wrapper_ branch_col_eq_null;
_Wrapper_ branch_col_ne_null;
_Wrapper_ branch_col_like;
_Wrapper_ branch_col_notlike;
_Wrapper_ branch_col_and_mask_eq_mask;
_Wrapper_ branch_col_and_mask_ne_mask;
_Wrapper_ branch_col_and_mask_eq_zero;
_Wrapper_ branch_col_and_mask_ne_zero;
_Wrapper_ interpret_exit_ok;
_Wrapper_ interpret_exit_nok;
_Wrapper_ interpret_exit_last_row;
_Wrapper_ add_val;
_Wrapper_ sub_val;
_Wrapper_ def_sub;
_Wrapper_ call_sub;
_Wrapper_ ret_sub;
_Wrapper_ finalise;
_Wrapper_ NdbInterpretedCode_getTable_wrapper;   // rename to avoid duplicate symbol
_Wrapper_ getNdbError;
_Wrapper_ getWordsUsed;
// _Wrapper_ copy; // not wrapped

#define WRAPPER_FUNCTION(A) DEFINE_JS_FUNCTION(Envelope::stencil, #A, A)

class NdbInterpretedCodeEnvelopeClass : public Envelope {
public:
  NdbInterpretedCodeEnvelopeClass() : Envelope("NdbInterpretedCode") {
    WRAPPER_FUNCTION( load_const_null);
    WRAPPER_FUNCTION( load_const_u16);
    WRAPPER_FUNCTION( load_const_u32);
    // WRAPPER_FUNCTION( load_const_u64); 
    WRAPPER_FUNCTION( read_attr);
    WRAPPER_FUNCTION( write_attr);
    WRAPPER_FUNCTION( add_reg);
    WRAPPER_FUNCTION( sub_reg);
    WRAPPER_FUNCTION( def_label);
    WRAPPER_FUNCTION( branch_label);
    WRAPPER_FUNCTION( branch_ge);
    WRAPPER_FUNCTION( branch_gt);
    WRAPPER_FUNCTION( branch_le);
    WRAPPER_FUNCTION( branch_lt);
    WRAPPER_FUNCTION( branch_eq);
    WRAPPER_FUNCTION( branch_ne);
    WRAPPER_FUNCTION( branch_ne_null);
    WRAPPER_FUNCTION( branch_eq_null);
    WRAPPER_FUNCTION( branch_col_eq);
    WRAPPER_FUNCTION( branch_col_ne);
    WRAPPER_FUNCTION( branch_col_lt);
    WRAPPER_FUNCTION( branch_col_le);
    WRAPPER_FUNCTION( branch_col_gt);
    WRAPPER_FUNCTION( branch_col_ge);
    WRAPPER_FUNCTION( branch_col_eq_null);
    WRAPPER_FUNCTION( branch_col_ne_null);
    WRAPPER_FUNCTION( branch_col_like);
    WRAPPER_FUNCTION( branch_col_notlike);
    WRAPPER_FUNCTION( branch_col_and_mask_eq_mask);
    WRAPPER_FUNCTION( branch_col_and_mask_ne_mask);
    WRAPPER_FUNCTION( branch_col_and_mask_eq_zero);
    WRAPPER_FUNCTION( branch_col_and_mask_ne_zero);
    WRAPPER_FUNCTION( interpret_exit_ok);
    WRAPPER_FUNCTION( interpret_exit_nok);
    WRAPPER_FUNCTION( interpret_exit_last_row);
    WRAPPER_FUNCTION( add_val);
    WRAPPER_FUNCTION( sub_val);
    WRAPPER_FUNCTION( def_sub);
    WRAPPER_FUNCTION( call_sub);
    WRAPPER_FUNCTION( ret_sub);
    WRAPPER_FUNCTION( finalise);
    WRAPPER_FUNCTION( getWordsUsed);
    // WRAPPER_FUNCTION( copy);   // not wrapped 
    DEFINE_JS_FUNCTION(Envelope::stencil, "getTable",
                       NdbInterpretedCode_getTable_wrapper);
    DEFINE_JS_FUNCTION(Envelope::stencil, "getNdbError", getNdbError<NdbInterpretedCode>);
  }
};

NdbInterpretedCodeEnvelopeClass NdbInterpretedCodeEnvelope;

/* The const version has no methods attached: */
Envelope ConstNdbInterpretedCodeEnvelope("const NdbInterpretedCode");

Envelope * getConstNdbInterpretedCodeEnvelope() {
  return & ConstNdbInterpretedCodeEnvelope;
}

Handle<Value> newNdbInterpretedCode(const Arguments & args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  
  PROHIBIT_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(1);

  JsValueConverter<const NdbDictionary::Table *> arg0(args[0]);
  
  NdbInterpretedCode * c = new NdbInterpretedCode(arg0.toC());
  
  Local<Object> jsObject = NdbInterpretedCodeEnvelope.newWrapper();
  wrapPointerInObject(c, NdbInterpretedCodeEnvelope, jsObject);
  freeFromGC(c, jsObject);
  return scope.Close(jsObject);
}

Handle<Value> load_const_null(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::load_const_null, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> load_const_u16(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::load_const_u16, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> load_const_u32(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::load_const_u32, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}


/* TODO: read_attr and write_attr have two forms */
Handle<Value> read_attr(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, 
                              const NdbDictionary::Column *> NCALL;
  NCALL ncall(& NdbInterpretedCode::read_attr, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> write_attr(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode,  
                              const NdbDictionary::Column *,
                              uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::write_attr, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> add_reg(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, 
                              uint32_t, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::add_reg, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> sub_reg(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, 
                              uint32_t, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::sub_reg, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> def_label(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, int> NCALL;
  NCALL ncall(& NdbInterpretedCode::def_label, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> branch_label(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_label, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> branch_ge(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, 
                              uint32_t, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_ge, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> branch_gt(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, 
                              uint32_t, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_gt, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> branch_le(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, 
                              uint32_t, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_le, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> branch_lt(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, 
                              uint32_t, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_lt, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> branch_eq(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, 
                              uint32_t, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_eq, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> branch_ne(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, 
                              uint32_t, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_ne, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> branch_ne_null(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode,  
                              uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_ne_null, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> branch_eq_null(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode,  
                              uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_eq_null, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}


/****************************************************************
 *     BRANCH ON COLUMN AND VALUE 
 *     These have hand-written wrappers 
 *     ARG0: Buffer
 *     ARG1: Offset
 *     ARG2: AttrID
 *     ARG3: Branch Label
 ****************************************************************/

/* Utility function */
const void * getValueAddr(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  Local<Object> buffer = args[0]->ToObject();
  size_t offset = args[1]->Uint32Value();
  return node::Buffer::Data(buffer) + offset;
}

Handle<Value> branch_col_eq(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_eq(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_ne(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_ne(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_lt(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_lt(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_le(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_le(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_gt(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_gt(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_ge(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_ge(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_and_mask_eq_mask(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_and_mask_eq_mask(
    val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_and_mask_ne_mask(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_and_mask_ne_mask(
    val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_and_mask_eq_zero(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_and_mask_eq_zero(
    val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_and_mask_ne_zero(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_and_mask_ne_zero(
    val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}


/****************************************************************
 *    Back to generic wrappers
 ****************************************************************/
Handle<Value> branch_col_eq_null(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode,  
                              uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_col_eq_null, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> branch_col_ne_null(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode,  
                              uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_col_ne_null, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

// FIXME: arg[0] needs to be converted from String
Handle<Value> branch_col_like(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_4_<int, NdbInterpretedCode,  
                              const void *, uint32_t, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_col_like, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

// FIXME: arg[0] needs to be converted from String
Handle<Value> branch_col_notlike(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_4_<int, NdbInterpretedCode,  
                              const void *, uint32_t, uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::branch_col_notlike, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}


/****************************************************************
 *   End of column/value branch instructions
 ****************************************************************/


Handle<Value> interpret_exit_ok(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_0_<int, NdbInterpretedCode> NCALL;
  NCALL ncall(& NdbInterpretedCode::interpret_exit_ok, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> interpret_exit_nok(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::interpret_exit_nok, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> interpret_exit_last_row(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_0_<int, NdbInterpretedCode> NCALL;
  NCALL ncall(& NdbInterpretedCode::interpret_exit_last_row, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> add_val(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode,  
                              uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::add_val, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> sub_val(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode,  
                              uint32_t, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::sub_val, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> def_sub(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::def_sub, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> call_sub(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, uint32_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::call_sub, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> ret_sub(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_0_<int, NdbInterpretedCode> NCALL;
  NCALL ncall(& NdbInterpretedCode::ret_sub, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> finalise(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_0_<int, NdbInterpretedCode> NCALL;
  NCALL ncall(& NdbInterpretedCode::finalise, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> NdbInterpretedCode_getTable_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeConstMethodCall_0_<const NdbDictionary::Table*, 
                                   NdbInterpretedCode> NCALL;
  NCALL ncall(& NdbInterpretedCode::getTable, args);
  ncall.wrapReturnValueAs(getNdbDictTableEnvelope());
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

Handle<Value> getWordsUsed(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeConstMethodCall_0_<uint32_t, NdbInterpretedCode> NCALL;
  NCALL ncall(& NdbInterpretedCode::getWordsUsed, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}


void NdbInterpretedCode_initOnLoad(Handle<Object> target) {
  HandleScope scope;

  Persistent<String> ic_key = Persistent<String>(String::NewSymbol("NdbInterpretedCode"));
  Persistent<Object> ic_obj = Persistent<Object>(Object::New());

  target->Set(ic_key, ic_obj);

  DEFINE_JS_FUNCTION(ic_obj, "create", newNdbInterpretedCode);
}

