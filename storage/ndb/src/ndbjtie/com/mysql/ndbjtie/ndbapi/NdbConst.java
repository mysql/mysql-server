/*
  Copyright (c) 2010, 2023, Oracle and/or its affiliates.
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
