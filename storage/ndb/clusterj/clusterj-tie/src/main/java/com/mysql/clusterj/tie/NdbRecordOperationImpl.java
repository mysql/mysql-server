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
import java.nio.ByteOrder;

import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.Blob;
import com.mysql.clusterj.core.store.Column;
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
 */
class NdbRecordOperationImpl implements Operation {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(NdbRecordOperationImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(NdbRecordOperationImpl.class);

    /** The ClusterTransaction that this operation belongs to */
    protected ClusterTransactionImpl clusterTransaction;

    /** The NdbOperation wrapped by this object */
    private NdbOperationConst ndbOperation = null;

    /** The NdbRecord for this operation */
    private NdbRecordImpl ndbRecordImpl = null;

    /** The mask for this operation, which contains a bit set for each column to be inserted */
    byte[] mask;

    /** The ByteBuffer containing all of the data */
    ByteBuffer buffer = null;

    /** Blobs for this NdbRecord */
    protected NdbRecordBlobImpl[] blobs = null;

    /** Blobs that have been accessed for this operation */
    protected List<NdbRecordBlobImpl> activeBlobs = new ArrayList<NdbRecordBlobImpl>();

    /** The size of the receive buffer for this operation (may be zero for non-read operations) */
    protected int bufferSize;

    /** The number of columns for this operation */
    protected int numberOfColumns;

    protected BufferManager bufferManager;

    /** Constructor used for insert and delete operations that do not need to read data.
     * 
     * @param clusterTransaction the cluster transaction
     * @param transaction the ndb transaction
     * @param storeTable the store table
     */
    public NdbRecordOperationImpl(ClusterTransactionImpl clusterTransaction, Table storeTable) {
        this.ndbRecordImpl = clusterTransaction.getCachedNdbRecordImpl(storeTable);
        this.bufferSize = ndbRecordImpl.getBufferSize();
        this.numberOfColumns = ndbRecordImpl.getNumberOfColumns();
        this.blobs = new NdbRecordBlobImpl[this.numberOfColumns];
        this.clusterTransaction = clusterTransaction;
        this.bufferManager = clusterTransaction.getBufferManager();
    }

    public void equalBigInteger(Column storeColumn, BigInteger value) {
        setBigInteger(storeColumn, value);
    }

    public void equalBoolean(Column storeColumn, boolean booleanValue) {
        setBoolean(storeColumn, booleanValue);
    }

    public void equalByte(Column storeColumn, byte value) {
        setByte(storeColumn, value);
    }

    public void equalBytes(Column storeColumn, byte[] value) {
        setBytes(storeColumn, value);
   }

    public void equalDecimal(Column storeColumn, BigDecimal value) {
        setDecimal(storeColumn, value);
    }

    public void equalDouble(Column storeColumn, double value) {
        setDouble(storeColumn, value);
    }

    public void equalFloat(Column storeColumn, float value) {
        setFloat(storeColumn, value);
    }

    public void equalInt(Column storeColumn, int value) {
        setInt(storeColumn, value);
    }

    public void equalShort(Column storeColumn, short value) {
        setShort(storeColumn, value);
    }

    public void equalLong(Column storeColumn, long value) {
        setLong(storeColumn, value);
    }

    public void equalString(Column storeColumn, String value) {
        setString(storeColumn, value);
    }

    public void getBlob(Column storeColumn) {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented", "getBlob"));
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
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented", "getValue"));
    }

    public void postExecuteCallback(Runnable callback) {
        clusterTransaction.postExecuteCallback(callback);
    }

    /** Construct a new ResultData using the saved column data and then execute the operation.
     */
    public ResultData resultData() {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented", "resultData"));
    }

    /** Construct a new ResultData and if requested, execute the operation.
     */
    public ResultData resultData(boolean execute) {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented", "resultData"));
    }

    public void setBigInteger(Column storeColumn, BigInteger value) {
        int columnId = ndbRecordImpl.setBigInteger(buffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setBoolean(Column storeColumn, Boolean booleanValue) {
        byte value = (booleanValue?(byte)0x01:(byte)0x00);
        setByte(storeColumn, value);
    }

    public void setByte(Column storeColumn, byte value) {
        int columnId = ndbRecordImpl.setByte(buffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setBytes(Column storeColumn, byte[] value) {
        int columnId = ndbRecordImpl.setBytes(buffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setDecimal(Column storeColumn, BigDecimal value) {
        int columnId = ndbRecordImpl.setDecimal(buffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setDouble(Column storeColumn, Double value) {
        int columnId = ndbRecordImpl.setDouble(buffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setFloat(Column storeColumn, Float value) {
        int columnId = ndbRecordImpl.setFloat(buffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setInt(Column storeColumn, Integer value) {
        int columnId = ndbRecordImpl.setInt(buffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setLong(Column storeColumn, long value) {
        int columnId = ndbRecordImpl.setLong(buffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setNull(Column storeColumn) {
        int columnId = ndbRecordImpl.setNull(buffer, storeColumn);
        columnSet(columnId);
    }

    public void setShort(Column storeColumn, Short value) {
        int columnId = ndbRecordImpl.setShort(buffer, storeColumn, value);
        columnSet(columnId);
    }

    public void setString(Column storeColumn, String value) {
        int columnId = ndbRecordImpl.setString(buffer, bufferManager, storeColumn, value);
        columnSet(columnId);
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

    public void beginDefinition() {
        // allocate a buffer for the operation data
        buffer = ByteBuffer.allocateDirect(bufferSize);
        // use platform's native byte ordering
        buffer.order(ByteOrder.nativeOrder());
        mask = new byte[1 + (numberOfColumns/8)];
    }

    public void endDefinition() {
        // create the insert operation
        buffer.position(0);
        buffer.limit(bufferSize);
        // create the insert operation
        ndbOperation = clusterTransaction.insertTuple(ndbRecordImpl.getNdbRecord(), buffer, mask, null);
        // now set the NdbBlob into the blobs
        for (NdbRecordBlobImpl blob: activeBlobs) {
            if (blob != null) {
                blob.setNdbBlob();
            }
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
    private void columnSet(int columnId) {
        int byteOffset = columnId / 8;
        int bitInByte = columnId - (byteOffset * 8);
        mask[byteOffset] |= NdbRecordImpl.BIT_IN_BYTE_MASK[bitInByte];
        
    }

}
