/*
  Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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
