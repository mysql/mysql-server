/*
  Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
/*
 * NdbInterpretedCode.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class NdbInterpretedCode extends Wrapper implements NdbInterpretedCodeConst
{
    public final native NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ getTable() /*_const_*/;
    public final native NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
    public final native int/*_Uint32_*/ getWordsUsed() /*_const_*/;
    static public final native NdbInterpretedCode create(NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ table /*_= 0_*/, ByteBuffer/*_Uint32 *_*/ buffer /*_= 0_*/, int/*_Uint32_*/ buffer_word_size /*_= 0_*/);
    static public final native void delete(NdbInterpretedCode p0);
    public final native int load_const_null(int/*_Uint32_*/ RegDest);
    public final native int load_const_u16(int/*_Uint32_*/ RegDest, int/*_Uint32_*/ Constant);
    public final native int load_const_u32(int/*_Uint32_*/ RegDest, int/*_Uint32_*/ Constant);
    public final native int load_const_u64(int/*_Uint32_*/ RegDest, long/*_Uint64_*/ Constant);
    public final native int read_attr(int/*_Uint32_*/ RegDest, int/*_Uint32_*/ attrId);
    public final native int read_attr(int/*_Uint32_*/ RegDest, NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ column);
    public final native int write_attr(int/*_Uint32_*/ attrId, int/*_Uint32_*/ RegSource);
    public final native int write_attr(NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ column, int/*_Uint32_*/ RegSource);
    public final native int add_reg(int/*_Uint32_*/ RegDest, int/*_Uint32_*/ RegSource1, int/*_Uint32_*/ RegSource2);
    public final native int sub_reg(int/*_Uint32_*/ RegDest, int/*_Uint32_*/ RegSource1, int/*_Uint32_*/ RegSource2);
    public final native int def_label(int LabelNum);
    public final native int branch_label(int/*_Uint32_*/ Label);
    public final native int branch_ge(int/*_Uint32_*/ RegLvalue, int/*_Uint32_*/ RegRvalue, int/*_Uint32_*/ Label);
    public final native int branch_gt(int/*_Uint32_*/ RegLvalue, int/*_Uint32_*/ RegRvalue, int/*_Uint32_*/ Label);
    public final native int branch_le(int/*_Uint32_*/ RegLvalue, int/*_Uint32_*/ RegRvalue, int/*_Uint32_*/ Label);
    public final native int branch_lt(int/*_Uint32_*/ RegLvalue, int/*_Uint32_*/ RegRvalue, int/*_Uint32_*/ Label);
    public final native int branch_eq(int/*_Uint32_*/ RegLvalue, int/*_Uint32_*/ RegRvalue, int/*_Uint32_*/ Label);
    public final native int branch_ne(int/*_Uint32_*/ RegLvalue, int/*_Uint32_*/ RegRvalue, int/*_Uint32_*/ Label);
    public final native int branch_ne_null(int/*_Uint32_*/ RegLvalue, int/*_Uint32_*/ Label);
    public final native int branch_eq_null(int/*_Uint32_*/ RegLvalue, int/*_Uint32_*/ Label);
    public final native int branch_col_eq(ByteBuffer/*_const void *_*/ val, int/*_Uint32_*/ len, int/*_Uint32_*/ attrId, int/*_Uint32_*/ Label);
    public final native int branch_col_ne(ByteBuffer/*_const void *_*/ val, int/*_Uint32_*/ len, int/*_Uint32_*/ attrId, int/*_Uint32_*/ Label);
    public final native int branch_col_lt(ByteBuffer/*_const void *_*/ val, int/*_Uint32_*/ len, int/*_Uint32_*/ attrId, int/*_Uint32_*/ Label);
    public final native int branch_col_le(ByteBuffer/*_const void *_*/ val, int/*_Uint32_*/ len, int/*_Uint32_*/ attrId, int/*_Uint32_*/ Label);
    public final native int branch_col_gt(ByteBuffer/*_const void *_*/ val, int/*_Uint32_*/ len, int/*_Uint32_*/ attrId, int/*_Uint32_*/ Label);
    public final native int branch_col_ge(ByteBuffer/*_const void *_*/ val, int/*_Uint32_*/ len, int/*_Uint32_*/ attrId, int/*_Uint32_*/ Label);
    public final native int branch_col_eq_null(int/*_Uint32_*/ attrId, int/*_Uint32_*/ Label);
    public final native int branch_col_ne_null(int/*_Uint32_*/ attrId, int/*_Uint32_*/ Label);
    public final native int branch_col_like(ByteBuffer/*_const void *_*/ val, int/*_Uint32_*/ len, int/*_Uint32_*/ attrId, int/*_Uint32_*/ Label);
    public final native int branch_col_notlike(ByteBuffer/*_const void *_*/ val, int/*_Uint32_*/ len, int/*_Uint32_*/ attrId, int/*_Uint32_*/ Label);
    public final native int interpret_exit_ok();
    public final native int interpret_exit_nok(int/*_Uint32_*/ ErrorCode);
    public final native int interpret_exit_nok();
    public final native int interpret_exit_last_row();
    public final native int add_val(int/*_Uint32_*/ attrId, int/*_Uint32_*/ aValue);
    public final native int add_val(int/*_Uint32_*/ attrId, long/*_Uint64_*/ aValue);
    public final native int sub_val(int/*_Uint32_*/ attrId, int/*_Uint32_*/ aValue);
    public final native int sub_val(int/*_Uint32_*/ attrId, long/*_Uint64_*/ aValue);
    public final native int def_sub(int/*_Uint32_*/ SubroutineNumber);
    public final native int call_sub(int/*_Uint32_*/ SubroutineNumber);
    public final native int ret_sub();
    public final native int finalise();
}
