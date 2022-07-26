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
 * NdbDictionary.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;
import com.mysql.jtie.ArrayWrapper;

public class NdbDictionary extends Wrapper
{
    static public native NdbDictionary create(); // MMM non-final, support for derivation
    static public /*_virtual_*/ native void delete(NdbDictionary p0); // MMM non-final, support for derivation
    public interface ObjectConst
    {
        public interface /*_enum_*/ Status
        {
            int New = 0 /*__*/,
                Changed = 1 /*__*/,
                Retrieved = 2 /*__*/,
                Invalid = 3 /*__*/,
                Altered = 4 /*__*/;
        }
        /*_virtual_*/ int/*_Status_*/ getObjectStatus() /*_const_*/ /*_= 0_*/;
        /*_virtual_*/ int getObjectVersion() /*_const_*/ /*_= 0_*/;
        /*_virtual_*/ int getObjectId() /*_const_*/ /*_= 0_*/;
    }
    static public abstract class Object extends Wrapper implements ObjectConst
    {
        public /*_virtual_*/ abstract int/*_Status_*/ getObjectStatus() /*_const_*/ /*_= 0_*/;
        public /*_virtual_*/ abstract int getObjectVersion() /*_const_*/ /*_= 0_*/;
        public /*_virtual_*/ abstract int getObjectId() /*_const_*/ /*_= 0_*/;
        // MMM abstract class: static public native Object create(); // MMM non-final, support for derivation
        static public /*_virtual_*/ native void delete(Object p0); // MMM non-final, support for derivation
        public interface /*_enum_*/ Type
        {
            int TypeUndefined = 0,
                SystemTable = 1,
                UserTable = 2,
                UniqueHashIndex = 3,
                OrderedIndex = 6,
                HashIndexTrigger = 7,
                IndexTrigger = 8,
                SubscriptionTrigger = 9,
                ReadOnlyConstraint = 10,
                TableEvent = 11,
                Tablespace = 20,
                LogfileGroup = 21,
                Datafile = 22,
                Undofile = 23;
        }
        public interface /*_enum_*/ State
        {
            int StateUndefined = 0,
                StateOffline = 1,
                StateBuilding = 2,
                StateDropping = 3,
                StateOnline = 4,
                ObsoleteStateBackup = 5,
                StateBroken = 9;
        }
        public interface /*_enum_*/ Store
        {
            int StoreUndefined = 0,
                StoreNotLogged = 1,
                StorePermanent = 2;
        }
        public interface /*_enum_*/ FragmentType
        {
            int FragUndefined = 0,
                FragSingle = 1,
                FragAllSmall = 2,
                FragAllMedium = 3,
                FragAllLarge = 4,
                DistrKeyHash = 5,
                DistrKeyLin = 6,
                UserDefined = 7;
        }
    }
    public interface ObjectIdConst extends ObjectConst
    {
        /*_virtual_*/ int/*_Status_*/ getObjectStatus() /*_const_*/;
        /*_virtual_*/ int getObjectVersion() /*_const_*/;
        /*_virtual_*/ int getObjectId() /*_const_*/;
    }
    static public class ObjectId extends Wrapper implements ObjectIdConst
    {
        public /*_virtual_*/ native int/*_Status_*/ getObjectStatus() /*_const_*/;
        public /*_virtual_*/ native int getObjectVersion() /*_const_*/;
        public /*_virtual_*/ native int getObjectId() /*_const_*/;
        static public native ObjectId create(); // MMM non-final, support for derivation
        static public /*_virtual_*/ native void delete(ObjectId p0); // MMM non-final, support for derivation
    }
    public interface ColumnConst // MMM does not extend ObjectConst
    {
        public interface /*_enum_*/ Type
        {
            int Undefined = 0 /*_NDB_TYPE_UNDEFINED_*/,
                Tinyint = 1 /*_NDB_TYPE_TINYINT_*/,
                Tinyunsigned = 2 /*_NDB_TYPE_TINYUNSIGNED_*/,
                Smallint = 3 /*_NDB_TYPE_SMALLINT_*/,
                Smallunsigned = 4 /*_NDB_TYPE_SMALLUNSIGNED_*/,
                Mediumint = 5 /*_NDB_TYPE_MEDIUMINT_*/,
                Mediumunsigned = 6 /*_NDB_TYPE_MEDIUMUNSIGNED_*/,
                Int = 7 /*_NDB_TYPE_INT_*/,
                Unsigned = 8 /*_NDB_TYPE_UNSIGNED_*/,
                Bigint = 9 /*_NDB_TYPE_BIGINT_*/,
                Bigunsigned = 10 /*_NDB_TYPE_BIGUNSIGNED_*/,
                Float = 11 /*_NDB_TYPE_FLOAT_*/,
                Double = 12 /*_NDB_TYPE_DOUBLE_*/,
                Olddecimal = 13 /*_NDB_TYPE_OLDDECIMAL_*/,
                Olddecimalunsigned = 28 /*_NDB_TYPE_OLDDECIMALUNSIGNED_*/,
                Decimal = 29 /*_NDB_TYPE_DECIMAL_*/,
                Decimalunsigned = 30 /*_NDB_TYPE_DECIMALUNSIGNED_*/,
                Char = 14 /*_NDB_TYPE_CHAR_*/,
                Varchar = 15 /*_NDB_TYPE_VARCHAR_*/,
                Binary = 16 /*_NDB_TYPE_BINARY_*/,
                Varbinary = 17 /*_NDB_TYPE_VARBINARY_*/,
                Datetime = 18 /*_NDB_TYPE_DATETIME_*/,
                Date = 19 /*_NDB_TYPE_DATE_*/,
                Blob = 20 /*_NDB_TYPE_BLOB_*/,
                Text = 21 /*_NDB_TYPE_TEXT_*/,
                Bit = 22 /*_NDB_TYPE_BIT_*/,
                Longvarchar = 23 /*_NDB_TYPE_LONGVARCHAR_*/,
                Longvarbinary = 24 /*_NDB_TYPE_LONGVARBINARY_*/,
                Time = 25 /*_NDB_TYPE_TIME_*/,
                Year = 26 /*_NDB_TYPE_YEAR_*/,
                Timestamp = 27 /*_NDB_TYPE_TIMESTAMP_*/;
        }
        public interface /*_enum_*/ ArrayType
        {
            int ArrayTypeFixed = 0 /*_NDB_ARRAYTYPE_FIXED_*/,
                ArrayTypeShortVar = 1 /*_NDB_ARRAYTYPE_SHORT_VAR_*/,
                ArrayTypeMediumVar = 2 /*_NDB_ARRAYTYPE_MEDIUM_VAR_*/;
        }
        public interface /*_enum_*/ StorageType
        {
            int StorageTypeMemory = 0 /*_NDB_STORAGETYPE_MEMORY_*/,
                StorageTypeDisk = 1 /*_NDB_STORAGETYPE_DISK_*/;
        }
        boolean getAutoIncrement() /*_const_*/;
        String/*_const char *_*/ getName() /*_const_*/;
        boolean getNullable() /*_const_*/;
        boolean getPrimaryKey() /*_const_*/;
        int getColumnNo() /*_const_*/;
        boolean equal(ColumnConst/*_const Column &_*/ column) /*_const_*/;
        int/*_Type_*/ getType() /*_const_*/;
        int getPrecision() /*_const_*/;
        int getScale() /*_const_*/;
        int getLength() /*_const_*/;
        // MMM unsupported, opaque MySQL server type, mapped by mysql utilities: CHARSET_INFO * getCharset() /*_const_*/;
        int getCharsetNumber() /*_const_*/;
        int getInlineSize() /*_const_*/;
        int getPartSize() /*_const_*/;
        int getStripeSize() /*_const_*/;
        int getSize() /*_const_*/;
        boolean getPartitionKey() /*_const_*/;
        int/*_ArrayType_*/ getArrayType() /*_const_*/;
        int/*_StorageType_*/ getStorageType() /*_const_*/;
        boolean getDynamic() /*_const_*/;
        boolean getIndexSourced() /*_const_*/;
    }
    static public class Column extends Wrapper implements ColumnConst
    {
        public final native boolean getAutoIncrement() /*_const_*/;
        public final native String/*_const char *_*/ getName() /*_const_*/;
        public final native boolean getNullable() /*_const_*/;
        public final native boolean getPrimaryKey() /*_const_*/;
        public final native int getColumnNo() /*_const_*/;
        public final native boolean equal(ColumnConst/*_const Column &_*/ column) /*_const_*/;
        public final native int/*_Type_*/ getType() /*_const_*/;
        public final native int getPrecision() /*_const_*/;
        public final native int getScale() /*_const_*/;
        public final native int getLength() /*_const_*/;
        // MMM unsupported, opaque MySQL server type, mapped by mysql utilities: public final native CHARSET_INFO * getCharset() /*_const_*/;
        public final native int getCharsetNumber() /*_const_*/;
        public final native int getInlineSize() /*_const_*/;
        public final native int getPartSize() /*_const_*/;
        public final native int getStripeSize() /*_const_*/;
        public final native int getSize() /*_const_*/;
        public final native boolean getPartitionKey() /*_const_*/;
        public final native int/*_ArrayType_*/ getArrayType() /*_const_*/;
        public final native int/*_StorageType_*/ getStorageType() /*_const_*/;
        public final native boolean getDynamic() /*_const_*/;
        public final native boolean getIndexSourced() /*_const_*/;
        static public final native Column create(String/*_const char *_*/ name /*_= ""_*/);
        static public final native Column create(ColumnConst/*_const Column &_*/ column);
        static public final native void delete(Column p0);
        public final native int setName(String/*_const char *_*/ name);
        public final native void setNullable(boolean p0);
        public final native void setPrimaryKey(boolean p0);
        public final native void setType(int/*_Type_*/ type);
        public final native void setPrecision(int p0);
        public final native void setScale(int p0);
        public final native void setLength(int length);
        // MMM unsupported, opaque MySQL server type, mapped by mysql utilities: public final native void setCharset(CHARSET_INFO * cs);
        public final native void setInlineSize(int size);
        public final native void setPartSize(int size);
        public final native void setStripeSize(int size);
        public final native void setPartitionKey(boolean enable);
        public final native void setArrayType(int/*_ArrayType_*/ type);
        public final native void setStorageType(int/*_StorageType_*/ type);
        public final native void setDynamic(boolean p0);
    }
    public interface TableConst extends ObjectConst
    {
        public interface /*_enum_*/ SingleUserMode
        {
            int SingleUserModeLocked = 0 /*_NDB_SUM_LOCKED_*/,
                SingleUserModeReadOnly = 1 /*_NDB_SUM_READONLY_*/,
                SingleUserModeReadWrite = 2 /*_NDB_SUM_READ_WRITE_*/;
        }
        String/*_const char *_*/ getName() /*_const_*/;
        int getTableId() /*_const_*/;
        ColumnConst/*_const Column *_*/ getColumn(String/*_const char *_*/ name) /*_const_*/; // MMM nameclash with non-const version
        ColumnConst/*_const Column *_*/ getColumn(int attributeId) /*_const_*/; // MMM nameclash with non-const version
        boolean getLogging() /*_const_*/;
        int/*_FragmentType_*/ getFragmentType() /*_const_*/;
        int getKValue() /*_const_*/;
        int getMinLoadFactor() /*_const_*/;
        int getMaxLoadFactor() /*_const_*/;
        int getNoOfColumns() /*_const_*/;
        int getNoOfPrimaryKeys() /*_const_*/;
        String/*_const char *_*/ getPrimaryKey(int no) /*_const_*/;
        boolean equal(TableConst/*_const Table &_*/ p0) /*_const_*/;
        ByteBuffer/*_const void *_*/ getFrmData() /*_const_*/;
        int/*_Uint32_*/ getFrmLength() /*_const_*/;
        ByteBuffer/*_const void *_*/ getFragmentData() /*_const_*/;
        int/*_Uint32_*/ getFragmentDataLen() /*_const_*/;
        ByteBuffer/*_const void *_*/ getRangeListData() /*_const_*/;
        int/*_Uint32_*/ getRangeListDataLen() /*_const_*/;
        NdbRecordConst/*_const NdbRecord *_*/ getDefaultRecord() /*_const_*/;
        boolean getLinearFlag() /*_const_*/;
        int/*_Uint32_*/ getFragmentCount() /*_const_*/;
        String/*_const char *_*/ getTablespaceName() /*_const_*/;
        boolean getTablespace(int[]/*_Uint32 *_*/ id /*_= 0_*/, int[]/*_Uint32 *_*/ version /*_= 0_*/) /*_const_*/;
        // MMM declared but not implemented in NDBAPI: int/*_Object.Type_*/ getObjectType() /*_const_*/;
        /*_virtual_*/ int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        void setStatusInvalid() /*_const_*/;
        /*_virtual_*/ int getObjectVersion() /*_const_*/;
        int/*_Uint32_*/ getDefaultNoPartitionsFlag() /*_const_*/;
        /*_virtual_*/ int getObjectId() /*_const_*/;
        long/*_Uint64_*/ getMaxRows() /*_const_*/;
        long/*_Uint64_*/ getMinRows() /*_const_*/;
        int/*_SingleUserMode_*/ getSingleUserMode() /*_const_*/;
        boolean getRowGCIIndicator() /*_const_*/;
        boolean getRowChecksumIndicator() /*_const_*/;
        int/*_Uint32_*/ getPartitionId(int/*_Uint32_*/ hashvalue) /*_const_*/;
    }
    static public class Table extends Object implements TableConst
    {
        public final native String/*_const char *_*/ getName() /*_const_*/;
        public final native int getTableId() /*_const_*/;
        public final native ColumnConst/*_const Column *_*/ getColumn(String/*_const char *_*/ name) /*_const_*/; // MMM nameclash with non-const version
        public final native ColumnConst/*_const Column *_*/ getColumn(int attributeId) /*_const_*/; // MMM nameclash with non-const version
        public final native boolean getLogging() /*_const_*/;
        public final native int/*_FragmentType_*/ getFragmentType() /*_const_*/;
        public final native int getKValue() /*_const_*/;
        public final native int getMinLoadFactor() /*_const_*/;
        public final native int getMaxLoadFactor() /*_const_*/;
        public final native int getNoOfColumns() /*_const_*/;
        public final native int getNoOfPrimaryKeys() /*_const_*/;
        public final native String/*_const char *_*/ getPrimaryKey(int no) /*_const_*/;
        public final native boolean equal(TableConst/*_const Table &_*/ p0) /*_const_*/;
        public final native ByteBuffer/*_const void *_*/ getFrmData() /*_const_*/;
        public final native int/*_Uint32_*/ getFrmLength() /*_const_*/;
        public final native ByteBuffer/*_const void *_*/ getFragmentData() /*_const_*/;
        public final native int/*_Uint32_*/ getFragmentDataLen() /*_const_*/;
        public final native ByteBuffer/*_const void *_*/ getRangeListData() /*_const_*/;
        public final native int/*_Uint32_*/ getRangeListDataLen() /*_const_*/;
        public final native NdbRecordConst/*_const NdbRecord *_*/ getDefaultRecord() /*_const_*/;
        public final native boolean getLinearFlag() /*_const_*/;
        public final native int/*_Uint32_*/ getFragmentCount() /*_const_*/;
        public final native String/*_const char *_*/ getTablespaceName() /*_const_*/;
        public final native boolean getTablespace(int[]/*_Uint32 *_*/ id /*_= 0_*/, int[]/*_Uint32 *_*/ version /*_= 0_*/) /*_const_*/;
        // MMM declared but not implemented in NDBAPI: public final native int/*_Object.Type_*/ getObjectType() /*_const_*/;
        public /*_virtual_*/ native int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        public final native void setStatusInvalid() /*_const_*/;
        public /*_virtual_*/ native int getObjectVersion() /*_const_*/;
        public final native int/*_Uint32_*/ getDefaultNoPartitionsFlag() /*_const_*/;
        public /*_virtual_*/ native int getObjectId() /*_const_*/;
        public final native long/*_Uint64_*/ getMaxRows() /*_const_*/;
        public final native long/*_Uint64_*/ getMinRows() /*_const_*/;
        public final native int/*_SingleUserMode_*/ getSingleUserMode() /*_const_*/;
        public final native boolean getRowGCIIndicator() /*_const_*/;
        public final native boolean getRowChecksumIndicator() /*_const_*/;
        public final native int/*_Uint32_*/ getPartitionId(int/*_Uint32_*/ hashvalue) /*_const_*/;
        static public native Table create(String/*_const char *_*/ name /*_= ""_*/); // MMM non-final, support for derivation
        static public native Table create(TableConst/*_const Table &_*/ table); // MMM non-final, support for derivation
        static public /*_virtual_*/ native void delete(Table p0); // MMM non-final, support for derivation
        // public final native Table & operator= (const Table & table); // MMM no need to map assignment operator to Java
        public final native Column/*_Column *_*/ getColumnM(int attributeId); // MMM renamed due to nameclash with const version
        public final native Column/*_Column *_*/ getColumnM(String/*_const char *_*/ name); // MMM renamed due to nameclash with const version
        public final native int setName(String/*_const char *_*/ name);
        public final native int addColumn(ColumnConst/*_const Column &_*/ p0);
        public final native void setLogging(boolean p0);
        public final native void setLinearFlag(int/*_Uint32_*/ flag);
        public final native void setFragmentCount(int/*_Uint32_*/ p0);
        public final native void setFragmentType(int/*_FragmentType_*/ p0);
        public final native void setKValue(int kValue);
        public final native void setMinLoadFactor(int p0);
        public final native void setMaxLoadFactor(int p0);
        public final native int setTablespaceName(String/*_const char *_*/ name);
        public final native int setTablespace(TablespaceConst/*_const Tablespace &_*/ p0);
        public final native void setDefaultNoPartitionsFlag(int/*_Uint32_*/ indicator);
        public final native int setFrm(ByteBuffer/*_const void *_*/ data, int/*_Uint32_*/ len);
        public final native int setFragmentData(ByteBuffer/*_const void *_*/ data, int/*_Uint32_*/ len);
        public final native int setRangeListData(ByteBuffer/*_const void *_*/ data, int/*_Uint32_*/ len);
        // MMM declared but not implemented in NDBAPI: public final native void setObjectType(int/*_Object.Type_*/ type);
        public final native void setMaxRows(long/*_Uint64_*/ maxRows);
        public final native void setMinRows(long/*_Uint64_*/ minRows);
        public final native void setSingleUserMode(int/*_SingleUserMode_*/ p0);
        public final native void setRowGCIIndicator(boolean value);
        public final native void setRowChecksumIndicator(boolean value);
        public final native int aggregate(NdbError/*_NdbError &_*/ error);
        public final native int validate(NdbError/*_NdbError &_*/ error);
    }
    public interface IndexConst extends ObjectConst
    {
        public interface /*_enum_*/ Type
        {
            int Undefined = 0,
                UniqueHashIndex = 3,
                OrderedIndex = 6;
        }
        String/*_const char *_*/ getName() /*_const_*/;
        String/*_const char *_*/ getTable() /*_const_*/;
        int/*_unsigned_*/ getNoOfColumns() /*_const_*/;
        ColumnConst/*_const Column *_*/ getColumn(int/*_unsigned_*/ no) /*_const_*/;
        int/*_Type_*/ getType() /*_const_*/; // MMM type nameclash, meant Index.Type
        boolean getLogging() /*_const_*/;
        /*_virtual_*/ int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        /*_virtual_*/ int getObjectVersion() /*_const_*/;
        /*_virtual_*/ int getObjectId() /*_const_*/;
        NdbRecordConst/*_const NdbRecord *_*/ getDefaultRecord() /*_const_*/;
    }
    static public class Index extends Object implements IndexConst
    {
        public final native String/*_const char *_*/ getName() /*_const_*/;
        public final native String/*_const char *_*/ getTable() /*_const_*/;
        public final native int/*_unsigned_*/ getNoOfColumns() /*_const_*/;
        public final native ColumnConst/*_const Column *_*/ getColumn(int/*_unsigned_*/ no) /*_const_*/;
        public final native int/*_Type_*/ getType() /*_const_*/; // MMM type nameclash, meant Index.Type
        public final native boolean getLogging() /*_const_*/;
        public /*_virtual_*/ native int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        public /*_virtual_*/ native int getObjectVersion() /*_const_*/;
        public /*_virtual_*/ native int getObjectId() /*_const_*/;
        public final native NdbRecordConst/*_const NdbRecord *_*/ getDefaultRecord() /*_const_*/;
        static public native Index create(String/*_const char *_*/ name /*_= ""_*/); // MMM non-final, support for derivation
        static public /*_virtual_*/ native void delete(Index p0); // MMM non-final, support for derivation
        public final native int setName(String/*_const char *_*/ name);
        public final native int setTable(String/*_const char *_*/ name);
        public final native int addColumn(ColumnConst/*_const Column &_*/ c);
        public final native int addColumnName(String/*_const char *_*/ name);
        // MMM! support <in:String[]>: public final native int addColumnNames(int/*_unsigned_*/ noOfNames, String[]/*_const char * *_*/ names);
        public final native void setType(int/*_Type_*/ type); // MMM type nameclash, meant Index.Type
        public final native void setLogging(boolean enable);
    }
    static public class OptimizeTableHandle extends Wrapper
    {
        static public final native OptimizeTableHandle create();
        static public final native void delete(OptimizeTableHandle p0);
        public final native int next();
        public final native int close();
    }
    static public class OptimizeIndexHandle extends Wrapper
    {
        static public final native OptimizeIndexHandle create();
        static public final native void delete(OptimizeIndexHandle p0);
        public final native int next();
        public final native int close();
    }
    public interface EventConst extends ObjectConst
    {
        public interface /*_enum_*/ TableEvent
        {
            int TE_INSERT = 1<<0,
                TE_DELETE = 1<<1,
                TE_UPDATE = 1<<2,
                TE_DROP = 1<<4,
                TE_ALTER = 1<<5,
                TE_CREATE = 1<<6,
                TE_GCP_COMPLETE = 1<<7,
                TE_CLUSTER_FAILURE = 1<<8,
                TE_STOP = 1<<9,
                TE_NODE_FAILURE = 1<<10,
                TE_SUBSCRIBE = 1<<11,
                TE_UNSUBSCRIBE = 1<<12,
                TE_ALL = 0xFFFF;
        }
        public interface /*_enum_*/ EventDurability
        {
            int ED_UNDEFINED = 0,
                ED_PERMANENT = 3;
        }
        public interface /*_enum_*/ EventReport
        {
            int ER_UPDATED = 0,
                ER_ALL = 1,
                ER_SUBSCRIBE = 2;
        }
        String/*_const char *_*/ getName() /*_const_*/;
        NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ getTable() /*_const_*/;
        String/*_const char *_*/ getTableName() /*_const_*/;
        boolean getTableEvent(int/*_const TableEvent_*/ te) /*_const_*/;
        int/*_EventDurability_*/ getDurability() /*_const_*/;
        int/*_EventReport_*/ getReport() /*_const_*/;
        int getNoOfEventColumns() /*_const_*/;
        ColumnConst/*_const Column *_*/ getEventColumn(int/*_unsigned_*/ no) /*_const_*/;
        /*_virtual_*/ int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        /*_virtual_*/ int getObjectVersion() /*_const_*/;
        /*_virtual_*/ int getObjectId() /*_const_*/;
    }
    static public class Event extends Object implements EventConst
    {
        public final native String/*_const char *_*/ getName() /*_const_*/;
        public final native NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ getTable() /*_const_*/;
        public final native String/*_const char *_*/ getTableName() /*_const_*/;
        public final native boolean getTableEvent(int/*_const TableEvent_*/ te) /*_const_*/;
        public final native int/*_EventDurability_*/ getDurability() /*_const_*/;
        public final native int/*_EventReport_*/ getReport() /*_const_*/;
        public final native int getNoOfEventColumns() /*_const_*/;
        public final native ColumnConst/*_const Column *_*/ getEventColumn(int/*_unsigned_*/ no) /*_const_*/;
        public /*_virtual_*/ native int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        public /*_virtual_*/ native int getObjectVersion() /*_const_*/;
        public /*_virtual_*/ native int getObjectId() /*_const_*/;
        static public native Event create(String/*_const char *_*/ name); // MMM non-final, support for derivation
        static public native Event create(String/*_const char *_*/ name, NdbDictionary.TableConst/*_const NdbDictionary.Table &_*/ table); // MMM non-final, support for derivation
        static public /*_virtual_*/ native void delete(Event p0); // MMM non-final, support for derivation
        public final native int setName(String/*_const char *_*/ name);
        public final native void setTable(NdbDictionary.TableConst/*_const NdbDictionary.Table &_*/ table);
        public final native int setTable(String/*_const char *_*/ tableName);
        public final native void addTableEvent(int/*_const TableEvent_*/ te);
        public final native void setDurability(int/*_EventDurability_*/ p0);
        public final native void setReport(int/*_EventReport_*/ p0);
        public final native void addEventColumn(int/*_unsigned_*/ attrId);
        public final native void addEventColumn(String/*_const char *_*/ columnName);
        // MMM! support <in:String[]>: public final native void addEventColumns(int n, String[]/*_const char * *_*/ columnNames);
        public final native void mergeEvents(boolean flag);
    }
    public interface /*_enum_*/ NdbRecordFlags
    {
        int RecMysqldShrinkVarchar = 0x1,
            RecMysqldBitfield = 0x2;
    }
    static public interface RecordSpecificationConstArray extends ArrayWrapper< RecordSpecificationConst >
    {
    }
    static public class RecordSpecificationArray extends Wrapper implements RecordSpecificationConstArray
    {
        static public native RecordSpecificationArray create(int length);
        static public native void delete(RecordSpecificationArray e);
        public native RecordSpecification at(int i);
    }
    public interface /*_struct_*/ RecordSpecificationConst
    {
        ColumnConst/*_const Column *_*/ column();
        int/*_Uint32_*/ offset();
        int/*_Uint32_*/ nullbit_byte_offset();
        int/*_Uint32_*/ nullbit_bit_in_byte();
    }
    static public class /*_struct_*/ RecordSpecification extends Wrapper implements RecordSpecificationConst
    {
        static public final native int/*_Uint32_*/ size();
        public final native ColumnConst/*_const Column *_*/ column();
        public final native int/*_Uint32_*/ offset();
        public final native int/*_Uint32_*/ nullbit_byte_offset();
        public final native int/*_Uint32_*/ nullbit_bit_in_byte();
        public final native void column(ColumnConst/*_const Column *_*/ p0);
        public final native void offset(int/*_Uint32_*/ p0);
        public final native void nullbit_byte_offset(int/*_Uint32_*/ p0);
        public final native void nullbit_bit_in_byte(int/*_Uint32_*/ p0);
        static public final native RecordSpecification create();
        static public final native void delete(RecordSpecification p0);
    }
    public interface /*_enum_*/ RecordType
    {
        int TableAccess = 0 /*__*/,
            IndexAccess = 1 /*__*/;
    }
    static public final native int/*_RecordType_*/ getRecordType(NdbRecordConst/*_const NdbRecord *_*/ record);
    static public final native String/*_const char *_*/ getRecordTableName(NdbRecordConst/*_const NdbRecord *_*/ record);
    static public final native String/*_const char *_*/ getRecordIndexName(NdbRecordConst/*_const NdbRecord *_*/ record);
    static public final native boolean getFirstAttrId(NdbRecordConst/*_const NdbRecord *_*/ record, int[]/*_Uint32 &_*/ firstAttrId);
    static public final native boolean getNextAttrId(NdbRecordConst/*_const NdbRecord *_*/ record, int[]/*_Uint32 &_*/ attrId);
    static public final native boolean getOffset(NdbRecordConst/*_const NdbRecord *_*/ record, int/*_Uint32_*/ attrId, int[]/*_Uint32 &_*/ offset);
    static public final native boolean getNullBitOffset(NdbRecordConst/*_const NdbRecord *_*/ record, int/*_Uint32_*/ attrId, int[]/*_Uint32 &_*/ nullbit_byte_offset, int[]/*_Uint32 &_*/ nullbit_bit_in_byte);
    static public final native String/*_const char *_*/ getValuePtr(NdbRecordConst/*_const NdbRecord *_*/ record, String/*_const char *_*/ row, int/*_Uint32_*/ attrId);
    // MMM! support <out:BB> or check if needed: static public final native char * getValuePtr(NdbRecordConst/*_const NdbRecord *_*/ record, char * row, int/*_Uint32_*/ attrId);
    static public final native boolean isNull(NdbRecordConst/*_const NdbRecord *_*/ record, String/*_const char *_*/ row, int/*_Uint32_*/ attrId);
    static public final native int setNull(NdbRecordConst/*_const NdbRecord *_*/ record, ByteBuffer/*_char *_*/ row, int/*_Uint32_*/ attrId, boolean value);
    static public final native int/*_Uint32_*/ getRecordRowLength(NdbRecordConst/*_const NdbRecord *_*/ record);
    // MMM convenience function, marked with NDBAPI_SKIP: static public final native const unsigned char * getEmptyBitmask();
    public interface /*_struct_*/ AutoGrowSpecificationConst
    {
        int/*_Uint32_*/ min_free();
        long/*_Uint64_*/ max_size();
        long/*_Uint64_*/ file_size();
        String/*_const char *_*/ filename_pattern();
    }
    static public class /*_struct_*/ AutoGrowSpecification extends Wrapper implements AutoGrowSpecificationConst
    {
        public final native int/*_Uint32_*/ min_free();
        public final native long/*_Uint64_*/ max_size();
        public final native long/*_Uint64_*/ file_size();
        public final native String/*_const char *_*/ filename_pattern();
        public final native void min_free(int/*_Uint32_*/ p0);
        public final native void max_size(long/*_Uint64_*/ p0);
        public final native void file_size(long/*_Uint64_*/ p0);
        public final native void filename_pattern(String/*_const char *_*/ p0);
        static public final native AutoGrowSpecification create();
        static public final native void delete(AutoGrowSpecification p0);
    }
    public interface LogfileGroupConst extends ObjectConst
    {
        String/*_const char *_*/ getName() /*_const_*/;
        int/*_Uint32_*/ getUndoBufferSize() /*_const_*/;
        AutoGrowSpecificationConst/*_const AutoGrowSpecification &_*/ getAutoGrowSpecification() /*_const_*/;
        long/*_Uint64_*/ getUndoFreeWords() /*_const_*/;
        /*_virtual_*/ int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        /*_virtual_*/ int getObjectVersion() /*_const_*/;
        /*_virtual_*/ int getObjectId() /*_const_*/;
    }
    static public class LogfileGroup extends Object implements LogfileGroupConst
    {
        public final native String/*_const char *_*/ getName() /*_const_*/;
        public final native int/*_Uint32_*/ getUndoBufferSize() /*_const_*/;
        public final native AutoGrowSpecificationConst/*_const AutoGrowSpecification &_*/ getAutoGrowSpecification() /*_const_*/;
        public final native long/*_Uint64_*/ getUndoFreeWords() /*_const_*/;
        public /*_virtual_*/ native int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        public /*_virtual_*/ native int getObjectVersion() /*_const_*/;
        public /*_virtual_*/ native int getObjectId() /*_const_*/;
        static public native LogfileGroup create(); // MMM non-final, support for derivation
        static public native LogfileGroup create(LogfileGroupConst/*_const LogfileGroup &_*/ p0); // MMM non-final, support for derivation
        static public /*_virtual_*/ native void delete(LogfileGroup p0); // MMM non-final, support for derivation
        public final native void setName(String/*_const char *_*/ name);
        public final native void setUndoBufferSize(int/*_Uint32_*/ sz);
        public final native void setAutoGrowSpecification(AutoGrowSpecificationConst/*_const AutoGrowSpecification &_*/ p0);
    }
    public interface TablespaceConst extends ObjectConst
    {
        String/*_const char *_*/ getName() /*_const_*/;
        int/*_Uint32_*/ getExtentSize() /*_const_*/;
        AutoGrowSpecificationConst/*_const AutoGrowSpecification &_*/ getAutoGrowSpecification() /*_const_*/;
        String/*_const char *_*/ getDefaultLogfileGroup() /*_const_*/;
        int/*_Uint32_*/ getDefaultLogfileGroupId() /*_const_*/;
        /*_virtual_*/ int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        /*_virtual_*/ int getObjectVersion() /*_const_*/;
        /*_virtual_*/ int getObjectId() /*_const_*/;
    }
    static public class Tablespace extends Object implements TablespaceConst
    {
        public final native String/*_const char *_*/ getName() /*_const_*/;
        public final native int/*_Uint32_*/ getExtentSize() /*_const_*/;
        public final native AutoGrowSpecificationConst/*_const AutoGrowSpecification &_*/ getAutoGrowSpecification() /*_const_*/;
        public final native String/*_const char *_*/ getDefaultLogfileGroup() /*_const_*/;
        public final native int/*_Uint32_*/ getDefaultLogfileGroupId() /*_const_*/;
        public /*_virtual_*/ native int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        public /*_virtual_*/ native int getObjectVersion() /*_const_*/;
        public /*_virtual_*/ native int getObjectId() /*_const_*/;
        static public native Tablespace create(); // MMM non-final, support for derivation
        static public native Tablespace create(TablespaceConst/*_const Tablespace &_*/ p0); // MMM non-final, support for derivation
        static public /*_virtual_*/ native void delete(Tablespace p0); // MMM non-final, support for derivation
        public final native void setName(String/*_const char *_*/ name);
        public final native void setExtentSize(int/*_Uint32_*/ sz);
        public final native void setAutoGrowSpecification(AutoGrowSpecificationConst/*_const AutoGrowSpecification &_*/ p0);
        public final native void setDefaultLogfileGroup(String/*_const char *_*/ name);
        public final native void setDefaultLogfileGroup(LogfileGroupConst/*_const LogfileGroup &_*/ p0);
    }
    public interface DatafileConst extends ObjectConst
    {
        String/*_const char *_*/ getPath() /*_const_*/;
        long/*_Uint64_*/ getSize() /*_const_*/;
        long/*_Uint64_*/ getFree() /*_const_*/;
        String/*_const char *_*/ getTablespace() /*_const_*/;
        void getTablespaceId(ObjectId/*_ObjectId *_*/ dst) /*_const_*/;
        // MMM declared but not implemented in NDBAPI: int/*_Uint32_*/ getNode() /*_const_*/;
        // MMM declared but not implemented in NDBAPI: int/*_Uint32_*/ getFileNo() /*_const_*/;
        /*_virtual_*/ int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        /*_virtual_*/ int getObjectVersion() /*_const_*/;
        /*_virtual_*/ int getObjectId() /*_const_*/;
    }
    static public class Datafile extends Object implements DatafileConst
    {
        public final native String/*_const char *_*/ getPath() /*_const_*/;
        public final native long/*_Uint64_*/ getSize() /*_const_*/;
        public final native long/*_Uint64_*/ getFree() /*_const_*/;
        public final native String/*_const char *_*/ getTablespace() /*_const_*/;
        public final native void getTablespaceId(ObjectId/*_ObjectId *_*/ dst) /*_const_*/;
        // MMM declared but not implemented in NDBAPI: public final native int/*_Uint32_*/ getNode() /*_const_*/;
        // MMM declared but not implemented in NDBAPI: public final native int/*_Uint32_*/ getFileNo() /*_const_*/;
        public /*_virtual_*/ native int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        public /*_virtual_*/ native int getObjectVersion() /*_const_*/;
        public /*_virtual_*/ native int getObjectId() /*_const_*/;
        static public native Datafile create(); // MMM non-final, support for derivation
        static public native Datafile create(DatafileConst/*_const Datafile &_*/ p0); // MMM non-final, support for derivation
        static public /*_virtual_*/ native void delete(Datafile p0); // MMM non-final, support for derivation
        public final native void setPath(String/*_const char *_*/ name);
        public final native void setSize(long/*_Uint64_*/ p0);
        public final native int setTablespace(String/*_const char *_*/ name);
        public final native int setTablespace(TablespaceConst/*_const Tablespace &_*/ p0);
        // MMM declared but not implemented in NDBAPI: public final native void setNode(int/*_Uint32_*/ nodeId);
    }
    public interface UndofileConst extends ObjectConst
    {
        String/*_const char *_*/ getPath() /*_const_*/;
        long/*_Uint64_*/ getSize() /*_const_*/;
        String/*_const char *_*/ getLogfileGroup() /*_const_*/;
        void getLogfileGroupId(ObjectId/*_ObjectId *_*/ dst) /*_const_*/;
        // MMM declared but not implemented in NDBAPI: int/*_Uint32_*/ getNode() /*_const_*/;
        // MMM declared but not implemented in NDBAPI: int/*_Uint32_*/ getFileNo() /*_const_*/;
        /*_virtual_*/ int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        /*_virtual_*/ int getObjectVersion() /*_const_*/;
        /*_virtual_*/ int getObjectId() /*_const_*/;
    }
    static public class Undofile extends Object implements UndofileConst
    {
        public final native String/*_const char *_*/ getPath() /*_const_*/;
        public final native long/*_Uint64_*/ getSize() /*_const_*/;
        public final native String/*_const char *_*/ getLogfileGroup() /*_const_*/;
        public final native void getLogfileGroupId(ObjectId/*_ObjectId *_*/ dst) /*_const_*/;
        // MMM declared but not implemented in NDBAPI: public final native int/*_Uint32_*/ getNode() /*_const_*/;
        // MMM declared but not implemented in NDBAPI: public final native int/*_Uint32_*/ getFileNo() /*_const_*/;
        public /*_virtual_*/ native int/*_Object.Status_*/ getObjectStatus() /*_const_*/;
        public /*_virtual_*/ native int getObjectVersion() /*_const_*/;
        public /*_virtual_*/ native int getObjectId() /*_const_*/;
        static public native Undofile create(); // MMM non-final, support for derivation
        static public native Undofile create(UndofileConst/*_const Undofile &_*/ p0); // MMM non-final, support for derivation
        static public /*_virtual_*/ native void delete(Undofile p0); // MMM non-final, support for derivation
        public final native void setPath(String/*_const char *_*/ path);
        public final native void setSize(long/*_Uint64_*/ p0);
        public final native void setLogfileGroup(String/*_const char *_*/ name);
        public final native void setLogfileGroup(LogfileGroupConst/*_const LogfileGroup &_*/ p0);
        // MMM declared but not implemented in NDBAPI: public final native void setNode(int/*_Uint32_*/ nodeId);
    }
    public interface DictionaryConst
    {
        public interface /*_struct_*/ ListConst
        {
            static public interface ElementConstArray extends ArrayWrapper< ElementConst >
            {
            }
            static public class ElementArray extends Wrapper implements ElementConstArray
            {
                static public native ElementArray create(int length);
                static public native void delete(ElementArray e);
                public native Element at(int i);
            }
            public interface /*_struct_*/ ElementConst
            {
                int/*_unsigned_*/ id();
                int/*_Object.Type_*/ type();
                int/*_Object.State_*/ state();
                int/*_Object.Store_*/ store();
                int/*_Uint32_*/ temp();
                String/*_char *_*/ database(); // MMM confirmed as null-terminated C string
                String/*_char *_*/ schema(); // MMM confirmed as null-terminated C string
                String/*_char *_*/ name(); // MMM confirmed as null-terminated C string
            }
            static public class /*_struct_*/ Element extends Wrapper implements ElementConst
            {
                public final native int/*_unsigned_*/ id();
                public final native int/*_Object.Type_*/ type();
                public final native int/*_Object.State_*/ state();
                public final native int/*_Object.Store_*/ store();
                public final native int/*_Uint32_*/ temp();
                public final native String/*_char *_*/ database(); // MMM confirmed as null-terminated C string
                public final native String/*_char *_*/ schema(); // MMM confirmed as null-terminated C string
                public final native String/*_char *_*/ name(); // MMM confirmed as null-terminated C string
                public final native void id(int/*_unsigned_*/ p0);
                public final native void type(int/*_Object.Type_*/ p0);
                public final native void state(int/*_Object.State_*/ p0);
                public final native void store(int/*_Object.Store_*/ p0);
                public final native void temp(int/*_Uint32_*/ p0);
                // MMM unsupported mapping <in:String->char*> (and questionable NDBAPI usage): public final native void database(String/*_char *_*/ p0);
                // MMM unsupported mapping <in:String->char*> (and questionable NDBAPI usage): public final native void schema(String/*_char *_*/ p0);
                // MMM unsupported mapping <in:String->char*> (and questionable NDBAPI usage): public final native void name(String/*_char *_*/ p0);
                static public final native Element create();
                static public final native void delete(Element p0);
            }
            int/*_unsigned_*/ count();
            ElementArray/*_Element *_*/ elements();
        }
        static public class /*_struct_*/ List extends Wrapper implements ListConst
        {
            public final native int/*_unsigned_*/ count();
            public final native ElementArray/*_Element *_*/ elements();
            public final native void count(int/*_unsigned_*/ p0);
            public final native void elements(ElementArray/*_Element *_*/ p0);
            static public final native List create();
            static public final native void delete(List p0);
        }
        int listObjects(List/*_List &_*/ list, int/*_Object.Type_*/ type /*_= Object.TypeUndefined_*/) /*_const_*/;
        NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
        TableConst/*_const Table *_*/ getTable(String/*_const char *_*/ name) /*_const_*/;
        IndexConst/*_const Index *_*/ getIndex(String/*_const char *_*/ indexName, String/*_const char *_*/ tableName) /*_const_*/;
        int listIndexes(List/*_List &_*/ list, String/*_const char *_*/ tableName) /*_const_*/;
        int listEvents(List/*_List &_*/ list) /*_const_*/;
    }
    static public class Dictionary extends Wrapper implements DictionaryConst
    {
        public final native int listObjects(List/*_List &_*/ list, int/*_Object.Type_*/ type /*_= Object.TypeUndefined_*/) /*_const_*/;
        public final native NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
        public final native TableConst/*_const Table *_*/ getTable(String/*_const char *_*/ name) /*_const_*/;
        public final native IndexConst/*_const Index *_*/ getIndex(String/*_const char *_*/ indexName, String/*_const char *_*/ tableName) /*_const_*/;
        public final native int listIndexes(List/*_List &_*/ list, String/*_const char *_*/ tableName) /*_const_*/;
        public final native int listEvents(List/*_List &_*/ list) /*_const_*/;
        public final native int createEvent(EventConst/*_const Event &_*/ event);
        public final native int dropEvent(String/*_const char *_*/ eventName, int force /*_= 0_*/);
        public final native EventConst/*_const Event *_*/ getEvent(String/*_const char *_*/ eventName);
        public final native int createTable(TableConst/*_const Table &_*/ table);
        public final native int optimizeTable(TableConst/*_const Table &_*/ t, OptimizeTableHandle/*_OptimizeTableHandle &_*/ h);
        public final native int optimizeIndex(IndexConst/*_const Index &_*/ ind, OptimizeIndexHandle/*_OptimizeIndexHandle &_*/ h);
        public final native int dropTable(Table/*_Table &_*/ table);
        public final native int dropTable(String/*_const char *_*/ name);
        public final native boolean supportedAlterTable(TableConst/*_const Table &_*/ f, TableConst/*_const Table &_*/ t);
        public final native void removeCachedTable(String/*_const char *_*/ table);
        public final native void removeCachedIndex(String/*_const char *_*/ index, String/*_const char *_*/ table);
        public final native void invalidateTable(String/*_const char *_*/ table);
        public final native void invalidateIndex(String/*_const char *_*/ index, String/*_const char *_*/ table);
        public final native int createIndex(IndexConst/*_const Index &_*/ index, boolean offline /*_= false_*/);
        public final native int createIndex(IndexConst/*_const Index &_*/ index, TableConst/*_const Table &_*/ table, boolean offline /*_= false_*/);
        public final native int dropIndex(String/*_const char *_*/ indexName, String/*_const char *_*/ tableName);
        public final native int createLogfileGroup(LogfileGroupConst/*_const LogfileGroup &_*/ p0, ObjectId/*_ObjectId *_*/ p1 /*_= 0_*/);
        public final native int dropLogfileGroup(LogfileGroupConst/*_const LogfileGroup &_*/ p0);
        // MMM object-copy returns not supported: public final native LogfileGroup getLogfileGroup(String/*_const char *_*/ name);
        public final native int createTablespace(TablespaceConst/*_const Tablespace &_*/ p0, ObjectId/*_ObjectId *_*/ p1 /*_= 0_*/);
        public final native int dropTablespace(TablespaceConst/*_const Tablespace &_*/ p0);
        // MMM object-copy returns not supported: public final native Tablespace getTablespace(String/*_const char *_*/ name);
        // MMM object-copy returns not supported: public final native Tablespace getTablespace(int/*_Uint32_*/ tablespaceId);
        public final native int createDatafile(DatafileConst/*_const Datafile &_*/ p0, boolean overwrite_existing /*_= false_*/, ObjectId/*_ObjectId *_*/ p1 /*_= 0_*/);
        public final native int dropDatafile(DatafileConst/*_const Datafile &_*/ p0);
        // MMM object-copy returns not supported: public final native Datafile getDatafile(int/*_Uint32_*/ node, String/*_const char *_*/ path);
        public final native int createUndofile(UndofileConst/*_const Undofile &_*/ p0, boolean overwrite_existing /*_= false_*/, ObjectId/*_ObjectId *_*/ p1 /*_= 0_*/);
        public final native int dropUndofile(UndofileConst/*_const Undofile &_*/ p0);
        // MMM object-copy returns not supported: public final native Undofile getUndofile(int/*_Uint32_*/ node, String/*_const char *_*/ path);
        public final native NdbRecord/*_NdbRecord *_*/ createRecord(TableConst/*_const Table *_*/ table, RecordSpecificationConstArray/*_const RecordSpecification *_*/ recSpec, int/*_Uint32_*/ length, int/*_Uint32_*/ elemSize, int/*_Uint32_*/ flags /*_= 0_*/);
        public final native NdbRecord/*_NdbRecord *_*/ createRecord(IndexConst/*_const Index *_*/ index, TableConst/*_const Table *_*/ table, RecordSpecificationConstArray/*_const RecordSpecification *_*/ recSpec, int/*_Uint32_*/ length, int/*_Uint32_*/ elemSize, int/*_Uint32_*/ flags /*_= 0_*/);
        public final native NdbRecord/*_NdbRecord *_*/ createRecord(IndexConst/*_const Index *_*/ index, RecordSpecificationConstArray/*_const RecordSpecification *_*/ recSpec, int/*_Uint32_*/ length, int/*_Uint32_*/ elemSize, int/*_Uint32_*/ flags /*_= 0_*/);
        public final native void releaseRecord(NdbRecord/*_NdbRecord *_*/ rec);
    }
}
