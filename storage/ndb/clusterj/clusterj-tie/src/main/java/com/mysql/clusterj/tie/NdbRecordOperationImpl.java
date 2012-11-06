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

import com.mysql.clusterj.core.store.Blob;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Db;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.clusterj.tie.DbImpl.BufferManager;

import com.mysql.ndbjtie.ndbapi.NdbBlob;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;

/**
 * Implementation of store operation that uses NdbRecord.
 * Operations of the "equal" variety delegate to the key NdbRecordImpl.
 * Operations of the "set" and "get" varieties delegate to the value NdbRecordImpl.
 */
public class NdbRecordOperationImpl implements Operation {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(NdbRecordOperationImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(NdbRecordOperationImpl.class);

    /** The ClusterTransaction that this operation belongs to */
    protected ClusterTransactionImpl clusterTransaction = null;

    /** The NdbOperation wrapped by this object */
    protected NdbOperationConst ndbOperation = null;

    /** The NdbRecord for keys */
    protected NdbRecordImpl ndbRecordKeys = null;

    /** The NdbRecord for values */
    protected NdbRecordImpl ndbRecordValues = null;

    /** The mask for this operation, which contains a bit set for each column referenced.
     * For insert, this contains a bit for each column to be inserted.
     * For update, this contains a bit for each column to be updated.
     * For read/scan operations, this contains a bit for each column to be read.
     */
    byte[] mask;

    /** The ByteBuffer containing keys */
    ByteBuffer keyBuffer = null;

    /** The ByteBuffer containing values */
    ByteBuffer valueBuffer = null;

    /** Blobs for this NdbRecord */
    protected NdbRecordBlobImpl[] blobs = null;

    /** Blobs that have been accessed for this operation */
    protected List<NdbRecordBlobImpl> activeBlobs = new ArrayList<NdbRecordBlobImpl>();

    /** The size of the key buffer for this operation */
    protected int keyBufferSize;

    /** The size of the value buffer for this operation */
    protected int valueBufferSize;

    /** The buffer manager for string encode and decode */
    protected BufferManager bufferManager;

    /** The table name */
    protected String tableName;

    /** The store table */
    protected Table storeTable;

    /** The store columns. */
    protected Column[] storeColumns;

    /** The number of columns */
    int numberOfColumns;

    /** The db for this operation */
    protected DbImpl db;

    /** Constructor used for smart value handler for new instances,
     * and the cluster transaction is not yet known. There is only one
     * NdbRecord and one buffer, so all operations result in using
     * the same buffer.
     * 
     * @param clusterConnection the cluster connection
     * @param db the Db
     * @param storeTable the store table
     */
    public NdbRecordOperationImpl(ClusterConnectionImpl clusterConnection, Db db, Table storeTable) {
        this.db = (DbImpl)db;
        this.storeTable = storeTable;
        this.tableName = storeTable.getName();
        this.ndbRecordValues = clusterConnection.getCachedNdbRecordImpl(storeTable);
        this.ndbRecordKeys = ndbRecordValues;
        this.valueBufferSize = ndbRecordValues.getBufferSize();
        this.keyBufferSize = ndbRecordKeys.getBufferSize();
        this.valueBuffer = ndbRecordValues.newBuffer();
        this.keyBuffer = valueBuffer;
        this.storeColumns = ndbRecordValues.storeColumns;
        this.numberOfColumns = storeColumns.length;
        this.blobs = new NdbRecordBlobImpl[this.numberOfColumns];
        this.bufferManager = ((DbImpl)db).getBufferManager();
        resetMask();
    }

    /** Constructor used when the transaction is known.
     * 
     * @param clusterTransaction the cluster transaction
     */
    public NdbRecordOperationImpl(ClusterTransactionImpl clusterTransaction, Table storeTable) {
        this.clusterTransaction = clusterTransaction;
        this.db = clusterTransaction.db;
        this.bufferManager = clusterTransaction.getBufferManager();
        this.tableName = storeTable.getName();
        this.ndbRecordValues = clusterTransaction.getCachedNdbRecordImpl(storeTable);
        this.valueBufferSize = ndbRecordValues.getBufferSize();
        this.valueBuffer = ndbRecordValues.newBuffer();
        this.numberOfColumns = ndbRecordValues.getNumberOfColumns();
        this.blobs = new NdbRecordBlobImpl[this.numberOfColumns];
        resetMask();
    }

    /** Constructor used to copy an existing NdbRecordOperationImpl for use with a SmartValueHandler.
     * The value buffer is copied and cannot be used by the existing NdbRecordOperationImpl.
     * 
     * @param ndbRecordOperationImpl2 the existing NdbRecordOperationImpl with value buffer
     */
    public NdbRecordOperationImpl(NdbRecordOperationImpl ndbRecordOperationImpl2) {
        this.ndbRecordValues = ndbRecordOperationImpl2.ndbRecordValues;
        this.valueBufferSize = ndbRecordOperationImpl2.valueBufferSize;
        this.ndbRecordKeys = ndbRecordValues;
        this.keyBufferSize = ndbRecordKeys.bufferSize;
        this.valueBuffer = ndbRecordOperationImpl2.valueBuffer;
        this.keyBuffer = this.valueBuffer;
        this.bufferManager = ndbRecordOperationImpl2.bufferManager;
        this.tableName = ndbRecordOperationImpl2.tableName;
        this.storeColumns = ndbRecordOperationImpl2.ndbRecordValues.storeColumns;
        this.numberOfColumns = this.storeColumns.length;
        this.blobs = new NdbRecordBlobImpl[this.numberOfColumns];
        this.activeBlobs = ndbRecordOperationImpl2.activeBlobs;
        resetMask();
    }

    public NdbOperationConst insert(ClusterTransactionImpl clusterTransactionImpl) {
        // position the buffer at the beginning for ndbjtie
        valueBuffer.limit(valueBufferSize);
        valueBuffer.position(0);
        ndbOperation = clusterTransactionImpl.insertTuple(ndbRecordValues.getNdbRecord(), valueBuffer, mask, null);
        clusterTransactionImpl.addOperationToCheck(this);
        // for each blob column set, get the blob handle and write the values
        for (NdbRecordBlobImpl blob: activeBlobs) {
            // activate the blob by getting the NdbBlob
            blob.setNdbBlob();
            // set values into the blob
            blob.setValue();
        }
        return ndbOperation;
    }

    public NdbOperationConst delete(ClusterTransactionImpl clusterTransactionImpl) {
        // position the buffer at the beginning for ndbjtie
        keyBuffer.limit(keyBufferSize);
        keyBuffer.position(0);
        ndbOperation = clusterTransactionImpl.deleteTuple(ndbRecordKeys.getNdbRecord(), keyBuffer, mask, null);
        return ndbOperation;
    }

    public void update(ClusterTransactionImpl clusterTransactionImpl) {
        // position the buffer at the beginning for ndbjtie
        valueBuffer.limit(valueBufferSize);
        valueBuffer.position(0);
        ndbOperation = clusterTransactionImpl.updateTuple(ndbRecordValues.getNdbRecord(), valueBuffer, mask, null);
        clusterTransactionImpl.addOperationToCheck(this);
        // for each blob column set, get the blob handle and write the values
        for (NdbRecordBlobImpl blob: activeBlobs) {
            // activate the blob by getting the NdbBlob
            blob.setNdbBlob();
            // set values into the blob
            blob.setValue();
        }
        return;
    }

    public void write(ClusterTransactionImpl clusterTransactionImpl) {
        // position the buffer at the beginning for ndbjtie
        valueBuffer.limit(valueBufferSize);
        valueBuffer.position(0);
        ndbOperation = clusterTransactionImpl.writeTuple(ndbRecordValues.getNdbRecord(), valueBuffer, mask, null);
        clusterTransactionImpl.addOperationToCheck(this);
        // for each blob column set, get the blob handle and write the values
        for (NdbRecordBlobImpl blob: activeBlobs) {
            // activate the blob by getting the NdbBlob
            blob.setNdbBlob();
            // set values into the blob
            blob.setValue();
        }
        return;
    }

    public void load(ClusterTransactionImpl clusterTransactionImpl) {
        // position the buffer at the beginning for ndbjtie
        valueBuffer.limit(valueBufferSize);
        valueBuffer.position(0);
        ndbOperation = clusterTransactionImpl.readTuple(ndbRecordKeys.getNdbRecord(), keyBuffer, ndbRecordValues.getNdbRecord(), valueBuffer, mask, null);
        // for each blob column set, get the blob handle
        for (NdbRecordBlobImpl blob: activeBlobs) {
            // activate the blob by getting the NdbBlob
            blob.setNdbBlob();
        }
    }

    protected void resetMask() {
        this.mask = new byte[1 + (numberOfColumns/8)];
    }

    public void allocateValueBuffer() {
        this.valueBuffer = ndbRecordValues.newBuffer();
    }

    protected void activateBlobs() {
        for (NdbRecordBlobImpl blob: activeBlobs) {
            blob.setNdbBlob();
        }
    }

    public void equalBigInteger(Column storeColumn, BigInteger value) {
        int columnId = ndbRecordKeys.setBigInteger(keyBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void equalBoolean(Column storeColumn, boolean booleanValue) {
        byte value = (booleanValue?(byte)0x01:(byte)0x00);
        equalByte(storeColumn, value);
    }

    public void equalByte(Column storeColumn, byte value) {
        int columnId = ndbRecordKeys.setByte(keyBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void equalBytes(Column storeColumn, byte[] value) {
        int columnId = ndbRecordKeys.setBytes(keyBuffer, storeColumn, value);
        columnSet(columnId);
   }

    public void equalDecimal(Column storeColumn, BigDecimal value) {
        int columnId = ndbRecordKeys.setDecimal(keyBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void equalDouble(Column storeColumn, double value) {
        int columnId = ndbRecordKeys.setDouble(keyBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void equalFloat(Column storeColumn, float value) {
        int columnId = ndbRecordKeys.setFloat(keyBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void equalInt(Column storeColumn, int value) {
        int columnId = ndbRecordKeys.setInt(keyBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void equalLong(Column storeColumn, long value) {
        int columnId = ndbRecordKeys.setLong(keyBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void equalShort(Column storeColumn, short value) {
        int columnId = ndbRecordKeys.setShort(keyBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void equalString(Column storeColumn, String value) {
        int columnId = ndbRecordKeys.setString(keyBuffer, bufferManager, storeColumn, value);
        columnSet(columnId);
    }

    public void getBlob(Column storeColumn) {
        getBlobHandle(storeColumn);
    }

    /**
     * Get the blob handle for this column. The same column will return the same blob handle
     * regardless of how many times it is called.
     * @param storeColumn the store column
     * @return the blob handle
     */
    public Blob getBlobHandle(Column storeColumn) {
        int columnId = storeColumn.getColumnId();
        NdbRecordBlobImpl result = blobs[columnId];
        if (result == null) {
            columnSet(columnId);
            result = new NdbRecordBlobImpl(this, storeColumn);
            blobs[columnId] = result;
            activeBlobs.add(result);
        }
        return result;
    }

    /** Specify the columns to be used for the operation.
     */
    public void getValue(Column storeColumn) {
        int columnId = storeColumn.getColumnId();
        columnSet(columnId);
    }

    public void postExecuteCallback(Runnable callback) {
        clusterTransaction.postExecuteCallback(callback);
    }

    /** Construct a new ResultData using the saved column data and then execute the operation.
     */
    public ResultData resultData() {
        return resultData(true);
    }

    /** Construct a new ResultData and if requested, execute the operation.
     */
    public ResultData resultData(boolean execute) {
        NdbRecordResultDataImpl result =
            new NdbRecordResultDataImpl(this);
        if (execute) {
            clusterTransaction.executeNoCommit(false, true);
        }
        return result;
    }

    public void setBigInteger(Column storeColumn, BigInteger value) {
        if (value == null) {
            setNull(storeColumn);
        } else {
            int columnId = ndbRecordValues.setBigInteger(valueBuffer, storeColumn, value);
            columnSet(columnId);
        }
    }

    public void setBigInteger(int columnId, BigInteger value) {
        setBigInteger(storeColumns[columnId], value);
    }

    public void setBoolean(Column storeColumn, Boolean booleanValue) {
        byte value = (booleanValue?(byte)0x01:(byte)0x00);
        setByte(storeColumn, value);
    }

    public void setBoolean(int columnId, boolean value) {
        setBoolean(storeColumns[columnId], value);
    }

    public void setByte(Column storeColumn, byte value) {
        int columnId = ndbRecordValues.setByte(valueBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setByte(int columnId, byte value) {
        setByte(storeColumns[columnId], value);
    }

    public void setBytes(Column storeColumn, byte[] value) {
        if (value == null) {
            setNull(storeColumn);
        } else {
            int columnId = ndbRecordValues.setBytes(valueBuffer, storeColumn, value);
            columnSet(columnId);
        }
    }

    public void setBytes(int columnId, byte[] value) {
        setBytes(storeColumns[columnId], value);
    }

    public void setDecimal(Column storeColumn, BigDecimal value) {
        if (value == null) {
            setNull(storeColumn);
        } else {
            int columnId = ndbRecordValues.setDecimal(valueBuffer, storeColumn, value);
            columnSet(columnId);
        }
    }

    public void setDecimal(int columnId, BigDecimal value) {
        setDecimal(storeColumns[columnId], value);
    }

    public void setDouble(Column storeColumn, Double value) {
        int columnId = ndbRecordValues.setDouble(valueBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setDouble(int columnId, double value) {
        setDouble(storeColumns[columnId], value);
    }

    public void setFloat(Column storeColumn, Float value) {
        int columnId = ndbRecordValues.setFloat(valueBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setFloat(int columnId, float value) {
        setFloat(storeColumns[columnId], value);
    }

    public void setInt(Column storeColumn, Integer value) {
        int columnId = ndbRecordValues.setInt(valueBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setInt(int columnId, int value) {
        setInt(storeColumns[columnId], value);
    }

    public void setLong(Column storeColumn, long value) {
        int columnId = ndbRecordValues.setLong(valueBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setLong(int columnId, long value) {
        setLong(storeColumns[columnId], value);
    }

    public void setNull(Column storeColumn) {
        int columnId = ndbRecordValues.setNull(valueBuffer, storeColumn);
        columnSet(columnId);
    }

    public void setNull(int columnId) {
        setNull(storeColumns[columnId]);
    }

    public void setShort(Column storeColumn, Short value) {
        int columnId = ndbRecordValues.setShort(valueBuffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setShort(int columnId, short value) {
        setShort(storeColumns[columnId], value);
    }

    public void setObjectBoolean(int columnId, Boolean value) {
        Column storeColumn = storeColumns[columnId];
        if (value == null) {
            setNull(storeColumn);
        } else {
            setBoolean(storeColumn, value);
        }
    }

    public void setObjectByte(int columnId, Byte value) {
        Column storeColumn = storeColumns[columnId];
        if (value == null) {
            setNull(storeColumn);
        } else {
            setByte(storeColumn, value);
        }
    }

    public void setObjectDouble(int columnId, Double value) {
        Column storeColumn = storeColumns[columnId];
        if (value == null) {
            setNull(storeColumn);
        } else {
            setDouble(storeColumn, value);
        }
    }

    public void setObjectFloat(int columnId, Float value) {
        Column storeColumn = storeColumns[columnId];
        if (value == null) {
            setNull(storeColumn);
        } else {
            setFloat(storeColumn, value);
        }
    }

    public void setObjectInt(int columnId, Integer value) {
        Column storeColumn = storeColumns[columnId];
        if (value == null) {
            setNull(storeColumn);
        } else {
            setInt(storeColumn, value);
        }
    }

    public void setObjectLong(int columnId, Long value) {
        Column storeColumn = storeColumns[columnId];
        if (value == null) {
            setNull(storeColumn);
        } else {
            setLong(storeColumn, value);
        }
    }

    public void setObjectShort(int columnId, Short value) {
        Column storeColumn = storeColumns[columnId];
        if (value == null) {
            setNull(storeColumn);
        } else {
            setShort(storeColumn, value);
        }
    }

    public void setString(Column storeColumn, String value) {
        if (value == null) {
            setNull(storeColumn);
        } else {
            int columnId = ndbRecordValues.setString(valueBuffer, bufferManager, storeColumn, value);
            columnSet(columnId);
        }
    }

    public void setString(int columnId, String value) {
        setString(storeColumns[columnId], value);
    }

    public int errorCode() {
        return ndbOperation.getNdbError().code();
    }

    protected static void handleError(int returnCode, NdbOperationConst ndbOperation2) {
        if (returnCode == 0) {
            return;
        } else {
            Utility.throwError(returnCode, ndbOperation2.getNdbError());
        }
    }

    protected static void handleError(Object object, NdbOperationConst ndbOperation) {
        if (object != null) {
            return;
        } else {
            Utility.throwError(null, ndbOperation.getNdbError());
        }
    }

    protected static void handleError(Object object, Dictionary ndbDictionary) {
        if (object != null) {
            return;
        } else {
            Utility.throwError(null, ndbDictionary.getNdbError());
        }
    }

    public NdbBlob getNdbBlob(Column storeColumn) {
        NdbBlob result = ndbOperation.getBlobHandle(storeColumn.getColumnId());
        handleError(result, ndbOperation);
        return result;
    }

    /**
     * Set this column into the mask for NdbRecord operation.
     * @param columnId the column id
     */
    protected void columnSet(int columnId) {
        int byteOffset = columnId / 8;
        int bitInByte = columnId - (byteOffset * 8);
        mask[byteOffset] |= NdbRecordImpl.BIT_IN_BYTE_MASK[bitInByte];
        
    }

    public NdbRecordImpl getValueNdbRecord() {
        return ndbRecordValues;
    }

    public boolean getBoolean(int columnId) {
        return ndbRecordValues.getBoolean(valueBuffer, columnId);
    }

    public boolean getBoolean(Column storeColumn) {
        return ndbRecordValues.getBoolean(valueBuffer, storeColumn.getColumnId());
    }

    public boolean[] getBooleans(int column) {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented",
                "NdbRecordResultDataImpl.getBooleans(int)"));
    }

    public boolean[] getBooleans(Column storeColumn) {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented",
                "NdbRecordResultDataImpl.getBooleans(Column)"));
    }

    public byte getByte(int columnId) {
        return ndbRecordValues.getByte(valueBuffer, columnId);
    }

    public byte getByte(Column storeColumn) {
        return ndbRecordValues.getByte(valueBuffer, storeColumn.getColumnId());
    }

    public short getShort(int columnId) {
        return ndbRecordValues.getShort(valueBuffer, columnId);
    }

    public short getShort(Column storeColumn) {
        return ndbRecordValues.getShort(valueBuffer, storeColumn.getColumnId());
     }

    public int getInt(int columnId) {
        return ndbRecordValues.getInt(valueBuffer, columnId);
    }

    public int getInt(Column storeColumn) {
        return getInt(storeColumn.getColumnId());
    }

    public long getLong(int columnId) {
        return ndbRecordValues.getLong(valueBuffer, columnId);
    }

    public float getFloat(int columnId) {
        return ndbRecordValues.getFloat(valueBuffer, columnId);
    }

    public float getFloat(Column storeColumn) {
        return getFloat(storeColumn.getColumnId());
    }

    public double getDouble(int columnId) {
        return ndbRecordValues.getDouble(valueBuffer, columnId);
    }

    public double getDouble(Column storeColumn) {
        return getDouble(storeColumn.getColumnId());
    }

    public String getString(int columnId) {
        return ndbRecordValues.getString(valueBuffer, columnId, bufferManager);
    }

    public String getString(Column storeColumn) {
        return ndbRecordValues.getString(valueBuffer, storeColumn.getColumnId(), bufferManager);
    }

    public byte[] getBytes(int column) {
        return ndbRecordValues.getBytes(valueBuffer, column);
    }

    public byte[] getBytes(Column storeColumn) {
        return ndbRecordValues.getBytes(valueBuffer, storeColumn);
     }

    public Object getObject(int column) {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented",
        "NdbRecordResultDataImpl.getObject(int)"));
    }

    public Object getObject(Column storeColumn) {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented",
        "NdbRecordResultDataImpl.getObject(Column)"));
    }

    public boolean wasNull(Column storeColumn) {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented",
        "NdbRecordResultDataImpl.wasNull(Column)"));
    }

    public Boolean getObjectBoolean(int column) {
        return ndbRecordValues.getObjectBoolean(valueBuffer, column);
    }

    public Boolean getObjectBoolean(Column storeColumn) {
        return ndbRecordValues.getObjectBoolean(valueBuffer, storeColumn.getColumnId());
    }

    public Byte getObjectByte(int columnId) {
        return ndbRecordValues.getObjectByte(valueBuffer, columnId);
    }

    public Byte getObjectByte(Column storeColumn) {
        return ndbRecordValues.getObjectByte(valueBuffer, storeColumn.getColumnId());
    }

    public Float getObjectFloat(int column) {
        return ndbRecordValues.getObjectFloat(valueBuffer, column);
    }

    public Float getObjectFloat(Column storeColumn) {
        return ndbRecordValues.getObjectFloat(valueBuffer, storeColumn.getColumnId());
    }

    public Double getObjectDouble(int column) {
        return ndbRecordValues.getObjectDouble(valueBuffer, column);
    }

    public Double getObjectDouble(Column storeColumn) {
        return ndbRecordValues.getObjectDouble(valueBuffer, storeColumn.getColumnId());
    }

    public Integer getObjectInteger(int columnId) {
        return ndbRecordValues.getObjectInteger(valueBuffer, columnId);
    }

    public Integer getObjectInteger(Column storeColumn) {
        return ndbRecordValues.getObjectInteger(valueBuffer, storeColumn.getColumnId());
    }

    public Long getObjectLong(int column) {
        return ndbRecordValues.getObjectLong(valueBuffer, column);
    }

    public Long getObjectLong(Column storeColumn) {
        return ndbRecordValues.getObjectLong(valueBuffer, storeColumn.getColumnId());
    }

    public Short getObjectShort(int columnId) {
        return ndbRecordValues.getObjectShort(valueBuffer, columnId);
    }

    public Short getObjectShort(Column storeColumn) {
        return ndbRecordValues.getObjectShort(valueBuffer, storeColumn.getColumnId());
    }

    public BigInteger getBigInteger(int column) {
        return ndbRecordValues.getBigInteger(valueBuffer, column);
    }

    public BigInteger getBigInteger(Column storeColumn) {
        return ndbRecordValues.getBigInteger(valueBuffer, storeColumn);
    }

    public BigDecimal getDecimal(int column) {
        return ndbRecordValues.getDecimal(valueBuffer, column);
    }

    public BigDecimal getDecimal(Column storeColumn) {
        return ndbRecordValues.getDecimal(valueBuffer, storeColumn);
    }

    public void beginDefinition() {
        // by default, nothing to do
    }

    public void endDefinition() {
        // by default, nothing to do
    }

    public void freeResourcesAfterExecute() {
        // by default, nothing to do
    }

    public String dumpValues() {
        return ndbRecordValues.dumpValues(valueBuffer, mask);
    }

    public String dumpKeys() {
        return ndbRecordKeys.dumpValues(keyBuffer, null);
    }

    public boolean isModified(int columnId) {
        return ndbRecordValues.isPresent(mask, columnId);
    }

    public boolean isNull(int columnId) {
        return ndbRecordValues.isNull(valueBuffer, columnId);
    }

    public void markModified(int columnId) {
        ndbRecordValues.markPresent(mask, columnId);
    }

    public void resetModified() {
        this.mask = new byte[1 + (numberOfColumns/8)];
    }

    public NdbRecordBlobImpl getBlobHandle(int columnId) {
        return (NdbRecordBlobImpl) getBlobHandle(storeColumns[columnId]);
    }

    public int getErrorCode() {
        return ndbOperation.getNdbError().code();
    }

    public int getClassification() {
        return ndbOperation.getNdbError().classification();
    }

    public int getMysqlCode() {
        return ndbOperation.getNdbError().mysql_code();
    }

    public int getStatus() {
        return ndbOperation.getNdbError().status();
    }

    @Override
    public String toString() {
        return tableName;
    }

    /** After executing the operation, fetch the blob values into each blob's data holder.
     * 
     */
    public void loadBlobValues() {
        for (NdbRecordBlobImpl ndbRecordBlobImpl: activeBlobs) {
            ndbRecordBlobImpl.readData();
        }
    }

    /** Transform this NdbRecordOperationImpl into one that can be used to back a SmartValueHandler.
     * For instances that are used in primary key or unique key operations, the same instance is used.
     * Scans are handled by a subclass that overrides this method.
     * 
     * @return this NdbRecordOperationImpl
     */
    public NdbRecordOperationImpl transformNdbRecordOperationImpl() {
        this.keyBuffer = valueBuffer;
        resetModified();
        return this;
    }

}
