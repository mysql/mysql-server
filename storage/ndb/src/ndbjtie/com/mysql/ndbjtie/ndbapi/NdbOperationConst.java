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
 * NdbOperationConst.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public interface NdbOperationConst
{
    public interface /*_enum_*/ Type
    {
        int PrimaryKeyAccess = 0,
            UniqueIndexAccess = 1,
            TableScan = 2,
            OrderedIndexScan = 3;
    }
    public interface /*_enum_*/ LockMode
    {
        int LM_Read = 0,
            LM_Exclusive = 1,
            LM_CommittedRead = 2,
            LM_Dirty = 2,
            LM_SimpleRead = 3;
    }
    public interface /*_enum_*/ AbortOption
    {
        int DefaultAbortOption = -1,
            AbortOnError = 0,
            AO_IgnoreError = 2;
    }
    /*_virtual_*/ NdbBlob/*_NdbBlob *_*/ getBlobHandle(String/*_const char *_*/ anAttrName) /*_const_*/; // MMM nameclash with non-const version
    /*_virtual_*/ NdbBlob/*_NdbBlob *_*/ getBlobHandle(int/*_Uint32_*/ anAttrId) /*_const_*/; // MMM nameclash with non-const version
    NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
    int getNdbErrorLine() /*_const_*/;
    String/*_const char *_*/ getTableName() /*_const_*/;
    NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ getTable() /*_const_*/;
    int/*_Type_*/ getType() /*_const_*/;
    int/*_LockMode_*/ getLockMode() /*_const_*/;
    int/*_AbortOption_*/ getAbortOption() /*_const_*/;
    /*_virtual_*/ NdbTransaction/*_NdbTransaction *_*/ getNdbTransaction() /*_const_*/;
    NdbLockHandle/*_const NdbLockHandle *_*/ getLockHandle() /*_const_*/;
}
