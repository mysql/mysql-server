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
    int/*_const Type_*/ getType() /*_const_*/;
    int/*_LockMode_*/ getLockMode() /*_const_*/;
    int/*_AbortOption_*/ getAbortOption() /*_const_*/;
    /*_virtual_*/ NdbTransaction/*_NdbTransaction *_*/ getNdbTransaction() /*_const_*/;
    NdbLockHandle/*_const NdbLockHandle *_*/ getLockHandle() /*_const_*/;
}
