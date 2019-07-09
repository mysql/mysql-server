/*
  Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.

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
 * Ndb.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;
import com.mysql.jtie.ArrayWrapper;

public class Ndb extends Wrapper implements NdbConst
{
    public final native int getAutoIncrementValue(NdbDictionary.TableConst aTable, long[] ret, int cacheSize, long step, long start);
    public final native String/*_const char *_*/ getDatabaseName() /*_const_*/;
    public final native String/*_const char *_*/ getDatabaseSchemaName() /*_const_*/;
    public final native NdbDictionary.Dictionary/*_NdbDictionary.Dictionary *_*/ getDictionary() /*_const_*/;
    public final native NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
    public final native String/*_const char *_*/ getNdbErrorDetail(NdbErrorConst/*_const NdbError &_*/ err, ByteBuffer /*_char *_*/ buff, int/*_Uint32_*/ buffLen) /*_const_*/;
    static public final native Ndb create(Ndb_cluster_connection/*_Ndb_cluster_connection *_*/ ndb_cluster_connection, String/*_const char *_*/ aCatalogName /*_= ""_*/, String/*_const char *_*/ aSchemaName /*_= "def"_*/);
    static public final native void delete(Ndb p0);
    public final native int setDatabaseName(String/*_const char *_*/ aDatabaseName);
    public final native int setDatabaseSchemaName(String/*_const char *_*/ aDatabaseSchemaName);
    public final native int init(int maxNoOfTransactions /*_= 4_*/);
    public final native NdbEventOperation/*_NdbEventOperation *_*/ createEventOperation(String/*_const char *_*/ eventName);
    public final native int dropEventOperation(NdbEventOperation/*_NdbEventOperation *_*/ eventOp);
    public final native int pollEvents(int aMillisecondNumber, long[]/*_Uint64 *_*/ latestGCI /*_= 0_*/);
    public final native NdbEventOperation/*_NdbEventOperation *_*/ nextEvent();
    public final native boolean isConsistent(long[]/*_Uint64 &_*/ gci);
    public final native boolean isConsistentGCI(long/*_Uint64_*/ gci);
    public final native NdbEventOperationConst/*_const NdbEventOperation *_*/ getGCIEventOperations(int[]/*_Uint32 *_*/ iter, int[]/*_Uint32 *_*/ event_types);
    public final native NdbTransaction/*_NdbTransaction *_*/ startTransaction(NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ table /*_= 0_*/, ByteBuffer/*_const char *_*/ keyData /*_= 0_*/, int/*_Uint32_*/ keyLen /*_= 0_*/);
    static public interface Key_part_ptrConstArray extends ArrayWrapper< Key_part_ptrConst >
    {
    }
    static public class Key_part_ptrArray extends Wrapper implements Key_part_ptrConstArray
    {
        static public native Key_part_ptrArray create(int length);
        static public native void delete(Key_part_ptrArray e);
        public native Key_part_ptr at(int i);
    }
    public interface /*_struct_*/ Key_part_ptrConst
    {
        // MMM! support <out:BB> or check if needed: ByteBuffer/*_const void *_*/ ptr();
        int/*_unsigned_*/ len();
    }
    static public class /*_struct_*/ Key_part_ptr extends Wrapper implements Key_part_ptrConst
    {
        // MMM! support <out:BB> or check if needed: public final native ByteBuffer/*_const void *_*/ ptr();
        public final native int/*_unsigned_*/ len();
        public final native void ptr(ByteBuffer/*_const void *_*/ p0);
        public final native void len(int/*_unsigned_*/ p0);
        static public final native Key_part_ptr create();
        static public final native void delete(Key_part_ptr p0);
    }
    public interface /*_struct_*/ PartitionSpecConst
    {
        public interface /*_enum_*/ SpecType
        {
            int PS_NONE = 0,
                PS_USER_DEFINED = 1,
                PS_DISTR_KEY_PART_PTR = 2,
                PS_DISTR_KEY_RECORD = 3;
        }
        int/*_SpecType_*/ type() /*_const_*/;
    }
    static public class /*_struct_*/ PartitionSpec extends Wrapper implements PartitionSpecConst
    {
        static public final native int/*_Uint32_*/ size();
        public final native int/*_SpecType_*/ type() /*_const_*/;
        // MMM! support mapping <union> or check if needed
        //union {
        //    struct {
        //        Uint32 partitionId;
        //    } UserDefined;
        //    struct {
        //        const Key_part_ptr* tableKeyParts;
        //        void* xfrmbuf;
        //        Uint32 xfrmbuflen;
        //    } KeyPartPtr;
        //    struct {
        //        const NdbRecord* keyRecord;
        //        const char* keyRow;
        //        void* xfrmbuf;
        //        Uint32 xfrmbuflen;
        //    } KeyRecord;
        //};
    }
    public final native NdbTransaction/*_NdbTransaction *_*/ startTransaction(NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ table, Key_part_ptrConstArray/*_const Key_part_ptr *_*/ keyData, ByteBuffer/*_void *_*/ xfrmbuf /*_= 0_*/, int/*_Uint32_*/ xfrmbuflen /*_= 0_*/);
    public final native NdbTransaction/*_NdbTransaction *_*/ startTransaction(NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ table, int/*_Uint32_*/ partitionId);
    static public final native int computeHash(int[]/*_Uint32 *_*/ hashvalueptr, NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ p0, Key_part_ptrConstArray/*_const Key_part_ptr *_*/ keyData, ByteBuffer/*_void *_*/ xfrmbuf /*_= 0_*/, int/*_Uint32_*/ xfrmbuflen /*_= 0_*/);
    public final native void closeTransaction(NdbTransaction/*_NdbTransaction *_*/ p0);
    public final native NdbErrorConst/*_const NdbError &_*/ getNdbError(int errorCode);
}
