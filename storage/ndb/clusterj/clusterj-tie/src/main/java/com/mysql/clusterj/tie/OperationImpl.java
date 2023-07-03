/*
   Copyright (c) 2009, 2023, Oracle and/or its affiliates.

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

package com.mysql.clusterj.tie;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.mysql.clusterj.ClusterJUserException;
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
import com.mysql.ndbjtie.ndbapi.NdbOperation;

/**
 *
 */
class OperationImpl implements Operation {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(OperationImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(OperationImpl.class);

    private NdbOperation ndbOperation;

    protected List<Column> storeColumns = new ArrayList<Column>();

    protected ClusterTransactionImpl clusterTransaction;

    protected VariableByteBufferPoolImpl byteBufferPool;

   /** The size of the receive buffer for this operation (may be zero for non-read operations) */
    protected int bufferSize;

    /** The maximum column id for this operation (may be zero for non-read operations) */
    protected int maximumColumnId;

    /** The offsets into the buffer for each column (may be null for non-read operations) */
    protected int[] offsets;

    /** The lengths of fields in the buffer for each column (may be null for non-read operations) */
    protected int[] lengths;

    /** The maximum length of any column in this operation */
    protected int maximumColumnLength;

    protected BufferManager bufferManager;

    /** Boolean flag that tracks whether this operation is a read operation */
    protected boolean isReadOp = false;

    /** Constructor used for insert and delete operations that do not need to read data.
     * 
     * @param operation the operation
     * @param transaction the transaction
     */
    public OperationImpl(NdbOperation operation, ClusterTransactionImpl transaction) {
        this.ndbOperation = operation;
        this.clusterTransaction = transaction;
        this.bufferManager = clusterTransaction.getBufferManager();
    }

    /** Constructor used for read operations. The table is used to obtain data used
     * to lay out memory for the result. 
     * @param storeTable the table
     * @param operation the operation
     * @param transaction the transaction
     */
    public OperationImpl(Table storeTable, NdbOperation operation, ClusterTransactionImpl transaction) {
        this(operation, transaction);
        TableImpl tableImpl = (TableImpl)storeTable;
        this.maximumColumnId = tableImpl.getMaximumColumnId();
        this.bufferSize = tableImpl.getBufferSize();
        this.offsets = tableImpl.getOffsets();
        this.lengths = tableImpl.getLengths();
        this.maximumColumnLength = tableImpl.getMaximumColumnLength();
        // mark this operation as a read and add this to operationsToCheck list
        this.isReadOp = true;
        transaction.addOperationToCheck(this);
    }

    public void equalBigInteger(Column storeColumn, BigInteger value) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void equalBoolean(Column storeColumn, boolean booleanValue) {
        byte value = (booleanValue?(byte)0x01:(byte)0x00);
        int returnCode = ndbOperation.equal(storeColumn.getName(), value);
        handleError(returnCode, ndbOperation);
    }

    public void equalByte(Column storeColumn, byte value) {
        int storeValue = Utility.convertByteValueForStorage(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), storeValue);
        handleError(returnCode, ndbOperation);
    }

