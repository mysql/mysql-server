/*
 Copyright (c) 2013, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <cstddef>

#include <NdbApi.hpp>

#include "NativeMethodCall.h"
#include "NdbJsConverters.h"
#include "NdbWrapperErrors.h"
#include "adapter_global.h"
#include "js_wrapper_macros.h"

V8WrapperFn load_const_null;
V8WrapperFn load_const_u16;
V8WrapperFn load_const_u32;
// V8WrapperFn load_const_u64;   // not wrapped
V8WrapperFn read_attr;
V8WrapperFn write_attr;
V8WrapperFn add_reg;
V8WrapperFn sub_reg;
V8WrapperFn def_label;
V8WrapperFn branch_label;
V8WrapperFn branch_ge;
V8WrapperFn branch_gt;
V8WrapperFn branch_le;
V8WrapperFn branch_lt;
V8WrapperFn branch_eq;
V8WrapperFn branch_ne;
V8WrapperFn branch_ne_null;
V8WrapperFn branch_eq_null;
V8WrapperFn branch_col_eq;
V8WrapperFn branch_col_ne;
V8WrapperFn branch_col_lt;
V8WrapperFn branch_col_le;
V8WrapperFn branch_col_gt;
V8WrapperFn branch_col_ge;
V8WrapperFn branch_col_eq_null;
V8WrapperFn branch_col_ne_null;
V8WrapperFn branch_col_like;
V8WrapperFn branch_col_notlike;
V8WrapperFn branch_col_and_mask_eq_mask;
V8WrapperFn branch_col_and_mask_ne_mask;
V8WrapperFn branch_col_and_mask_eq_zero;
V8WrapperFn branch_col_and_mask_ne_zero;
V8WrapperFn interpret_exit_ok;
V8WrapperFn interpret_exit_nok;
V8WrapperFn interpret_exit_last_row;
V8WrapperFn add_val;
V8WrapperFn sub_val;
V8WrapperFn def_sub;
V8WrapperFn call_sub;
V8WrapperFn ret_sub;
V8WrapperFn finalise;
V8WrapperFn
    NdbInterpretedCode_getTable_wrapper;  // rename to avoid duplicate symbol
V8WrapperFn getNdbError;
V8WrapperFn getWordsUsed;
// V8WrapperFn copy; // not wrapped

#define WRAPPER_FUNCTION(A) addMethod(#A, A)

class NdbInterpretedCodeEnvelopeClass : public Envelope {
 public:
  NdbInterpretedCodeEnvelopeClass() : Envelope("NdbInterpretedCode") {
    EscapableHandleScope scope(v8::Isolate::GetCurrent());
    WRAPPER_FUNCTION(load_const_null);
    WRAPPER_FUNCTION(load_const_u16);
    WRAPPER_FUNCTION(load_const_u32);
    // WRAPPER_FUNCTION( load_const_u64);
    WRAPPER_FUNCTION(read_attr);
    WRAPPER_FUNCTION(write_attr);
    WRAPPER_FUNCTION(add_reg);
    WRAPPER_FUNCTION(sub_reg);
    WRAPPER_FUNCTION(def_label);
    WRAPPER_FUNCTION(branch_label);
    WRAPPER_FUNCTION(branch_ge);
    WRAPPER_FUNCTION(branch_gt);
    WRAPPER_FUNCTION(branch_le);
    WRAPPER_FUNCTION(branch_lt);
    WRAPPER_FUNCTION(branch_eq);
    WRAPPER_FUNCTION(branch_ne);
    WRAPPER_FUNCTION(branch_ne_null);
    WRAPPER_FUNCTION(branch_eq_null);
    WRAPPER_FUNCTION(branch_col_eq);
    WRAPPER_FUNCTION(branch_col_ne);
    WRAPPER_FUNCTION(branch_col_lt);
    WRAPPER_FUNCTION(branch_col_le);
    WRAPPER_FUNCTION(branch_col_gt);
    WRAPPER_FUNCTION(branch_col_ge);
    WRAPPER_FUNCTION(branch_col_eq_null);
    WRAPPER_FUNCTION(branch_col_ne_null);
    WRAPPER_FUNCTION(branch_col_like);
    WRAPPER_FUNCTION(branch_col_notlike);
    WRAPPER_FUNCTION(branch_col_and_mask_eq_mask);
    WRAPPER_FUNCTION(branch_col_and_mask_ne_mask);
    WRAPPER_FUNCTION(branch_col_and_mask_eq_zero);
    WRAPPER_FUNCTION(branch_col_and_mask_ne_zero);
    WRAPPER_FUNCTION(interpret_exit_ok);
    WRAPPER_FUNCTION(interpret_exit_nok);
    WRAPPER_FUNCTION(interpret_exit_last_row);
    WRAPPER_FUNCTION(add_val);
    WRAPPER_FUNCTION(sub_val);
    WRAPPER_FUNCTION(def_sub);
    WRAPPER_FUNCTION(call_sub);
    WRAPPER_FUNCTION(ret_sub);
    WRAPPER_FUNCTION(finalise);
    WRAPPER_FUNCTION(getWordsUsed);
    // WRAPPER_FUNCTION( copy);   // not wrapped
    addMethod("getTable", NdbInterpretedCode_getTable_wrapper);
    addMethod("getNdbError", getNdbError<NdbInterpretedCode>);
  }
};

NdbInterpretedCodeEnvelopeClass NdbInterpretedCodeEnvelope;

/* The const version has no methods attached: */
Envelope ConstNdbInterpretedCodeEnvelope("const NdbInterpretedCode");

