/*
  Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
 * NdbOperation.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;
import com.mysql.jtie.ArrayWrapper;

public class NdbOperation extends Wrapper implements NdbOperationConst
{
    public /*_virtual_*/ native NdbBlob/*_NdbBlob *_*/ getBlobHandle(String/*_const char *_*/ anAttrName) /*_const_*/; // MMM nameclash with non-const version
    public /*_virtual_*/ native NdbBlob/*_NdbBlob *_*/ getBlobHandle(int/*_Uint32_*/ anAttrId) /*_const_*/; // MMM nameclash with non-const version
    public final native NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
    public final native int getNdbErrorLine() /*_const_*/;
    public final native String/*_const char *_*/ getTableName() /*_const_*/;
    public final native NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ getTable() /*_const_*/;
    public final native int/*_Type_*/ getType() /*_const_*/;
    public final native int/*_LockMode_*/ getLockMode() /*_const_*/;
    public final native int/*_AbortOption_*/ getAbortOption() /*_const_*/;
    public /*_virtual_*/ native NdbTransaction/*_NdbTransaction *_*/ getNdbTransaction() /*_const_*/;
    public final native NdbLockHandle/*_const NdbLockHandle *_*/ getLockHandle() /*_const_*/;
    public final native NdbLockHandle/*_const NdbLockHandle *_*/ getLockHandleM();
    public /*_virtual_*/ native int insertTuple();
    public /*_virtual_*/ native int updateTuple();
    public /*_virtual_*/ native int writeTuple();
    public /*_virtual_*/ native int deleteTuple();
    public /*_virtual_*/ native int readTuple(int/*_LockMode_*/ p0);
    public final native int equal(String/*_const char *_*/ anAttrName, ByteBuffer/*_const char *_*/ aValue);
    public final native int equal(String/*_const char *_*/ anAttrName, int/*_Int32_*/ aValue); // MMM covers signed and unsigned integral types
    public final native int equal(String/*_const char *_*/ anAttrName, long/*_Int64_*/ aValue); // MMM covers signed and unsigned integral types
    public final native int equal(int/*_Uint32_*/ anAttrId, ByteBuffer/*_const char *_*/ aValue);
    public final native int equal(int/*_Uint32_*/ anAttrId, int/*_Int32_*/ aValue); // MMM covers signed and unsigned integral types
    public final native int equal(int/*_Uint32_*/ anAttrId, long/*_Int64_*/ aValue); // MMM covers signed and unsigned integral types
    public final native NdbRecAttr/*_NdbRecAttr *_*/ getValue(String/*_const char *_*/ anAttrName, ByteBuffer/*_char *_*/ aValue /*_= 0_*/);
    public final native NdbRecAttr/*_NdbRecAttr *_*/ getValue(int/*_Uint32_*/ anAttrId, ByteBuffer/*_char *_*/ aValue /*_= 0_*/);
    public final native NdbRecAttr/*_NdbRecAttr *_*/ getValue(NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ p0, ByteBuffer/*_char *_*/ val /*_= 0_*/);
    public final native int setValue(String/*_const char *_*/ anAttrName, ByteBuffer/*_const char *_*/ aValue);
    public final native int setValue(String/*_const char *_*/ anAttrName, int/*_Int32_*/ aValue); // MMM covers signed and unsigned integral types
    public final native int setValue(String/*_const char *_*/ anAttrName, long/*_Int64_*/ aValue); // MMM covers signed and unsigned integral types
    public final native int setValue(String/*_const char *_*/ anAttrName, float aValue);
    public final native int setValue(String/*_const char *_*/ anAttrName, double aValue);
    public final native int setValue(int/*_Uint32_*/ anAttrId, ByteBuffer/*_const char *_*/ aValue);
    public final native int setValue(int/*_Uint32_*/ anAttrId, int/*_Int32_*/ aValue); // MMM covers signed and unsigned integral types
    public final native int setValue(int/*_Uint32_*/ anAttrId, long/*_Int64_*/ aValue); // MMM covers signed and unsigned integral types
    public final native int setValue(int/*_Uint32_*/ anAttrId, float aValue);
    public final native int setValue(int/*_Uint32_*/ anAttrId, double aValue);
    public /*_virtual_*/ native NdbBlob/*_NdbBlob *_*/ getBlobHandleM(String/*_const char *_*/ anAttrName); // MMM renamed due to nameclash with const version
    public /*_virtual_*/ native NdbBlob/*_NdbBlob *_*/ getBlobHandleM(int/*_Uint32_*/ anAttrId); // MMM renamed due to nameclash with const version
    public final native int setAbortOption(int/*_AbortOption_*/ p0);
    public interface /*_struct_*/ GetValueSpecConst
    {
        NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ column();
        // MMM! support <out:BB> or check if needed: ByteBuffer/*_void *_*/ appStorage();
        NdbRecAttr/*_NdbRecAttr *_*/ recAttr();
    }
    static public interface GetValueSpecConstArray extends ArrayWrapper< GetValueSpecConst >
    {
    }
    static public class GetValueSpecArray extends Wrapper implements GetValueSpecConstArray
    {
        static public native GetValueSpecArray create(int length);
        static public native void delete(GetValueSpecArray e);
        public native GetValueSpec at(int i);
    }
    static public class /*_struct_*/ GetValueSpec extends Wrapper implements GetValueSpecConst
    {
        public final native NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ column();
        // MMM! support <out:BB> or check if needed: public final native ByteBuffer/*_void *_*/ appStorage();
        public final native NdbRecAttr/*_NdbRecAttr *_*/ recAttr();
        public final native void column(NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ p0);
        // MMM! support <out:BB> or check if needed: public final native void appStorage(ByteBuffer/*_void *_*/ p0);
        public final native void recAttr(NdbRecAttr/*_NdbRecAttr *_*/ p0);
        static public final native GetValueSpec create();
        static public final native void delete(GetValueSpec p0);
    }
    static public interface SetValueSpecConstArray extends ArrayWrapper< SetValueSpecConst >
    {
    }
    static public class SetValueSpecArray extends Wrapper implements SetValueSpecConstArray
    {
        static public native SetValueSpecArray create(int length);
        static public native void delete(SetValueSpecArray e);
        public native SetValueSpec at(int i);
    }
    public interface /*_struct_*/ SetValueSpecConst
    {
        NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ column();
        // MMM! support <out:BB> or check if needed: ByteBuffer/*_const void *_*/ value();
    }
    static public class /*_struct_*/ SetValueSpec extends Wrapper implements SetValueSpecConst
    {
        public final native NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ column();
        // MMM! support <out:BB> or check if needed: public final native ByteBuffer/*_const void *_*/ value();
        public final native void column(NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ p0);
        // MMM! support <out:BB> or check if needed: public final native void value(ByteBuffer/*_const void *_*/ p0);
        static public final native SetValueSpec create();
        static public final native void delete(SetValueSpec p0);
    }
    public interface /*_struct_*/ OperationOptionsConst
    {
        long/*_Uint64_*/ optionsPresent();
        public interface /*_enum_*/ Flags
        {
            int OO_ABORTOPTION = 0x01,
                OO_GETVALUE = 0x02,
                OO_SETVALUE = 0x04,
                OO_PARTITION_ID = 0x08,
                OO_INTERPRETED = 0x10,
                OO_ANYVALUE = 0x20,
                OO_CUSTOMDATA = 0x40;
        }
        int/*_AbortOption_*/ abortOption();
        GetValueSpecArray/*_GetValueSpec *_*/ extraGetValues();
        int/*_Uint32_*/ numExtraGetValues();
        SetValueSpecConstArray/*_const SetValueSpec *_*/ extraSetValues();
        int/*_Uint32_*/ numExtraSetValues();
        int/*_Uint32_*/ partitionId();
        NdbInterpretedCodeConst/*_const NdbInterpretedCode *_*/ interpretedCode();
        int/*_Uint32_*/ anyValue();
        // MMM ByteBuffer/*_void *_*/ customData();
    }
    static public class /*_struct_*/ OperationOptions extends Wrapper implements OperationOptionsConst
    {
        static public final native int/*_Uint32_*/ size();
        public final native long/*_Uint64_*/ optionsPresent();
        public final native int/*_AbortOption_*/ abortOption();
        public final native GetValueSpecArray/*_GetValueSpec *_*/ extraGetValues();
        public final native int/*_Uint32_*/ numExtraGetValues();
        public final native SetValueSpecConstArray/*_const SetValueSpec *_*/ extraSetValues();
        public final native int/*_Uint32_*/ numExtraSetValues();
        public final native int/*_Uint32_*/ partitionId();
        public final native NdbInterpretedCodeConst/*_const NdbInterpretedCode *_*/ interpretedCode();
        public final native int/*_Uint32_*/ anyValue();
        // MMM! support <out:BB> or check if needed: public final native ByteBuffer/*_void *_*/ customData();
        public final native void optionsPresent(long/*_Uint64_*/ p0);
        public final native void abortOption(int/*_AbortOption_*/ p0);
        public final native void extraGetValues(GetValueSpecArray/*_GetValueSpec *_*/ p0);
        public final native void numExtraGetValues(int/*_Uint32_*/ p0);
        public final native void extraSetValues(SetValueSpecConstArray/*_const SetValueSpec *_*/ p0);
        public final native void numExtraSetValues(int/*_Uint32_*/ p0);
        public final native void partitionId(int/*_Uint32_*/ p0);
        public final native void interpretedCode(NdbInterpretedCodeConst/*_const NdbInterpretedCode *_*/ p0);
        public final native void anyValue(int/*_Uint32_*/ p0);
        // MMM! support <out:BB> or check if needed: public final native public final native void customData(ByteBuffer/*_void *_*/ p0);
        static public final native OperationOptions create();
        static public final native void delete(OperationOptions p0);
    }
}
