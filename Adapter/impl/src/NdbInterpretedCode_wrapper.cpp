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

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NativeMethodCall.h"
#include "unified_debug.h"

#include "NdbJsConverters.h"
#include "NdbWrappers.h"
#include "NdbWrapperErrors.h"

using namespace v8;

typedef Handle<Value> _Wrapper_(const Arguments &);

_Wrapper_ load_const_null;
_Wrapper_ load_const_u16;
_Wrapper_ load_const_u32;
_Wrapper_ load_const_u64;
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
_Wrapper_ getTable;
_Wrapper_ getNdbError;
_Wrapper_ getWordsUsed;
_Wrapper_ copy;

#define WRAPPER_FUNCTION(A) DEFINE_JS_FUNCTION(Envelope::stencil, #A, A)


class NdbInterpretedCodeEnvelopeClass : public Envelope {
public:
  NdbInterpretedCodeEnvelopeClass() : Envelope("NdbInterpretedCode") {
    WRAPPER_FUNCTION( load_const_null);
    WRAPPER_FUNCTION( load_const_u16);
    WRAPPER_FUNCTION( load_const_u32);
    WRAPPER_FUNCTION( load_const_u64);
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
    WRAPPER_FUNCTION( getTable);
    WRAPPER_FUNCTION( getNdbError);
    WRAPPER_FUNCTION( getWordsUsed);
    WRAPPER_FUNCTION( copy);
  }
};

NdbInterpretedCodeEnvelopeClass NdbInterpretedCodeEnvelope;

Envelope * getNdbInterpretedCodeEnvelope() {
  return & NdbInterpretedCodeEnvelope;
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

/* TODO: JsValueConverter for Uint64
Handle<Value> load_const_u64(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint64_t> NCALL;
  NCALL ncall(& NdbInterpretedCode::load_const_u64, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}
*/

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
 ****************************************************************/

/* Utility function */
const void * getValueAddr(const Arguments &args) {
  HandleScope scope;
  Local<Object> buffer = args[0]->ToObject();
  size_t offset = args[1]->Uint32Value();
  return node::Buffer::Data(buffer) + offset;
}

Handle<Value> branch_col_eq(const Arguments &args) {
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_eq(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_ne(const Arguments &args) {
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_ne(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_lt(const Arguments &args) {
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_lt(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_le(const Arguments &args) {
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_le(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_gt(const Arguments &args) {
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_gt(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
  return scope.Close(toJS<int>(rval));
}

Handle<Value> branch_col_ge(const Arguments &args) {
  HandleScope scope;
  const void * val = getValueAddr(args);
  NdbInterpretedCode * code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_ge(val, 0, args[2]->Uint32Value(), args[3]->Uint32Value());
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

/** FIXME: Return value conversion
Handle<Value> getTable(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeConstMethodCall_0_<const NdbDictionary::Table*, 
                                   NdbInterpretedCode> NCALL;
  NCALL ncall(& NdbInterpretedCode::getTable, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}
*/

Handle<Value> getWordsUsed(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeConstMethodCall_0_<uint32_t, NdbInterpretedCode> NCALL;
  NCALL ncall(& NdbInterpretedCode::getWordsUsed, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}

/** FIXME: Value conversion 
Handle<Value> copy(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  HandleScope scope;
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, NdbInterpretedCode *> NCALL;
  NCALL ncall(& NdbInterpretedCode::copy, args);
  ncall.run();
  return scope.Close(ncall.jsReturnVal());
}
*/