Envelope *getConstNdbInterpretedCodeEnvelope() {
  return &ConstNdbInterpretedCodeEnvelope;
}

void newNdbInterpretedCode(const Arguments &args) {
  EscapableHandleScope scope(args.GetIsolate());

  DEBUG_MARKER(UDEB_DETAIL);
  PROHIBIT_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(1);

  JsValueConverter<const NdbDictionary::Table *> arg0(args[0]);
  NdbInterpretedCode *c = new NdbInterpretedCode(arg0.toC());
  Local<Value> jsObject = NdbInterpretedCodeEnvelope.wrap(c);
  NdbInterpretedCodeEnvelope.freeFromGC(c, jsObject);
  args.GetReturnValue().Set(scope.Escape(jsObject));
}

void load_const_null(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, uint32_t> NCALL;
  NCALL ncall(&NdbInterpretedCode::load_const_null, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void load_const_u16(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::load_const_u16, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void load_const_u32(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::load_const_u32, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

/* TODO: read_attr and write_attr have two forms */
void read_attr(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t,
                              const NdbDictionary::Column *>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::read_attr, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void write_attr(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_2_<int, NdbInterpretedCode,
                              const NdbDictionary::Column *, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::write_attr, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void add_reg(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, uint32_t, uint32_t,
                              uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::add_reg, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void sub_reg(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, uint32_t, uint32_t,
                              uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::sub_reg, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void def_label(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, int> NCALL;
  NCALL ncall(&NdbInterpretedCode::def_label, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void branch_label(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, uint32_t> NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_label, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void branch_ge(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, uint32_t, uint32_t,
                              uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_ge, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void branch_gt(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, uint32_t, uint32_t,
                              uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_gt, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void branch_le(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, uint32_t, uint32_t,
                              uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_le, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void branch_lt(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, uint32_t, uint32_t,
                              uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_lt, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void branch_eq(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, uint32_t, uint32_t,
                              uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_eq, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void branch_ne(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_3_<int, NdbInterpretedCode, uint32_t, uint32_t,
                              uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_ne, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void branch_ne_null(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_ne_null, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void branch_eq_null(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_eq_null, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
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
const void *getValueAddr(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  Local<Object> buffer = ArgToObject(args, 0);
  size_t offset = GetUint32Arg(args, 1);
  return GetBufferData(buffer) + offset;
}

void branch_col_eq(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  const void *val = getValueAddr(args);
  NdbInterpretedCode *code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval =
      code->branch_col_eq(val, 0, GetUint32Arg(args, 2), GetUint32Arg(args, 3));
  args.GetReturnValue().Set(rval);
}

void branch_col_ne(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  const void *val = getValueAddr(args);
  NdbInterpretedCode *code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval =
      code->branch_col_ne(val, 0, GetUint32Arg(args, 2), GetUint32Arg(args, 3));
  args.GetReturnValue().Set(rval);
}

void branch_col_lt(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  const void *val = getValueAddr(args);
  NdbInterpretedCode *code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval =
      code->branch_col_lt(val, 0, GetUint32Arg(args, 2), GetUint32Arg(args, 3));
  args.GetReturnValue().Set(rval);
}

void branch_col_le(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  const void *val = getValueAddr(args);
  NdbInterpretedCode *code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval =
      code->branch_col_le(val, 0, GetUint32Arg(args, 2), GetUint32Arg(args, 3));
  args.GetReturnValue().Set(rval);
}

void branch_col_gt(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  const void *val = getValueAddr(args);
  NdbInterpretedCode *code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval =
      code->branch_col_gt(val, 0, GetUint32Arg(args, 2), GetUint32Arg(args, 3));
  args.GetReturnValue().Set(rval);
}

void branch_col_ge(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  const void *val = getValueAddr(args);
  NdbInterpretedCode *code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval =
      code->branch_col_ge(val, 0, GetUint32Arg(args, 2), GetUint32Arg(args, 3));
  args.GetReturnValue().Set(rval);
}

void branch_col_and_mask_eq_mask(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  const void *val = getValueAddr(args);
  NdbInterpretedCode *code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_and_mask_eq_mask(val, 0, GetUint32Arg(args, 2),
                                               GetUint32Arg(args, 3));
  args.GetReturnValue().Set(rval);
}

void branch_col_and_mask_ne_mask(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  const void *val = getValueAddr(args);
  NdbInterpretedCode *code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_and_mask_ne_mask(val, 0, GetUint32Arg(args, 2),
                                               GetUint32Arg(args, 3));
  args.GetReturnValue().Set(rval);
}

void branch_col_and_mask_eq_zero(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  const void *val = getValueAddr(args);
  NdbInterpretedCode *code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_and_mask_eq_zero(val, 0, GetUint32Arg(args, 2),
                                               GetUint32Arg(args, 3));
  args.GetReturnValue().Set(rval);
}

void branch_col_and_mask_ne_zero(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  const void *val = getValueAddr(args);
  NdbInterpretedCode *code = unwrapPointer<NdbInterpretedCode *>(args.Holder());
  int rval = code->branch_col_and_mask_ne_zero(val, 0, GetUint32Arg(args, 2),
                                               GetUint32Arg(args, 3));
  args.GetReturnValue().Set(rval);
}

/****************************************************************
 *    Back to generic wrappers
 ****************************************************************/
void branch_col_eq_null(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_col_eq_null, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void branch_col_ne_null(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_col_ne_null, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

// FIXME: arg[0] needs to be converted from String
void branch_col_like(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_4_<int, NdbInterpretedCode, const void *, uint32_t,
                              uint32_t, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_col_like, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

// FIXME: arg[0] needs to be converted from String
void branch_col_notlike(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_4_<int, NdbInterpretedCode, const void *, uint32_t,
                              uint32_t, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::branch_col_notlike, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

/****************************************************************
 *   End of column/value branch instructions
 ****************************************************************/

void interpret_exit_ok(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_0_<int, NdbInterpretedCode> NCALL;
  NCALL ncall(&NdbInterpretedCode::interpret_exit_ok, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void interpret_exit_nok(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, uint32_t> NCALL;
  NCALL ncall(&NdbInterpretedCode::interpret_exit_nok, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void interpret_exit_last_row(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_0_<int, NdbInterpretedCode> NCALL;
  NCALL ncall(&NdbInterpretedCode::interpret_exit_last_row, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void add_val(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::add_val, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void sub_val(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_2_<int, NdbInterpretedCode, uint32_t, uint32_t>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::sub_val, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void def_sub(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, uint32_t> NCALL;
  NCALL ncall(&NdbInterpretedCode::def_sub, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void call_sub(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_1_<int, NdbInterpretedCode, uint32_t> NCALL;
  NCALL ncall(&NdbInterpretedCode::call_sub, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void ret_sub(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_0_<int, NdbInterpretedCode> NCALL;
  NCALL ncall(&NdbInterpretedCode::ret_sub, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void finalise(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeMethodCall_0_<int, NdbInterpretedCode> NCALL;
  NCALL ncall(&NdbInterpretedCode::finalise, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void NdbInterpretedCode_getTable_wrapper(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeConstMethodCall_0_<const NdbDictionary::Table *,
                                   NdbInterpretedCode>
      NCALL;
  NCALL ncall(&NdbInterpretedCode::getTable, args);
  ncall.wrapReturnValueAs(getNdbDictTableEnvelope());
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void getWordsUsed(const Arguments &args) {
  DEBUG_MARKER(UDEB_DETAIL);
  EscapableHandleScope scope(args.GetIsolate());
  typedef NativeConstMethodCall_0_<uint32_t, NdbInterpretedCode> NCALL;
  NCALL ncall(&NdbInterpretedCode::getWordsUsed, args);
  ncall.run();
  args.GetReturnValue().Set(scope.Escape(ncall.jsReturnVal()));
}

void NdbInterpretedCode_initOnLoad(Local<Object> target) {
  Local<Object> ic_obj = Object::New(target->GetIsolate());
  SetProp(target, "NdbInterpretedCode", ic_obj);
  DEFINE_JS_FUNCTION(ic_obj, "create", newNdbInterpretedCode);
}