    public void equalBytes(Column storeColumn, byte[] value) {
        if (logger.isDetailEnabled()) logger.detail("Column: " + storeColumn.getName() + " columnId: " + storeColumn.getColumnId() + " data length: " + value.length);
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), buffer);
        handleError(returnCode, ndbOperation);
   }

    public void equalDecimal(Column storeColumn, BigDecimal value) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void equalDouble(Column storeColumn, double value) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void equalFloat(Column storeColumn, float value) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void equalInt(Column storeColumn, int value) {
        int returnCode = ndbOperation.equal(storeColumn.getName(), value);
        handleError(returnCode, ndbOperation);
    }

    public void equalShort(Column storeColumn, short value) {
        int storeValue = Utility.convertShortValueForStorage(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), storeValue);
        handleError(returnCode, ndbOperation);
    }

    public void equalLong(Column storeColumn, long value) {
        long storeValue = Utility.convertLongValueForStorage(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), storeValue);
        handleError(returnCode, ndbOperation);
    }

    public void equalString(Column storeColumn, String value) {
        ByteBuffer stringStorageBuffer = Utility.encode(value, storeColumn, bufferManager);
        int returnCode = ndbOperation.equal(storeColumn.getName(), stringStorageBuffer);
        bufferManager.clearStringStorageBuffer();
        handleError(returnCode, ndbOperation);
    }

    public void getBlob(Column storeColumn) {
        NdbBlob ndbBlob = ndbOperation.getBlobHandleM(storeColumn.getColumnId());
        handleError(ndbBlob, ndbOperation);
    }

    public Blob getBlobHandle(Column storeColumn) {
        NdbBlob blobHandle = ndbOperation.getBlobHandleM(storeColumn.getColumnId());
        handleError(blobHandle, ndbOperation);
        return new BlobImpl(blobHandle, this.byteBufferPool);
    }

    /** Specify the columns to be used for the operation.
     * For now, just save the columns. When resultData is called, pass the columns
     * to the ResultData constructor and then execute the operation.
     * 
     */
    public void getValue(Column storeColumn) {
        storeColumns.add(storeColumn);
    }

    public void postExecuteCallback(Runnable callback) {
        clusterTransaction.postExecuteCallback(callback);
    }

    /** Construct a new ResultData using the saved column data and then execute the operation.
     * 
     */
    public ResultData resultData() {
        return resultData(true);
    }

    /** Construct a new ResultData and if requested, execute the operation.
     * 
     */
    public ResultData resultData(boolean execute) {
        if (logger.isDetailEnabled()) logger.detail("storeColumns: " + Arrays.toString(storeColumns.toArray()));
        ResultDataImpl result;
        if (execute) {
            result = new ResultDataImpl(ndbOperation, storeColumns, maximumColumnId, bufferSize,
                    offsets, lengths, bufferManager, false);
            clusterTransaction.executeNoCommit(false, true);
        } else {
            result = new ResultDataImpl(ndbOperation, storeColumns, maximumColumnId, bufferSize,
                    offsets, lengths, bufferManager, true);
        }
        return result;
    }

    public void setBigInteger(Column storeColumn, BigInteger value) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void setBoolean(Column storeColumn, Boolean value) {
        byte byteValue = (value?(byte)0x01:(byte)0x00);
        setByte(storeColumn, byteValue);
    }

    public void setByte(Column storeColumn, byte value) {
        int storeValue = Utility.convertByteValueForStorage(storeColumn, value);
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), storeValue);
        handleError(returnCode, ndbOperation);
    }

    public void setBytes(Column storeColumn, byte[] value) {
        // TODO use the string storage buffer instead of allocating a new buffer for each value
        int length = value.length;
        if (length > storeColumn.getLength()) {
            throw new ClusterJUserException(local.message("ERR_Data_Too_Long", 
                    storeColumn.getName(), storeColumn.getLength(), length));
        }
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void setDecimal(Column storeColumn, BigDecimal value) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void setDouble(Column storeColumn, Double value) {
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), value);
        handleError(returnCode, ndbOperation);
    }

    public void setFloat(Column storeColumn, Float value) {
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), value);
        handleError(returnCode, ndbOperation);
    }

    public void setInt(Column storeColumn, Integer value) {
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), value);
        handleError(returnCode, ndbOperation);
    }

    public void setLong(Column storeColumn, long value) {
        long storeValue = Utility.convertLongValueForStorage(storeColumn, value);
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), storeValue);
        handleError(returnCode, ndbOperation);
    }

    public void setNull(Column storeColumn) {
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), null);
        handleError(returnCode, ndbOperation);
    }

    public void setShort(Column storeColumn, Short value) {
        int storeValue = Utility.convertShortValueForStorage(storeColumn, value);
        int returnCode = ndbOperation.setValue(storeColumn.getName(), storeValue);
        handleError(returnCode, ndbOperation);
    }

    public void setString(Column storeColumn, String value) {
        ByteBuffer stringStorageBuffer = Utility.encode(value, storeColumn, bufferManager);
        int length = stringStorageBuffer.remaining() - storeColumn.getPrefixLength();
        if (length > storeColumn.getLength()) {
            throw new ClusterJUserException(local.message("ERR_Data_Too_Long", 
                    storeColumn.getName(), storeColumn.getLength(), length));
        }
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), stringStorageBuffer);
        bufferManager.clearStringStorageBuffer();
        handleError(returnCode, ndbOperation);
    }

    public int errorCode() {
        return ndbOperation.getNdbError().code();
    }

    protected void handleError(int returnCode, NdbOperation ndbOperation) {
        if (returnCode == 0) {
            return;
        } else {
            Utility.throwError(returnCode, ndbOperation.getNdbError());
        }
    }

    protected static void handleError(Object object, NdbOperation ndbOperation) {
        if (object != null) {
            return;
        } else {
            Utility.throwError(null, ndbOperation.getNdbError());
        }
    }

    public void beginDefinition() {
        // nothing to do
    }

    public void endDefinition() {
        // nothing to do
    }

    public boolean isReadOperation() {
        return isReadOp;
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

    public void freeResourcesAfterExecute() {
        System.out.println("OperationImpl.freeResourcesAfterExecute()");
    }

}
