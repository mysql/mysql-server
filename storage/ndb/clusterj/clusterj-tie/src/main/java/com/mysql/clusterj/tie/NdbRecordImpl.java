/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.tie;

import java.math.BigDecimal;
import java.math.BigInteger;

import java.nio.ByteBuffer;

import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJFatalUserException;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Table;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.clusterj.tie.DbImpl.BufferManager;

import com.mysql.ndbjtie.ndbapi.NdbRecord;
import com.mysql.ndbjtie.ndbapi.NdbRecordConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.ColumnConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.RecordSpecification;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.RecordSpecificationArray;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;

/**
 * Wrapper around an NdbRecord. The default implementation can be used for insert,
 * using an NdbRecord that defines every column in the table. After construction, the instance is
 * read-only and can be shared among all threads that use the same cluster connection; and the size of the
 * buffer required for operations is available. The NdbRecord instance is released when the cluster
 * connection is closed. Column values can be set using a provided
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
    protected final static int SIZEOF_RECORD_SPECIFICATION = ClusterConnectionServiceImpl.SIZEOF_RECORD_SPECIFICATION;

    /** The NdbRecord for this operation */
    private NdbRecord ndbRecord = null;

    /** The store columns for this operation */
    protected List<Column> storeColumns = new ArrayList<Column>();

    /** The RecordSpecificationArray used to define the columns in the NdbRecord */
    private RecordSpecificationArray recordSpecificationArray;

    /** The NdbTable */
    TableConst tableConst;

    /** The size of the receive buffer for this operation (may be zero for non-read operations) */
    protected int bufferSize;

    /** The maximum column id for this operation (may be zero for non-read operations) */
    protected int maximumColumnId;

    /** The offsets into the buffer for each column (may be null for non-read operations) */
    protected int[] offsets;

    /** Values for setting column mask and null bit mask */
    protected final static byte[] BIT_IN_BYTE_MASK = new byte[] {1, 2, 4, 8, 16, 32, 64, -128};

    /** The position in the null indicator for the field */
    protected int nullablePositions[] = null;

    /** The null indicator for the field bit in the byte */
    protected int nullbitBitInByte[] = null;

    /** The null indicator for the field byte offset*/
    protected int nullbitByteOffset[] = null;

    /** The maximum length of any column in this operation */
    protected int maximumColumnLength;

    /** The dictionary used to create (and release) the NdbRecord */
    private Dictionary ndbDictionary;

    /** Number of columns for this NdbRecord */
    private int numberOfColumns;

    /** These fields are only used during construction of the RecordSpecificationArray */
    int offset = 0;
    int nullablePosition = 0;

    /** Constructor used for insert operations that do not need to read data.
     * 
     * @param storeTable the store table
     * @param ndbDictionary the ndb dictionary
     */
    protected NdbRecordImpl(Table storeTable, Dictionary ndbDictionary) {
        this.ndbDictionary = ndbDictionary;
        this.tableConst = getNdbTable(storeTable.getName());
        this.numberOfColumns = tableConst.getNoOfColumns();
        this.recordSpecificationArray = RecordSpecificationArray.create(numberOfColumns);
        this.offsets = new int[numberOfColumns];
        this.nullablePositions = new int[numberOfColumns];
        this.nullbitBitInByte = new int[numberOfColumns];
        this.nullbitByteOffset = new int[numberOfColumns];
        this.ndbRecord = createNdbRecord(storeTable, ndbDictionary);
    }


    public int setBigInteger(ByteBuffer buffer, Column storeColumn, BigInteger value) {
        int columnId = storeColumn.getColumnId();
        int newPosition = offsets[columnId];
        buffer.position(newPosition);
        ByteBuffer bigIntegerBuffer = Utility.convertValue(storeColumn, value);
        buffer.put(bigIntegerBuffer);
        return columnId;
    }

    public int setByte(ByteBuffer buffer, Column storeColumn, byte value) {
        int columnId = storeColumn.getColumnId();
        buffer.put(offsets[columnId], value);
        return columnId;
    }

    public int setBytes(ByteBuffer buffer, Column storeColumn, byte[] value) {
        int columnId = storeColumn.getColumnId();
        int newPosition = offsets[columnId];
        buffer.position(newPosition);
        Utility.convertValue(buffer, storeColumn, value);
        return columnId;
    }

    public int setDecimal(ByteBuffer buffer, Column storeColumn, BigDecimal value) {
        int columnId = storeColumn.getColumnId();
        int newPosition = offsets[columnId];
        buffer.position(newPosition);
        ByteBuffer decimalBuffer = Utility.convertValue(storeColumn, value);
        buffer.put(decimalBuffer);
        return columnId;
    }

    public int setDouble(ByteBuffer buffer, Column storeColumn, Double value) {
        int columnId = storeColumn.getColumnId();
        buffer.putDouble(offsets[columnId], value);
        return columnId;
    }

    public int setFloat(ByteBuffer buffer, Column storeColumn, Float value) {
        int columnId = storeColumn.getColumnId();
        buffer.putFloat(offsets[columnId], value);
        return columnId;
    }

    public int setInt(ByteBuffer buffer, Column storeColumn, Integer value) {
        int columnId = storeColumn.getColumnId();
        buffer.putInt(offsets[columnId], value);
        return columnId;
    }

    public int setLong(ByteBuffer buffer, Column storeColumn, long value) {
        int columnId = storeColumn.getColumnId();
        long storeValue = Utility.convertLongValueForStorage(storeColumn, value);
        buffer.putLong(offsets[columnId], storeValue);
        return columnId;
    }

    public int setNull(ByteBuffer buffer, Column storeColumn) {
        int columnId = storeColumn.getColumnId();
        int index = nullbitByteOffset[columnId];
        byte mask = BIT_IN_BYTE_MASK[nullbitBitInByte[columnId]];
        byte nullbyte = buffer.get(index);
        buffer.put(index, (byte)(nullbyte|mask));
        return columnId;
    }

    public int setShort(ByteBuffer buffer, Column storeColumn, Short value) {
        int columnId = storeColumn.getColumnId();
        buffer.putShort(offsets[columnId], value);
        return columnId;
    }

    public int setString(ByteBuffer buffer, BufferManager bufferManager, Column storeColumn, String value) {
        int columnId = storeColumn.getColumnId();
        buffer.position(offsets[columnId]);
        // for now, use the encode method to encode the value then copy it
        ByteBuffer converted = Utility.encode(value, storeColumn, bufferManager);
        buffer.put(converted);
        return columnId;
    }

    protected static void handleError(Object object, Dictionary ndbDictionary) {
        if (object != null) {
            return;
        } else {
            Utility.throwError(null, ndbDictionary.getNdbError());
        }
    }

    protected NdbRecord createNdbRecord(Table storeTable, Dictionary ndbDictionary) {
        String[] columnNames = storeTable.getColumnNames();
        List<Column> align8 = new ArrayList<Column>();
        List<Column> align4 = new ArrayList<Column>();
        List<Column> align2 = new ArrayList<Column>();
        List<Column> align1 = new ArrayList<Column>();
        List<Column> nullables = new ArrayList<Column>();
        for (String columnName: columnNames) {
            Column storeColumn = storeTable.getColumn(columnName);
            storeColumns.add(storeColumn);
            // for each column, put into alignment bucket
            switch (storeColumn.getType()) {
                case Bigint:
                case Bigunsigned:
                case Bit:
                case Blob:
                case Date:
                case Datetime:
                case Double:
                case Text:
                case Time:
                case Timestamp:
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
        for (Column storeColumn: align8) {
            handleColumn(8, storeColumn);
        }
        for (Column storeColumn: align4) {
            handleColumn(4, storeColumn);
        }
        for (Column storeColumn: align2) {
            handleColumn(2, storeColumn);
        }
        for (Column storeColumn: align1) {
            handleColumn(1, storeColumn);
        }
        bufferSize = offset;

        // now create an NdbRecord
        NdbRecord result = ndbDictionary.createRecord(tableConst, recordSpecificationArray,
                numberOfColumns, SIZEOF_RECORD_SPECIFICATION, 0);
        // delete the RecordSpecificationArray since it is no longer needed
        RecordSpecificationArray.delete(recordSpecificationArray);
        handleError(result, ndbDictionary);
        return result;
    }

    /** Create a record specification for a column. Keep track of the offset into the buffer
     * and the null indicator position for each column.
     * 
     * @param alignment the alignment for this column in the buffer
     * @param storeColumn the column
     */
    private void handleColumn(int alignment, Column storeColumn) {
        int columnId = storeColumn.getColumnId();
        RecordSpecification recordSpecification = recordSpecificationArray.at(columnId);
        ColumnConst columnConst = tableConst.getColumn(columnId);
        recordSpecification.column(columnConst);
        recordSpecification.offset(offset);
        offsets[columnId] = offset;
        int columnSpace = storeColumn.getColumnSpace();
        offset += (columnSpace==0)?8:columnSpace;
        if (storeColumn.getNullable()) {
            nullablePositions[columnId] = nullablePosition++;
            int nullbitByteOffsetValue = nullablePosition/8;
            int nullbitBitInByteValue = nullablePosition - nullablePosition / 8 * 8;
            nullbitBitInByte[columnId] = nullbitBitInByteValue;
            nullbitByteOffset[columnId] = nullbitByteOffsetValue;
            recordSpecification.nullbit_byte_offset(nullbitByteOffsetValue);
            recordSpecification.nullbit_bit_in_byte(nullbitBitInByteValue);
        } else {
            recordSpecification.nullbit_byte_offset(0);
            recordSpecification.nullbit_bit_in_byte(0);
        }
        if (logger.isDetailEnabled()) logger.detail(
                "column: " + storeColumn.getName()
                + " columnSpace: " + columnSpace 
                + " offset: " + offsets[columnId]
                + " nullable position: " + nullablePositions[columnId]
                + " nullbitByteOffset: " + nullbitByteOffset[columnId]
                + " nullbitBitInByte: " +  nullbitBitInByte[columnId]);
    }

    TableConst getNdbTable(String tableName) {
        TableConst ndbTable = ndbDictionary.getTable(tableName);
        if (ndbTable == null) {
            // try the lower case table name
            ndbTable = ndbDictionary.getTable(tableName.toLowerCase());
        }
        return ndbTable;
    }

    public int getBufferSize() {
        return bufferSize;
    }

    public NdbRecordConst getNdbRecord() {
        return ndbRecord;
    }

    public int getNumberOfColumns() {
        return numberOfColumns;
    }

    protected void releaseNdbRecord() {
        if (ndbRecord != null) {
            ndbDictionary.releaseRecord(ndbRecord);
            ndbRecord = null;
        }
    }

}
