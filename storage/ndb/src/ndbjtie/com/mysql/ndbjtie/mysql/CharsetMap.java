/*
  Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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
 * CharsetMap.java
 */

package com.mysql.ndbjtie.mysql;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class CharsetMap extends Wrapper implements CharsetMapConst
{
    public final native String/*_const char *_*/ getName(int cs_number) /*_const_*/;
    public final native String/*_const char *_*/ getMysqlName(int cs_number) /*_const_*/; 
    public final native int getCharsetNumber(String/*_const char *_*/ mysql_name) /*_const_*/;
    public final native int getUTF8CharsetNumber() /*_const_*/;
    public final native int getUTF16CharsetNumber() /*_const_*/;
    public final native boolean[] isMultibyte(int cs_number) /*_const_*/;
    public final native int recode(int[]/*_Int32 *_*/ lengths, int cs_from, int cs_to, ByteBuffer/*_const void *_*/ src, ByteBuffer/*_void *_*/ dest) /*_const_*/;
    static public final native CharsetMap create();
    static public final native void delete(CharsetMap p0);
}
