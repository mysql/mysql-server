/*
  Copyright 2010 Sun Microsystems, Inc.
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
 * NdbBlob.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class NdbBlob extends Wrapper implements NdbBlobConst
{
    public final native NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
    public final native NdbOperationConst/*_const NdbOperation *_*/ getNdbOperation() /*_const_*/;
    public interface /*_enum_*/ State
    {
        int Idle = 0,
            Prepared = 1,
            Active = 2,
            Closed = 3,
            Invalid = 9;
    }
    public final native int/*_State_*/ getState();
    // MMM! support variable-width type non-const references or check if needed: public final native void getVersion(int[]/*_int &_*/ version);
    public final native int getValue(ByteBuffer/*_void *_*/ data, int/*_Uint32_*/ bytes);
    public final native int setValue(ByteBuffer/*_const void *_*/ data, int/*_Uint32_*/ bytes);
    // MMM no need to map: public final native typedef int ActiveHook(NdbBlob/*_NdbBlob *_*/ me, ByteBuffer/*_void *_*/ arg);
    // MMM no need to map: public final native int setActiveHook(ActiveHook/*_ActiveHook *_*/ activeHook, ByteBuffer/*_void *_*/ arg);
    // MMM! support variable-width type non-const references or check if needed: public final native int getNull(int[]/*_int &_*/ isNull);
    public final native int setNull();
    public final native int getLength(long[]/*_Uint64 &_*/ length);
    public final native int truncate(long/*_Uint64_*/ length /*_= 0_*/);
    public final native int getPos(long[]/*_Uint64 &_*/ pos);
    public final native int setPos(long/*_Uint64_*/ pos);
    public final native int readData(ByteBuffer/*_void *_*/ data, int[]/*_Uint32 &_*/ bytes);
    public final native int writeData(ByteBuffer/*_const void *_*/ data, int/*_Uint32_*/ bytes);
    public final native NdbDictionary.ColumnConst/*_const NdbDictionary.Column *_*/ getColumn();
    static public final native int getBlobTableName(ByteBuffer/*_char *_*/ btname, Ndb/*_Ndb *_*/ anNdb, String/*_const char *_*/ tableName, String/*_const char *_*/ columnName);
    static public final native int getBlobEventName(ByteBuffer/*_char *_*/ bename, Ndb/*_Ndb *_*/ anNdb, String/*_const char *_*/ eventName, String/*_const char *_*/ columnName);
    public final native NdbBlob/*_NdbBlob *_*/ blobsFirstBlob();
    public final native NdbBlob/*_NdbBlob *_*/ blobsNextBlob();
    public final native int close(boolean execPendingBlobOps /*= true*/);
}
