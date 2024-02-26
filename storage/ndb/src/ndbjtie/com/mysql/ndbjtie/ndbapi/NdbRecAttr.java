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
 * NdbRecAttr.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class NdbRecAttr extends Wrapper implements NdbRecAttrConst
{
    public final native NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ getColumn() /*_const_*/;
    public final native int/*_NdbDictionary.Column.Type_*/ getType() /*_const_*/;
    public final native int/*_Uint32_*/ get_size_in_bytes() /*_const_*/;
    public final native int isNULL() /*_const_*/;
    public final native long/*_Int64_*/ int64_value() /*_const_*/;
    public final native int/*_Int32_*/ int32_value() /*_const_*/;
    public final native int/*_Int32_*/ medium_value() /*_const_*/;
    public final native short short_value() /*_const_*/;
    public final native byte char_value() /*_const_*/;
    public final native byte/*_Int8_*/ int8_value() /*_const_*/;
    public final native long/*_Uint64_*/ u_64_value() /*_const_*/;
    public final native int/*_Uint32_*/ u_32_value() /*_const_*/;
    public final native int/*_Uint32_*/ u_medium_value() /*_const_*/;
    public final native short/*_Uint16_*/ u_short_value() /*_const_*/;
    public final native byte/*_Uint8_*/ u_char_value() /*_const_*/;
    public final native byte/*_Uint8_*/ u_8_value() /*_const_*/;
    public final native float float_value() /*_const_*/;
    public final native double double_value() /*_const_*/;
    // MMM! support <out:BB> or check if needed: public final native char * aRef() /*_const_*/;
    public final native NdbRecAttr/*_NdbRecAttr *_*/ cloneNative/*_clone_*/() /*_const_*/; // MMM renamed due to nameclash with Java's Object version
    // MMM c'tor not part of the public API: static public final native NdbRecAttr create();
    static public final native void delete(NdbRecAttr p0);
}
