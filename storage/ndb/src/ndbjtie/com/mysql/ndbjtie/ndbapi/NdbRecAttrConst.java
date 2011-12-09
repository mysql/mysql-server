/*
  Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * NdbRecAttrConst.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public interface NdbRecAttrConst
{
    NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ getColumn() /*_const_*/;
    int/*_NdbDictionary.Column.Type_*/ getType() /*_const_*/;
    int/*_Uint32_*/ get_size_in_bytes() /*_const_*/;
    int isNULL() /*_const_*/;
    long/*_Int64_*/ int64_value() /*_const_*/;
    int/*_Int32_*/ int32_value() /*_const_*/;
    int/*_Int32_*/ medium_value() /*_const_*/;
    short short_value() /*_const_*/;
    byte char_value() /*_const_*/;
    byte/*_Int8_*/ int8_value() /*_const_*/;
    long/*_Uint64_*/ u_64_value() /*_const_*/;
    int/*_Uint32_*/ u_32_value() /*_const_*/;
    int/*_Uint32_*/ u_medium_value() /*_const_*/;
    short/*_Uint16_*/ u_short_value() /*_const_*/;
    byte/*_Uint8_*/ u_char_value() /*_const_*/;
    byte/*_Uint8_*/ u_8_value() /*_const_*/;
    float float_value() /*_const_*/;
    double double_value() /*_const_*/;
    // MMM! support <out:BB> or check if needed: char * aRef() /*_const_*/;
    NdbRecAttr/*_NdbRecAttr *_*/ cloneNative/*_clone_*/() /*_const_*/; // MMM renamed due to nameclash with Java's Object version
}
