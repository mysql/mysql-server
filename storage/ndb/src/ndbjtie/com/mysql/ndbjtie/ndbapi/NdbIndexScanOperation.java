/*
  Copyright 2010 Sun Microsystems, Inc.
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
 * NdbIndexScanOperation.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class NdbIndexScanOperation extends NdbScanOperation implements NdbIndexScanOperationConst
{
    public final native boolean getSorted() /*_const_*/;
    public final native boolean getDescending() /*_const_*/;
    public /*_virtual_*/ native int readTuples(int/*_LockMode_*/ lock_mode /*_= LM_Read_*/, int/*_Uint32_*/ scan_flags /*_= 0_*/, int/*_Uint32_*/ parallel /*_= 0_*/, int/*_Uint32_*/ batch /*_= 0_*/);
    public interface /*_enum_*/ BoundType
    {
        int BoundLE = 0,
            BoundLT = 1,
            BoundGE = 2,
            BoundGT = 3,
            BoundEQ = 4;
    }
    public interface /*_enum_*/ NotSpecified
    {
        int MaxRangeNo = 0xfff;
    }
    public final native int setBound(String/*_const char *_*/ attr, int type, ByteBuffer/*_const void *_*/ value);
    public final native int setBound(int/*_Uint32_*/ anAttrId, int type, ByteBuffer/*_const void *_*/ aValue);
    public final native int end_of_bound(int/*_Uint32_*/ range_no /*_= 0_*/);
    public final native int get_range_no();
    public interface /*_struct_*/ IndexBoundConst
    {
        ByteBuffer/*_const char *_*/ low_key();
        int/*_Uint32_*/ low_key_count();
        boolean low_inclusive();
        ByteBuffer/*_const char *_*/ high_key();
        int/*_Uint32_*/ high_key_count();
        boolean high_inclusive();
        int/*_Uint32_*/ range_no();
    }
    static public class /*_struct_*/ IndexBound extends Wrapper implements IndexBoundConst
    {
        public final native ByteBuffer/*_const char *_*/ low_key();
        public final native int/*_Uint32_*/ low_key_count();
        public final native boolean low_inclusive();
        public final native ByteBuffer/*_const char *_*/ high_key();
        public final native int/*_Uint32_*/ high_key_count();
        public final native boolean high_inclusive();
        public final native int/*_Uint32_*/ range_no();
        public final native void low_key(ByteBuffer/*_const char *_*/ p0);
        public final native void low_key_count(int/*_Uint32_*/ p0);
        public final native void low_inclusive(boolean p0);
        public final native void high_key(ByteBuffer/*_const char *_*/ p0);
        public final native void high_key_count(int/*_Uint32_*/ p0);
        public final native void high_inclusive(boolean p0);
        public final native void range_no(int/*_Uint32_*/ p0);
        static public final native IndexBound create();
        static public final native void delete(IndexBound p0);
    }
    public final native int setBound(NdbRecordConst/*_const NdbRecord *_*/ key_record, IndexBoundConst/*_const IndexBound &_*/ bound);
}
