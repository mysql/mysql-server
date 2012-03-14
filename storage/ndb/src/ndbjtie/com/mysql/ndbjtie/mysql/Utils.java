/*
  Copyright 2010 Sun Microsystems, Inc.
  All rights reserved. Use is subject to license terms.

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
 * Utils.java
 */

package com.mysql.ndbjtie.mysql;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class Utils
{
    static public final int E_DEC_OK = 0;
    static public final int E_DEC_TRUNCATED = 1;
    static public final int E_DEC_OVERFLOW = 2;
    static public final int E_DEC_BAD_NUM = 8;
    static public final int E_DEC_OOM = 16;
    //static public final int E_DEC_ERROR = 31; // MMM not used at this time?
    //static public final int E_DEC_FATAL_ERROR = 30; // MMM not used at this time?
    static public final native int decimal_str2bin(ByteBuffer/*_const char *_*/ str, int str_len, int prec, int scale, ByteBuffer/*_void *_*/ dest, int buf_len);
    static public final native int decimal_bin2str(ByteBuffer/*_const void *_*/ bin, int bin_len, int prec, int scale, ByteBuffer/*_char *_*/ dest, int buf_len);
}
