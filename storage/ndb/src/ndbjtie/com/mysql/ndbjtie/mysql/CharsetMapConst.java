/*
  Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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
 * CharsetMapConst.java
 */

package com.mysql.ndbjtie.mysql;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public interface CharsetMapConst
{
    public interface /*_enum_*/ RecodeStatus
    {
        int RECODE_OK = 0,
            RECODE_BAD_CHARSET = 1,
            RECODE_BAD_SRC = 2,
            RECODE_BUFF_TOO_SMALL = 3;
    }

    String/*_const char *_*/ getName(int cs_number) /*_const_*/;
    String/*_const char *_*/ getMysqlName(int cs_number) /*_const_*/; 
    int getCharsetNumber(String/*_const char *_*/ mysql_name) /*_const_*/;
    int getUTF8CharsetNumber() /*_const_*/;
    int getUTF16CharsetNumber() /*_const_*/;
    boolean[] isMultibyte(int cs_number) /*_const_*/;
    int/*_RecodeStatus_*/ recode(int[]/*_int32_t *_*/ lengths, int cs_from, int cs_to, ByteBuffer/*_const void *_*/ src, ByteBuffer/*_void *_*/ dest) /*_const_*/;
}
