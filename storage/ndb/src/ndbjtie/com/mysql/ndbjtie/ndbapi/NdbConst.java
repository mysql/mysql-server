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
 * NdbConst.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public interface NdbConst
{
    String/*_const char *_*/ getDatabaseName() /*_const_*/;
    String/*_const char *_*/ getDatabaseSchemaName() /*_const_*/;
    NdbDictionary.Dictionary/*_NdbDictionary.Dictionary *_*/ getDictionary() /*_const_*/;
    NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
    String/*_const char *_*/ getNdbErrorDetail(NdbErrorConst/*_const NdbError &_*/ err, ByteBuffer /*_char *_*/ buff, int/*_Uint32_*/ buffLen) /*_const_*/;
}
