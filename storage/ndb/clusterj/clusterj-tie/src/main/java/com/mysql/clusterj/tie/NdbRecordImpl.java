/*
   Copyright (c) 2012, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.tie;

import java.math.BigDecimal;
import java.math.BigInteger;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.ColumnType;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.clusterj.tie.DbImpl.BufferManager;

import com.mysql.ndbjtie.ndbapi.NdbRecord;
import com.mysql.ndbjtie.ndbapi.NdbRecordConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.ColumnConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.IndexConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.RecordSpecification;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.RecordSpecificationArray;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;

/**
 * Wrapper around an NdbRecord. Operations may use one or two instances.
 * <ul><li>The table implementation can be used for create, read, update, or delete
 * using an NdbRecord that defines every column in the table. If a Projection,
 * projected column names are set during construction of the DomainTypeHandler,
 * and the projection can be used for create only if all columns that do not have
 * a default value are included in the projection.
 * The name of the NdbRecordImpl includes the name of the table plus projected column indicator.
 * The NdbRecord created by the constructor includes only columns in the projection.
 * </li><li>The index implementation for unique indexes can be used with a unique lookup operation.
 * </li><li>The index implementation for ordered (non-unique) indexes can be used with an index scan operation.
 * </li></ul>
 * After construction, the size of the buffer required for operations is available.
 * The instance is read-only and can be shared among all threads that use the same cluster connection.
 * The NdbRecordImpl object itself does not own any data or index buffers.
 * Methods on the instance generally require a buffer to be passed, which is modified by the method.
 * The NdbRecord instance is released when the cluster
 * connection is closed or when schema change invalidates it. Column values can be set using a provided
 * buffer and buffer manager.
 */
