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
 * NdbError.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class /*_struct_*/ NdbError extends Wrapper implements NdbErrorConst
{
    public final native int/*_Status_*/ status();
    public final native int/*_Classification_*/ classification();
    public final native int code();
    public final native int mysql_code();
    public final native String/*_const char *_*/ message();
    public final native void status(int/*_Status_*/ p0);
    public final native void classification(int/*_Classification_*/ p0);
    public final native void code(int p0);
    public final native void mysql_code(int p0);
    public final native void message(String/*_const char *_*/ p0);
    // MMM c'tor not part of the public API: static public final native NdbError create();
    // MMM c'tor not part of the public API: static public final native void delete(NdbError p0);
}
