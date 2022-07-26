/*
  Copyright (c) 2010, 2022, Oracle and/or its affiliates.
  Use is subject to license terms.

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
/*
 * NdbScanFilter.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class NdbScanFilter extends Wrapper implements NdbScanFilterConst
{
    public final native NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
    public final native NdbInterpretedCodeConst/*_const NdbInterpretedCode *_*/ getInterpretedCode() /*_const_*/;
    public final native NdbOperation/*_NdbOperation *_*/ getNdbOperation() /*_const_*/;
    static public final native NdbScanFilter create(NdbInterpretedCode/*_NdbInterpretedCode *_*/ code);
    static public final native NdbScanFilter create(NdbOperation/*_NdbOperation *_*/ op);
    static public final native void delete(NdbScanFilter p0);
    public interface /*_enum_*/ Group
    {
        int AND = 1,
            OR = 2,
            NAND = 3,
            NOR = 4;
    }
    public interface /*_enum_*/ BinaryCondition
    {
        int COND_LE = 0,
            COND_LT = 1,
            COND_GE = 2,
            COND_GT = 3,
            COND_EQ = 4,
            COND_NE = 5,
            COND_LIKE = 6,
            COND_NOT_LIKE = 7;
    }
    public final native int begin(int/*_Group_*/ group /*_= AND_*/);
    public final native int end();
    public final native int istrue();
    public final native int isfalse();
    public final native int cmp(int/*_BinaryCondition_*/ cond, int ColId, ByteBuffer/*_const void *_*/ val, int/*_Uint32_*/ len /*_= 0_*/);
    public final native int eq(int ColId, int/*_Uint32_*/ value);
    public final native int ne(int ColId, int/*_Uint32_*/ value);
    public final native int lt(int ColId, int/*_Uint32_*/ value);
    public final native int le(int ColId, int/*_Uint32_*/ value);
    public final native int gt(int ColId, int/*_Uint32_*/ value);
    public final native int ge(int ColId, int/*_Uint32_*/ value);
    public final native int eq(int ColId, long/*_Uint64_*/ value);
    public final native int ne(int ColId, long/*_Uint64_*/ value);
    public final native int lt(int ColId, long/*_Uint64_*/ value);
    public final native int le(int ColId, long/*_Uint64_*/ value);
    public final native int gt(int ColId, long/*_Uint64_*/ value);
    public final native int ge(int ColId, long/*_Uint64_*/ value);
    public final native int isnull(int ColId);
    public final native int isnotnull(int ColId);
    public interface /*_enum_*/ Error
    {
        int FilterTooLarge = 4294;
    }
}