public class NdbRecordImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(NdbRecordImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(NdbRecordImpl.class);

    /** The size of the NdbRecord struct */
    protected final static int SIZEOF_RECORD_SPECIFICATION =
            ClusterConnectionServiceImpl.SIZEOF_RECORD_SPECIFICATION;

    /** The NdbRecord for this operation, created at construction */
    private NdbRecord ndbRecord = null;

    /** The store columns for this operation */
    protected Column[] storeColumns = null;

    /** The RecordSpecificationArray used to define the columns in the NdbRecord */
    private RecordSpecificationArray recordSpecificationArray;

    /** The NdbTable */
    TableConst tableConst = null;

    /** The NdbIndex, which will be null for complete-table instances */
    IndexConst indexConst = null;

    /** The name of this NdbRecord; table name + index name */
    String name;

    /** The size of the buffer for this NdbRecord, set during analyzeColumns */
    protected int bufferSize;

    /** The maximum column id for this NdbRecord */
    protected int maximumColumnId;

    /** The offsets into the buffer for each column */
    protected int[] offsets;

    /** The lengths of the column data */
    protected int[] lengths;

    /** Values for setting column mask and null bit mask: 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 */
    protected final static byte[] BIT_IN_BYTE_MASK = new byte[] {1, 2, 4, 8, 16, 32, 64, -128};

    /** Values for resetting column mask and null bit mask: 0xfe, 0xfd, 0xfb, 0xf7, 0xef, 0xdf, 0xbf, 0x7f */
    protected static final byte[] RESET_BIT_IN_BYTE_MASK = new byte[] {-2, -3, -5, -9, -17, -33, -65, 127};

    /** Values for masking BIT column types for byte, short, and int domain types indexed by column length */
    protected static int[] BIT_INT_MASK = new int[]
        {(int)0,
         (int)0x1,         (int)0x3,         (int)0x7,         (int)0xf,
         (int)0x1f,        (int)0x3f,        (int)0x7f,        (int)0xff,
         (int)0x1ff,       (int)0x3ff,       (int)0x7ff,       (int)0xfff,
         (int)0x1fff,      (int)0x3fff,      (int)0x7fff,      (int)0xffff,
         (int)0x1ffff,     (int)0x3ffff,     (int)0x7ffff,     (int)0xfffff,
         (int)0x1fffff,    (int)0x3fffff,    (int)0x7fffff,    (int)0xffffff,
         (int)0x1ffffff,   (int)0x3ffffff,   (int)0x7ffffff,   (int)0xfffffff,
         (int)0x1fffffff,  (int)0x3fffffff,  (int)0x7fffffff,  (int)0xffffffff
        };

    /** Values for masking BIT column types for long domain types indexed by column length */
    protected static long[] BIT_LONG_MASK = new long[]
        {(long)0,
        (long)0x1,                 (long)0x3,                 (long)0x7,                 (long)0xf,
        (long)0x1f,                (long)0x3f,                (long)0x7f,                (long)0xff,
        (long)0x1ff,               (long)0x3ff,               (long)0x7ff,               (long)0xfff,
        (long)0x1fff,              (long)0x3fff,              (long)0x7fff,              (long)0xffff,
        (long)0x1ffff,             (long)0x3ffff,             (long)0x7ffff,             (long)0xfffff,
        (long)0x1fffff,            (long)0x3fffff,            (long)0x7fffff,            (long)0xffffff,
        (long)0x1ffffff,           (long)0x3ffffff,           (long)0x7ffffff,           (long)0xfffffff,
        (long)0x1fffffff,          (long)0x3fffffff,          (long)0x7fffffff,          (long)0xffffffff,
        (long)0x1ffffffffL,        (long)0x3ffffffffL,        (long)0x7ffffffffL,        (long)0xfffffffffL,
        (long)0x1fffffffffL,       (long)0x3fffffffffL,       (long)0x7fffffffffL,       (long)0xffffffffffL,
        (long)0x1ffffffffffL,      (long)0x3ffffffffffL,      (long)0x7ffffffffffL,      (long)0xfffffffffffL,
        (long)0x1fffffffffffL,     (long)0x3fffffffffffL,     (long)0x7fffffffffffL,     (long)0xffffffffffffL,
        (long)0x1ffffffffffffL,    (long)0x3ffffffffffffL,    (long)0x7ffffffffffffL,    (long)0xfffffffffffffL,
        (long)0x1fffffffffffffL,   (long)0x3fffffffffffffL,   (long)0x7fffffffffffffL,   (long)0xffffffffffffffL,
        (long)0x1ffffffffffffffL,  (long)0x3ffffffffffffffL,  (long)0x7ffffffffffffffL,  (long)0xfffffffffffffffL,
        (long)0x1fffffffffffffffL, (long)0x3fffffffffffffffL, (long)0x7fffffffffffffffL, (long)0xffffffffffffffffL
        };

    /** The null indicator for the field bit in the short */
    protected int nullbitBitInByte[] = null;

    /** The null indicator for the field byte offset*/
    protected int nullbitByteOffset[] = null;

    /** The size of the null indicator byte array */
    protected int nullIndicatorSize;

    /** The maximum length of any column in this operation */
    protected int maximumColumnLength;

    /** The dictionary used to create (and release) the NdbRecord */
    private Dictionary ndbDictionary;

    /** Number of columns for this NdbRecord */
    private int numberOfTableColumns;

    /** ByteBuffer pool for new records, created during createNdbRecord once the buffer size is known */
    private FixedByteBufferPoolImpl bufferPool = null;

    /** These fields are only used during construction of the RecordSpecificationArray */
    int offset = 0;
    int nullablePosition = 0;
    byte[] defaultValues;

    private int[] recordSpecificationIndexes = null;

    /** The autoincrement column or null if none */
    private Column autoIncrementColumn;

    /** The function to handle setting autoincrement values */
    private AutoIncrementValueSetter autoIncrementValueSetter;

    /** The set of projected column names */
    private Set<String> projectedColumnSet;

    /** Constructor for table operations. The NdbRecord has entries just for
     * projected columns.
     * @param storeTable the store table
     * @param ndbDictionary the ndb dictionary
     */
    protected NdbRecordImpl(Table storeTable, Dictionary ndbDictionary) {
        this.ndbDictionary = ndbDictionary;
        this.tableConst = getNdbTable(storeTable.getName());
        this.name = storeTable.getKey();
        this.numberOfTableColumns = tableConst.getNoOfColumns();
        this.recordSpecificationIndexes = new int[numberOfTableColumns];
        this.offsets = new int[numberOfTableColumns];
        this.lengths = new int[numberOfTableColumns];
        this.nullbitBitInByte = new int[numberOfTableColumns];
        this.nullbitByteOffset = new int[numberOfTableColumns];
        this.storeColumns = new Column[numberOfTableColumns];
        this.projectedColumnSet = new TreeSet<String>();
        for (String projectedColumnName: storeTable.getProjectedColumnNames()) {
            this.projectedColumnSet.add(projectedColumnName);
        }
        try {
           this.autoIncrementColumn = storeTable.getAutoIncrementColumn();
            if (this.autoIncrementColumn != null) {
                chooseAutoIncrementValueSetter();
            }
            this.ndbRecord = createNdbRecord(storeTable, ndbDictionary);
            if (logger.isDetailEnabled()) logger.detail(storeTable.getName() + " " + dumpDefinition());
            initializeDefaultBuffer();
        } finally {
            // delete the RecordSpecificationArray since it is no longer needed
            RecordSpecificationArray.delete(this.recordSpecificationArray);
        }
    }

    /** Constructor for index operations. The NdbRecord has columns just for
     * the columns in the index. 
     * 
     * @param storeIndex the store index
     * @param storeTable the store table
     * @param ndbDictionary the ndb dictionary
     */
    protected NdbRecordImpl(Index storeIndex, Table storeTable, Dictionary ndbDictionary) {
        this.ndbDictionary = ndbDictionary;
        this.tableConst = getNdbTable(storeTable.getName());
        this.indexConst = getNdbIndex(storeIndex.getInternalName(), tableConst.getName());
        this.name = storeTable.getName() + ":" + storeIndex.getInternalName();
        this.numberOfTableColumns = tableConst.getNoOfColumns();
        int numberOfIndexColumns = this.indexConst.getNoOfColumns();
        this.recordSpecificationIndexes = new int[numberOfTableColumns];
        this.offsets = new int[numberOfTableColumns];
        this.lengths = new int[numberOfTableColumns];
        this.nullbitBitInByte = new int[numberOfTableColumns];
        this.nullbitByteOffset = new int[numberOfTableColumns];
        this.storeColumns = new Column[numberOfTableColumns];
        this.projectedColumnSet = new TreeSet<String>();
        for (String projectedColumnName: storeTable.getProjectedColumnNames()) {
            this.projectedColumnSet.add(projectedColumnName);
        }
        this.recordSpecificationArray = RecordSpecificationArray.create(numberOfIndexColumns);
        try {
            this.ndbRecord = createNdbRecord(storeIndex, storeTable, ndbDictionary);
            if (logger.isDetailEnabled()) logger.detail(storeIndex.getInternalName() + " " + dumpDefinition());
            initializeDefaultBuffer();
        } finally {
            // delete the RecordSpecificationArray since it is no longer needed
            RecordSpecificationArray.delete(this.recordSpecificationArray);
        }
    }

    /** Initialize the byte buffer containing default values for all columns.
     * Non-null columns are initialized to zero. Nullable columns are initialized to null.
     * When a new byte buffer is required, the byte buffer is used for initialization.
     */
    private void initializeDefaultBuffer() {
        // create the default value for the buffer: null values or zeros for all columns
        defaultValues = new byte[bufferSize];
        ByteBuffer zeros = bufferPool.borrowBuffer();
        zeros.order(ByteOrder.nativeOrder());
        // just to be sure, initialize with zeros
        zeros.put(defaultValues);
        // not all columns are set at this point, so only check for those that are set
        for (Column storeColumn: storeColumns) {
            // nullable columns get the null bit set
            if (storeColumn != null && storeColumn.getNullable()) {
                setNull(zeros, storeColumn);
            }
        }
        zeros.position(0);
        zeros.limit(bufferSize);
        zeros.get(defaultValues);
        bufferPool.returnBuffer(zeros);
        // default values is now immutable and can be used thread-safe
    }

    /** Allocate a new direct buffer and optionally initialize with default values for all columns.
     * The buffer returned is positioned at 0 with the limit set to the buffer size.
     * @param initialize true to initialize the buffer with default values
     * @return a new byte buffer for use with this NdbRecord.
     */
    protected ByteBuffer newBuffer(boolean initialize) {
        ByteBuffer result = bufferPool.borrowBuffer();
        initializeBuffer(result, initialize);
        return result;
    }

    /** Allocate a new direct buffer and initialize with default values for all columns.
     * The buffer returned is positioned at 0 with the limit set to the buffer size.
     * @return a new byte buffer for use with this NdbRecord.
     */
    protected ByteBuffer newBuffer() {
        return newBuffer(true);
    }

    /** Return the buffer to the buffer pool */
    protected void returnBuffer(ByteBuffer buffer) {
        bufferPool.returnBuffer(buffer);
    }

    /** Check the NdbRecord buffer guard */
    protected void checkGuard(ByteBuffer buffer, String where) {
        bufferPool.checkGuard(buffer, where);
    }

    /** Make a buffer ready for use and optionally initialize it with default values for all columns.
     * Set the buffer's position to zero and limit to its capacity.
     * @param buffer the buffer
     * @param initialize true to initialize the buffer with default values
     */
    protected void initializeBuffer(ByteBuffer buffer, boolean initialize) {
        buffer.order(ByteOrder.nativeOrder());
        buffer.clear();
        if (initialize) {
            buffer.put(defaultValues);
            buffer.clear();
        }
    }

    /** Make a buffer ready for use and initialize it with default values for all columns.
     * Set the buffer's position to zero and limit to its capacity.
     * @param buffer the buffer
     */
    protected void initializeBuffer(ByteBuffer buffer) {
        initializeBuffer(buffer, true);
    }

    public int setNull(ByteBuffer buffer, Column storeColumn) {
        int columnId = storeColumn.getColumnId();
        if (!storeColumn.getNullable()) {
            return columnId;
        }
        int index = nullbitByteOffset[columnId];
        byte mask = BIT_IN_BYTE_MASK[nullbitBitInByte[columnId]];
        byte nullbyte = buffer.get(index);
        buffer.put(index, (byte)(nullbyte|mask));
        return columnId;
    }

    public int resetNull(ByteBuffer buffer, Column storeColumn) {
        int columnId = storeColumn.getColumnId();
        if (!storeColumn.getNullable()) {
            return columnId;
        }
        int index = nullbitByteOffset[columnId];
        byte mask = RESET_BIT_IN_BYTE_MASK[nullbitBitInByte[columnId]];
        byte nullbyte = buffer.get(index);
        buffer.put(index, (byte)(nullbyte & mask));
        return columnId;
    }

    public int setBigInteger(ByteBuffer buffer, Column storeColumn, BigInteger value) {
        resetNull(buffer, storeColumn);
        int columnId = storeColumn.getColumnId();
        int newPosition = offsets[columnId];
        buffer.position(newPosition);
        Utility.convertValue(buffer, storeColumn, value);
        buffer.limit(bufferSize);
        buffer.position(0);
        return columnId;
    }

    public int setByte(ByteBuffer buffer, Column storeColumn, byte value) {
        resetNull(buffer, storeColumn);
        int columnId = storeColumn.getColumnId();
        if (storeColumn.getType() == ColumnType.Bit) {
            // the byte is stored as a BIT array of four bytes
            buffer.putInt(offsets[columnId], value & 0xff);
        } else {
            buffer.put(offsets[columnId], value);
        }
        buffer.limit(bufferSize);
        buffer.position(0);
        return columnId;
    }

    public int setBytes(ByteBuffer buffer, Column storeColumn, byte[] value) {
        resetNull(buffer, storeColumn);
        int columnId = storeColumn.getColumnId();
        int offset = offsets[columnId];
        int length = storeColumn.getLength();
        int prefixLength = storeColumn.getPrefixLength();
        if (length < value.length) {
            throw new ClusterJUserException(local.message("ERR_Data_Too_Long",
                    storeColumn.getName(), length, value.length));
        }
        buffer.limit(offset + prefixLength + length);
        buffer.position(offset);
        Utility.convertValue(buffer, storeColumn, value);
        buffer.limit(bufferSize);
        buffer.position(0);
        return columnId;
    }

    public int setDecimal(ByteBuffer buffer, Column storeColumn, BigDecimal value) {
        resetNull(buffer, storeColumn);
        int columnId = storeColumn.getColumnId();
        int newPosition = offsets[columnId];
        buffer.position(newPosition);
        Utility.convertValue(buffer, storeColumn, value);
        return columnId;
    }

    public int setDouble(ByteBuffer buffer, Column storeColumn, double value) {
        resetNull(buffer, storeColumn);
        int columnId = storeColumn.getColumnId();
        buffer.putDouble(offsets[columnId], value);
        return columnId;
    }

    public int setFloat(ByteBuffer buffer, Column storeColumn, float value) {
        resetNull(buffer, storeColumn);
        int columnId = storeColumn.getColumnId();
        buffer.putFloat(offsets[columnId], value);
        return columnId;
    }

    public int setInt(ByteBuffer buffer, Column storeColumn, int value) {
        resetNull(buffer, storeColumn);
        int columnId = storeColumn.getColumnId();
        int storageValue = Utility.convertIntValueForStorage(storeColumn, value);
        buffer.putInt(offsets[columnId], storageValue);
        return columnId;
    }

    public int setLong(ByteBuffer buffer, Column storeColumn, long value) {
        resetNull(buffer, storeColumn);
        int columnId = storeColumn.getColumnId();
        long storeValue = Utility.convertLongValueForStorage(storeColumn, value);
        buffer.putLong(offsets[columnId], storeValue);
        return columnId;
    }

    public int setShort(ByteBuffer buffer, Column storeColumn, short value) {
        resetNull(buffer, storeColumn);
        int columnId = storeColumn.getColumnId();
        if (storeColumn.getLength() == 4) {
            // the short is stored as a BIT array of four bytes
            buffer.putInt(offsets[columnId], value & 0xffff);
        } else {
            buffer.putShort(offsets[columnId], (short)value);
        }
        return columnId;
    }

    public int setString(ByteBuffer buffer, BufferManager bufferManager, Column storeColumn, String value) {
        resetNull(buffer, storeColumn);
        int columnId = storeColumn.getColumnId();
        int offset = offsets[columnId];
        int prefixLength = storeColumn.getPrefixLength();
        int length = storeColumn.getLength() + prefixLength;
        buffer.limit(offset + length);
        buffer.position(offset);
        // TODO provide the buffer to Utility.encode to avoid copying
        // for now, use the encode method to encode the value then copy it
        ByteBuffer converted = Utility.encode(value, storeColumn, bufferManager);
        if (length < converted.remaining()) {
            throw new ClusterJUserException(local.message("ERR_Data_Too_Long",
                    storeColumn.getName(), length - prefixLength, converted.remaining() - prefixLength));
        }
        buffer.put(converted);
        buffer.limit(bufferSize);
        buffer.position(0);
        return columnId;
    }

    public boolean getBoolean(ByteBuffer buffer, int columnId) {
        int value = buffer.getInt(offsets[columnId]);
        return Utility.getBoolean(storeColumns[columnId], value);
    }

    public byte getByte(ByteBuffer buffer, int columnId) {
        Column storeColumn = storeColumns[columnId];
        if (storeColumn.getType() == ColumnType.Bit) {
            int mask = BIT_INT_MASK[storeColumn.getLength()];
            // the byte was stored in a BIT column as four bytes
            return (byte)(mask & buffer.getInt(offsets[columnId]));
        } else {
            // the byte was stored as a byte
            return buffer.get(offsets[columnId]);
        }
    }

    public byte[] getBytes(ByteBuffer byteBuffer, int columnId) {
        return getBytes(byteBuffer, storeColumns[columnId]);
    }

    public byte[] getBytes(ByteBuffer byteBuffer, Column storeColumn) {
        int columnId = storeColumn.getColumnId();
        if (isNull(byteBuffer, columnId)) {
            return null;
        }
        int prefixLength = storeColumn.getPrefixLength();
        int actualLength = lengths[columnId];
        int offset = offsets[columnId];
        switch (prefixLength) {
            case 0:
                break;
            case 1:
                actualLength = (byteBuffer.get(offset) + 256) % 256;
                offset += 1;
                break;
            case 2:
                actualLength = (byteBuffer.get(offset) + 256) % 256;
                int length2 = (byteBuffer.get(offset + 1) + 256) % 256;
                actualLength += 256 * length2;
                offset += 2;
                break;
            default:
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Invalid_Prefix_Length", prefixLength));
        }
        byteBuffer.position(offset);
        byte[] result = new byte[actualLength];
        byteBuffer.get(result);
        byteBuffer.limit(bufferSize);
        byteBuffer.position(0);
        return result;
     }

    public double getDouble(ByteBuffer buffer, int columnId) {
        double result = buffer.getDouble(offsets[columnId]);
        return result;
    }

    public float getFloat(ByteBuffer buffer, int columnId) {
        float result = buffer.getFloat(offsets[columnId]);
        return result;
    }

    public int getInt(ByteBuffer buffer, int columnId) {
        int value = buffer.getInt(offsets[columnId]);
        Column storeColumn = storeColumns[columnId];
        if (storeColumn.getType() == ColumnType.Bit) {
            int mask = BIT_INT_MASK[storeColumn.getLength()];
            return mask & Utility.getInt(storeColumns[columnId], value);
        }
        return Utility.getInt(storeColumns[columnId], value);
    }

    public long getLong(ByteBuffer buffer, int columnId) {
        long value = buffer.getLong(offsets[columnId]);
        Column storeColumn = storeColumns[columnId];
        if (storeColumn.getType() == ColumnType.Bit) {
            long mask = BIT_LONG_MASK[storeColumn.getLength()];
            return mask & Utility.getLong(storeColumns[columnId], value);
        }
        return Utility.getLong(storeColumns[columnId], value);
    }

    public short getShort(ByteBuffer buffer, int columnId) {
        Column storeColumn = storeColumns[columnId];
        if (storeColumn.getType() == ColumnType.Bit) {
            int mask = BIT_INT_MASK[storeColumn.getLength()];
            // the short was stored in a BIT column as four bytes
            return (short)(mask & buffer.getInt(offsets[columnId]));
        } else {
            // the short was stored as a short
            return buffer.getShort(offsets[columnId]);
        }
    }

    public String getString(ByteBuffer byteBuffer, int columnId, BufferManager bufferManager) {
      if (isNull(byteBuffer, columnId)) {
          return null;
      }
      Column storeColumn = storeColumns[columnId];
      int prefixLength = storeColumn.getPrefixLength();
      int actualLength;
      int offset = offsets[columnId];
      byteBuffer.limit(byteBuffer.capacity());
      switch (prefixLength) {
          case 0:
              actualLength = lengths[columnId];
              break;
          case 1:
              actualLength = (byteBuffer.get(offset) + 256) % 256;
              offset += 1;
              break;
          case 2:
              actualLength = (byteBuffer.get(offset) + 256) % 256;
              int length2 = (byteBuffer.get(offset + 1) + 256) % 256;
              actualLength += 256 * length2;
              offset += 2;
              break;
          default:
              throw new ClusterJFatalInternalException(
                      local.message("ERR_Invalid_Prefix_Length", prefixLength));
      }
      byteBuffer.position(offset);
      byteBuffer.limit(offset + actualLength);

      String result = Utility.decode(byteBuffer, storeColumn.getCharsetNumber(), bufferManager);
      if(prefixLength == 0) {
          result = result.trim();
      }
      byteBuffer.limit(bufferSize);
      byteBuffer.position(0);
      return result;
    }

    public BigInteger getBigInteger(ByteBuffer byteBuffer, int columnId) {
        Column storeColumn = storeColumns[columnId];
        int index = storeColumn.getColumnId();
        int offset = offsets[index];
        int precision = storeColumn.getPrecision();
        int scale = storeColumn.getScale();
        int length = Utility.getDecimalColumnSpace(precision, scale);
        byteBuffer.position(offset);
        BigInteger result = Utility.getBigInteger(byteBuffer, length, precision, scale);
        byteBuffer.limit(bufferSize);
        byteBuffer.position(0);
        return result;
    }

    public BigInteger getBigInteger(ByteBuffer byteBuffer, Column storeColumn) {
        int index = storeColumn.getColumnId();
        int offset = offsets[index];
        int precision = storeColumn.getPrecision();
        int scale = storeColumn.getScale();
        int length = Utility.getDecimalColumnSpace(precision, scale);
        byteBuffer.position(offset);
        BigInteger result = Utility.getBigInteger(byteBuffer, length, precision, scale);
        byteBuffer.limit(bufferSize);
        byteBuffer.position(0);
        return result;
    }

    public BigDecimal getDecimal(ByteBuffer byteBuffer, int columnId) {
        Column storeColumn = storeColumns[columnId];
        int index = storeColumn.getColumnId();
        int offset = offsets[index];
        int precision = storeColumn.getPrecision();
        int scale = storeColumn.getScale();
        int length = Utility.getDecimalColumnSpace(precision, scale);
        byteBuffer.position(offset);
        BigDecimal result = Utility.getDecimal(byteBuffer, length, precision, scale);
        byteBuffer.limit(bufferSize);
        byteBuffer.position(0);
        return result;
      }

    public BigDecimal getDecimal(ByteBuffer byteBuffer, Column storeColumn) {
      int index = storeColumn.getColumnId();
      int offset = offsets[index];
      int precision = storeColumn.getPrecision();
      int scale = storeColumn.getScale();
      int length = Utility.getDecimalColumnSpace(precision, scale);
      byteBuffer.position(offset);
      BigDecimal result = Utility.getDecimal(byteBuffer, length, precision, scale);
      byteBuffer.limit(bufferSize);
      byteBuffer.position(0);
      return result;
    }

    public Boolean getObjectBoolean(ByteBuffer byteBuffer, int columnId) {
        if (isNull(byteBuffer, columnId)) {
            return null;
        }
        return Boolean.valueOf(getBoolean(byteBuffer, columnId));        
    }

    public Boolean getObjectBoolean(ByteBuffer byteBuffer, Column storeColumn) {
        return getObjectBoolean(byteBuffer, storeColumn.getColumnId());
    }

    public Byte getObjectByte(ByteBuffer byteBuffer, int columnId) {
        if (isNull(byteBuffer, columnId)) {
            return null;
        }
        return getByte(byteBuffer, columnId);        
    }

    public Byte getObjectByte(ByteBuffer byteBuffer, Column storeColumn) {
        return getObjectByte(byteBuffer, storeColumn.getColumnId());
    }

    public Float getObjectFloat(ByteBuffer byteBuffer, int columnId) {
        if (isNull(byteBuffer, columnId)) {
            return null;
        }
        return getFloat(byteBuffer, columnId);        
    }

    public Float getObjectFloat(ByteBuffer byteBuffer, Column storeColumn) {
        return getObjectFloat(byteBuffer, storeColumn.getColumnId());
    }

    public Double getObjectDouble(ByteBuffer byteBuffer, int columnId) {
        if (isNull(byteBuffer, columnId)) {
            return null;
        }
        return getDouble(byteBuffer, columnId);        
    }

    public Double getObjectDouble(ByteBuffer byteBuffer, Column storeColumn) {
        return getObjectDouble(byteBuffer, storeColumn.getColumnId());
    }

    public Integer getObjectInteger(ByteBuffer byteBuffer, int columnId) {
        if (isNull(byteBuffer, columnId)) {
            return null;
        }
        return getInt(byteBuffer, columnId);        
    }

    public Integer getObjectInteger(ByteBuffer byteBuffer, Column storeColumn) {
        return getObjectInteger(byteBuffer, storeColumn.getColumnId());
    }

    public Long getObjectLong(ByteBuffer byteBuffer, int columnId) {
        if (isNull(byteBuffer, columnId)) {
            return null;
        }
        return getLong(byteBuffer, columnId);        
    }

    public Long getObjectLong(ByteBuffer byteBuffer, Column storeColumn) {
        return getObjectLong(byteBuffer, storeColumn.getColumnId());
    }

    public Short getObjectShort(ByteBuffer byteBuffer, int columnId) {
        if (isNull(byteBuffer, columnId)) {
            return null;
        }
        return getShort(byteBuffer, columnId);        
    }

    public Short getObjectShort(ByteBuffer byteBuffer, Column storeColumn) {
        return getObjectShort(byteBuffer, storeColumn.getColumnId());
    }

    public boolean isNull(ByteBuffer buffer, int columnId) {
        if (!storeColumns[columnId].getNullable()) {
            return false;
        }
        byte nullbyte = buffer.get(nullbitByteOffset[columnId]);
        boolean result = isSet(nullbyte, nullbitBitInByte[columnId]);
        return result;
    }

    public boolean isPresent(byte[] mask, int columnId) {
        byte present = mask[columnId/8];
        return isSet(present, columnId % 8);
    }

    public void markPresent(byte[] mask, int columnId) {
        int offset = columnId/8;
        int bitMask = BIT_IN_BYTE_MASK[columnId % 8];
        mask[offset] |= (byte)bitMask;
    }

    protected boolean isSet(byte test, int bitInByte) {
        int mask = BIT_IN_BYTE_MASK[bitInByte];
        boolean result = (test & mask) != 0;
        return result;
    }

    protected static void handleError(Object object, Dictionary ndbDictionary) {
        if (object != null) {
            return;
        } else {
            Utility.throwError(null, ndbDictionary.getNdbError());
        }
    }

    protected NdbRecord createNdbRecord(Index storeIndex, Table storeTable, Dictionary ndbDictionary) {
        String[] columnNames = storeIndex.getColumnNames();
        this.recordSpecificationArray = RecordSpecificationArray.create(columnNames.length);
        // analyze columns; sort into alignment buckets, allocate space in the buffer
        // and build the record specification array
        analyzeColumns(storeTable, columnNames);
        // create the NdbRecord
        NdbRecord result = ndbDictionary.createRecord(indexConst, tableConst, recordSpecificationArray,
                columnNames.length, SIZEOF_RECORD_SPECIFICATION, 0);
        handleError(result, ndbDictionary);
        int rowLength = NdbDictionary.getRecordRowLength(result);
        // create the buffer pool now that the size of the record is known
        if (this.bufferSize < rowLength) {
            logger.warn("NdbRecordImpl.createNdbRecord rowLength for " + this.name + " is " + rowLength +
                    " but we only allocate length of " + this.bufferSize);
            this.bufferSize = rowLength;
        }
        bufferPool = new FixedByteBufferPoolImpl(this.bufferSize, this.name);
        return result;
    }

    protected NdbRecord createNdbRecord(Table storeTable, Dictionary ndbDictionary) {
        // only allocate space in the NdbRecord for projected columns
        String[] columnNames = storeTable.getColumnNames();
        String[] projectedColumnNames = storeTable.getProjectedColumnNames();
        this.recordSpecificationArray = RecordSpecificationArray.create(projectedColumnNames.length);
        // analyze columns; sort into alignment buckets, allocate space in the buffer,
        // and build the record specification array
        analyzeColumns(storeTable, columnNames);
        // create the NdbRecord
        NdbRecord result = ndbDictionary.createRecord(tableConst, recordSpecificationArray,
                projectedColumnNames.length, SIZEOF_RECORD_SPECIFICATION, 0);
        handleError(result, ndbDictionary);
        int rowLength = NdbDictionary.getRecordRowLength(result);
        // create the buffer pool now that the size of the record is known
        if (this.bufferSize < rowLength) {
            logger.warn("NdbRecordImpl.createNdbRecord rowLength for " + this.name + " is " + rowLength +
                    " but we only allocate length of " + this.bufferSize);
            this.bufferSize = rowLength;
        }
        bufferPool = new FixedByteBufferPoolImpl(this.bufferSize, this.name);
        return result;
    }

    /** Construct the list of storeColumns from the full list of columns in the table,
     * but only allocate space in the buffer for the columns that are used in the projection.
     */
    private void analyzeColumns(Table storeTable, String[] columnNames) {
        List<Column> align8 = new ArrayList<Column>();
        List<Column> align4 = new ArrayList<Column>();
        List<Column> align2 = new ArrayList<Column>();
        List<Column> align1 = new ArrayList<Column>();
        List<Column> nullables = new ArrayList<Column>();
        int columnIndex = 0;
        int projectionIndex = 0;
        for (String columnName: columnNames) {
            Column storeColumn = storeTable.getColumn(columnName);
            int columnId = storeColumn.getColumnId();
            recordSpecificationIndexes[columnId] = projectionIndex;
            lengths[columnIndex] = storeColumn.getLength();
            storeColumns[columnIndex] = storeColumn;
            ++columnIndex;
            // if this column is not in the projection, skip the rest of this processing
            if (!(projectedColumnSet.contains(columnName))) { continue; }
            ++projectionIndex;
            // for each projected column, put into alignment bucket
            switch (storeColumn.getType()) {
                case Bigint:
                case Bigunsigned:
                case Bit:
                case Blob:
                case Date:
                case Datetime:
                case Datetime2:
                case Double:
                case Text:
                case Time:
                case Time2:
                case Timestamp:
                case Timestamp2:
                    align8.add(storeColumn);
                    break;
                case Binary:
                case Char:
                case Decimal:
                case Decimalunsigned:
                case Longvarbinary:
                case Longvarchar:
                case Olddecimal:
                case Olddecimalunsigned:
                case Tinyint:
                case Tinyunsigned:
                case Varbinary:
                case Varchar:
                    align1.add(storeColumn);
                    break;
                case Float:
                case Int:
                case Mediumint:
                case Mediumunsigned:
                case Unsigned:
                    align4.add(storeColumn);
                    break;
                case Smallint:
                case Smallunsigned:
                case Year:
                    align2.add(storeColumn);
                    break;
                case Undefined:
                    throw new ClusterJFatalUserException(local.message("ERR_Unknown_Column_Type",
                            storeTable.getName(), columnName, storeColumn.getType()));
                default:
                    throw new ClusterJFatalInternalException(local.message("ERR_Unknown_Column_Type",
                            storeTable.getName(), columnName, storeColumn.getType()));
            }
            if (storeColumn.getNullable()) {
                nullables.add(storeColumn);
            }
        }
        // for each column, allocate space in the buffer, starting with align8 and ending with align1
        // null indicators are allocated first, with one bit per nullable column
        // nullable columns take one bit each
        offset = nullables.size() + 7 / 8;
        // align the first column following the nullable column indicators to 8
        offset = (7 + offset) / 8 * 8;
        nullIndicatorSize = offset;
        for (Column storeColumn: align8) {
            analyzeColumn(8, storeColumn);
        }
        for (Column storeColumn: align4) {
            analyzeColumn(4, storeColumn);
        }
        for (Column storeColumn: align2) {
            analyzeColumn(2, storeColumn);
        }
        for (Column storeColumn: align1) {
            analyzeColumn(1, storeColumn);
        }
        bufferSize = offset;
        if (logger.isDebugEnabled()) logger.debug(dumpDefinition());
    }

    /** Create a record specification for a column. Keep track of the offset into the buffer
     * and the null indicator position for each column.
     * 
     * @param alignment the alignment for this column in the buffer
     * @param storeColumn the column
     */
    private void analyzeColumn(int alignment, Column storeColumn) {
        int columnId = storeColumn.getColumnId();
        int recordSpecificationIndex = recordSpecificationIndexes[columnId];
        RecordSpecification recordSpecification = recordSpecificationArray.at(recordSpecificationIndex);
        ColumnConst columnConst = tableConst.getColumn(columnId);
        recordSpecification.column(columnConst);
        recordSpecification.offset(offset);
        offsets[columnId] = offset;
        int columnSpace = storeColumn.getColumnSpace();
        offset += ((columnSpace==0)?alignment:columnSpace);
        if (storeColumn.getNullable()) {
            int nullbitByteOffsetValue = nullablePosition/8;
            int nullbitBitInByteValue = nullablePosition - nullablePosition / 8 * 8;
            nullbitBitInByte[columnId] = nullbitBitInByteValue;
            nullbitByteOffset[columnId] = nullbitByteOffsetValue;
            recordSpecification.nullbit_byte_offset(nullbitByteOffsetValue);
            recordSpecification.nullbit_bit_in_byte(nullbitBitInByteValue);
            ++nullablePosition;
        } else {
            recordSpecification.nullbit_byte_offset(0);
            recordSpecification.nullbit_bit_in_byte(0);
        }
    }

    private String dumpDefinition() {
        StringBuilder builder = new StringBuilder(name);
        builder.append(" numberOfColumns: ");
        builder.append(projectedColumnSet.size());
        builder.append('\n');
        for (int columnId = 0; columnId < numberOfTableColumns; ++columnId) {
            Column storeColumn = storeColumns[columnId];
            if (storeColumn != null && projectedColumnSet.contains(storeColumn.getName())) {
                builder.append(" column: ");
                builder.append(storeColumn.getName());
                builder.append(" offset: ");
                builder.append(offsets[columnId]);
                builder.append(" length: ");
                builder.append(lengths[columnId]);
                builder.append(" nullbitBitInByte: ");
                builder.append(nullbitBitInByte[columnId]);
                builder.append(" nullbitByteOffset: ");
                builder.append(nullbitByteOffset[columnId]);
                builder.append('\n');
            }
        }
        return builder.toString();
    }

    public String dumpValues(ByteBuffer data, byte[] mask) {
        StringBuilder builder = new StringBuilder("table name: ");
        builder.append(name);
        builder.append(" numberOfColumns: ");
        builder.append(numberOfTableColumns);
        builder.append('\n');
        for (int columnId = 0; columnId < numberOfTableColumns; ++columnId) {
            Column storeColumn = storeColumns[columnId];
            if (storeColumn != null && projectedColumnSet.contains(storeColumn.getName())) {
                builder.append(" column: ");
                builder.append(storeColumn.getName());
                builder.append(" offset: ");
                builder.append(offsets[columnId]);
                builder.append(" length: ");
                builder.append(lengths[columnId]);
                builder.append(" nullbitBitInByte: ");
                int nullBitInByte = nullbitBitInByte[columnId];
                builder.append(nullBitInByte);
                builder.append(" nullbitByteOffset: ");
                int nullByteOffset = nullbitByteOffset[columnId];
                builder.append(nullByteOffset);
                builder.append(" data: ");
                int size = storeColumn.getColumnSpace() != 0 ? storeColumn.getColumnSpace():storeColumn.getSize();
                int offset = offsets[columnId];
                data.limit(bufferSize);
                data.position(0);
                for (int index = offset; index < offset + size; ++index) {
                    builder.append(String.format("%2x ", data.get(index)));
                }
                builder.append(" null: ");
                builder.append(isNull(data, columnId));
                if (mask != null) {
                    builder.append(" present: ");
                    builder.append(isPresent(mask, columnId));
                }
                builder.append('\n');
            }
        }
        data.position(0);
        return builder.toString();
    }

    TableConst getNdbTable(String tableName) {
        TableConst ndbTable = ndbDictionary.getTable(tableName);
        if (ndbTable == null) {
            // try the lower case table name
            ndbTable = ndbDictionary.getTable(tableName.toLowerCase());
        }
        if (ndbTable == null) {
            Utility.throwError(ndbTable, ndbDictionary.getNdbError(), tableName);
        }
        return ndbTable;
    }

    TableConst getNdbTable() {
        assertValid();
        return tableConst;
    }

    IndexConst getNdbIndex(String indexName, String tableName) {
        IndexConst ndbIndex = ndbDictionary.getIndex(indexName, tableName);
        if (ndbIndex == null) {
            Utility.throwError(ndbIndex, ndbDictionary.getNdbError(),  tableName+ "+" + indexName);
        }
        return ndbIndex;
    }

    IndexConst getNdbIndex() {
        assertValid();
        return indexConst;
    }

    public int getBufferSize() {
        return bufferSize;
    }

    public NdbRecordConst getNdbRecord() {
        assertValid();
        return ndbRecord;
    }

    public int getNumberOfColumns() {
        return numberOfTableColumns;
    }

    protected void releaseNdbRecord() {
        if (ndbRecord != null) {
            if (logger.isDebugEnabled())logger.debug("Releasing NdbRecord for " + tableConst.getName());
            ndbDictionary.releaseRecord(ndbRecord);
            ndbRecord = null;
            // release the buffer pool; pooled byte buffers will be garbage collected
            this.bufferPool = null;
        }
    }

    protected void assertValid() {
        if (ndbRecord == null) {
            throw new ClusterJUserException(local.message("ERR_NdbRecord_was_released"));
        }
    }
    public int getNullIndicatorSize() {
        return nullIndicatorSize;
    }

    public boolean isLob(int columnId) {
        return storeColumns[columnId].isLob();
    }

    public void setAutoIncrementValue(ByteBuffer valueBuffer, long value) {
        autoIncrementValueSetter.set(valueBuffer, value);
    }

    /** Choose the appropriate autoincrement value setter based on the column type.
     * This is done once during construction when the autoincrement column is known.
     */
    private void chooseAutoIncrementValueSetter() {
        switch (autoIncrementColumn.getType()) {
            case Int:
            case Unsigned:
                if (logger.isDebugEnabled())
                    logger.debug("chooseAutoIncrementValueSetter autoIncrementValueSetterInt.");
                autoIncrementValueSetter = autoIncrementValueSetterInt;
                break;
            case Bigint:
            case Bigunsigned:
                if (logger.isDebugEnabled())
                    logger.debug("chooseAutoIncrementValueSetter autoIncrementValueSetterBigint.");
                autoIncrementValueSetter = autoIncrementValueSetterLong;
                break;
            case Mediumint:
                if (logger.isDebugEnabled())
                    logger.debug("chooseAutoIncrementValueSetter autoIncrementValueSetterMediumint.");
                autoIncrementValueSetter = autoIncrementValueSetterMediumInt;
                break;
            case Mediumunsigned:
                if (logger.isDebugEnabled())
                    logger.debug("chooseAutoIncrementValueSetter autoIncrementValueSetterMediumunsigned.");
                autoIncrementValueSetter = autoIncrementValueSetterMediumUnsigned;
                break;
            case Smallint:
            case Smallunsigned:
                if (logger.isDebugEnabled())
                    logger.debug("chooseAutoIncrementValueSetter autoIncrementValueSetterSmallint.");
                autoIncrementValueSetter = autoIncrementValueSetterShort;
                break;
            case Tinyint:
            case Tinyunsigned:
                if (logger.isDebugEnabled())
                    logger.debug("chooseAutoIncrementValueSetter autoIncrementValueSetterTinyint.");
                autoIncrementValueSetter = autoIncrementValueSetterByte;
                break;
            default: 
                logger.error("chooseAutoIncrementValueSetter undefined.");
                autoIncrementValueSetter = autoIncrementValueSetterError;
                throw new ClusterJFatalInternalException(local.message("ERR_Unsupported_AutoIncrement_Column_Type",
                        autoIncrementColumn.getType(), autoIncrementColumn.getName(), tableConst.getName()));
        }
    }

    protected interface AutoIncrementValueSetter {
        void set(ByteBuffer valueBuffer, long value);
    }

    protected AutoIncrementValueSetter autoIncrementValueSetterError = new AutoIncrementValueSetter() {
        public void set(ByteBuffer valueBuffer, long value) {
            throw new ClusterJFatalInternalException(local.message("ERR_No_AutoIncrement_Column",
                    tableConst.getName()));
        }
    };

    protected AutoIncrementValueSetter autoIncrementValueSetterInt = new AutoIncrementValueSetter() {
        public void set(ByteBuffer valueBuffer, long value) {
            if (logger.isDetailEnabled()) logger.detail("autoincrement set value: " + value);
            if (value < 0 || value > Integer.MAX_VALUE) {
                throw new ClusterJDatastoreException(local.message("ERR_AutoIncrement_Value_Out_Of_Range",
                        value, autoIncrementColumn.getName(), tableConst.getName()));
            }
            setInt(valueBuffer, autoIncrementColumn, (int)value);
        }
    };

    protected AutoIncrementValueSetter autoIncrementValueSetterLong = new AutoIncrementValueSetter() {
        public void set(ByteBuffer valueBuffer, long value) {
            if (logger.isDetailEnabled()) logger.detail("autoincrement set value: " + value);
            if (value < 0) {
                throw new ClusterJDatastoreException(local.message("ERR_AutoIncrement_Value_Out_Of_Range",
                        value, autoIncrementColumn.getName(), tableConst.getName()));
            }
            setLong(valueBuffer, autoIncrementColumn, value);
        }
    };

    protected AutoIncrementValueSetter autoIncrementValueSetterMediumInt = new AutoIncrementValueSetter() {
        public void set(ByteBuffer valueBuffer, long value) {
            if (logger.isDetailEnabled()) logger.detail("autoincrement set value: " + value);
            if (value < Utility.MIN_MEDIUMINT_VALUE || value > Utility.MAX_MEDIUMINT_VALUE) {
                throw new ClusterJDatastoreException(local.message("ERR_AutoIncrement_Value_Out_Of_Range",
                        value, autoIncrementColumn.getName(), tableConst.getName()));
            }
            setInt(valueBuffer, autoIncrementColumn, (int)value);
        }
    };

    protected AutoIncrementValueSetter autoIncrementValueSetterMediumUnsigned = new AutoIncrementValueSetter() {
        public void set(ByteBuffer valueBuffer, long value) {
            if (logger.isDetailEnabled()) logger.detail("autoincrement set value: " + value);
            if (value < 0 || value > Utility.MAX_MEDIUMUNSIGNED_VALUE) {
                throw new ClusterJDatastoreException(local.message("ERR_AutoIncrement_Value_Out_Of_Range",
                        value, autoIncrementColumn.getName(), tableConst.getName()));
            }
            setInt(valueBuffer, autoIncrementColumn, (int)value);
        }
    };

    protected AutoIncrementValueSetter autoIncrementValueSetterShort = new AutoIncrementValueSetter() {
        public void set(ByteBuffer valueBuffer, long value) {
            if (logger.isDetailEnabled()) logger.detail("autoincrement set value: " + value);
            if (value < 0 || value > Short.MAX_VALUE) {
                throw new ClusterJDatastoreException(local.message("ERR_AutoIncrement_Value_Out_Of_Range",
                        value, autoIncrementColumn.getName(), tableConst.getName()));
            }
            setShort(valueBuffer, autoIncrementColumn, (short)value);
        }
    };

    protected AutoIncrementValueSetter autoIncrementValueSetterByte = new AutoIncrementValueSetter() {
        public void set(ByteBuffer valueBuffer, long value) {
            if (logger.isDetailEnabled()) logger.detail("autoincrement set value: " + value);
            if (value < 0 || value > Byte.MAX_VALUE) {
                throw new ClusterJDatastoreException(local.message("ERR_AutoIncrement_Value_Out_Of_Range",
                        value, autoIncrementColumn.getName(), tableConst.getName()));
            }
            setByte(valueBuffer, autoIncrementColumn, (byte)value);
        }
    };
}
