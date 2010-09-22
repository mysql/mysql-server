/*
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *  All rights reserved. Use is subject to license terms.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.tie;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.util.ArrayList;
import java.util.Arrays;
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

import com.mysql.ndbjtie.mysql.CharsetMapConst;
import com.mysql.ndbjtie.ndbapi.NdbBlob;
import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
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

    /** String storage buffer initial size (used for non-primitive output data) 
     * including two byte length field to be sent directly to ndb api.*/
    public static final int STRING_STORAGE_BUFFER_INITIAL_SIZE = 502;

    /** String byte buffer initial size */
    public static final int STRING_BYTE_BUFFER_INITIAL_SIZE = 1000;

    /** String byte buffer current size */
    private int stringByteBufferCurrentSize = STRING_BYTE_BUFFER_INITIAL_SIZE;

    /** Scratch buffers for String encoding; reused for each String column in the operation.
     * These buffers share common data but have their own position and limit. */
    ByteBuffer stringByteBuffer = null;
    CharBuffer stringCharBuffer = null;

    private NdbOperation ndbOperation;

    protected List<Column> storeColumns = new ArrayList<Column>();

    protected ClusterTransactionImpl clusterTransaction;

    /** The size of the receive buffer for this operation (may be zero for non-read operations) */
    protected int bufferSize;

    /** The maximum column id for this operation (may be zero for non-read operations) */
    protected int maximumColumnId;

    /** The offsets into the buffer for each column (may be null for non-read operations) */
    protected int[] offsets;

    /** The lengths of fields in the buffer for each column (may be null for non-read operations) */
    protected int[] lengths;

    /** Shared buffer for output operations, with a minimum size of 8 */
    protected ByteBuffer stringStorageBuffer = ByteBuffer.allocateDirect(STRING_STORAGE_BUFFER_INITIAL_SIZE);

    /** Constructor used for insert and delete operations that do not need to read data.
     * 
     * @param operation the operation
     * @param transaction the transaction
     */
    public OperationImpl(NdbOperation operation, ClusterTransactionImpl transaction) {
        this.ndbOperation = operation;
        this.clusterTransaction = transaction;
    }

    /** Constructor used for read operations. The table is used to obtain data used
     * to lay out memory for the result. 
     * @param storeTable the table
     * @param operation the operation
     * @param transaction the transaction
     */
    public OperationImpl(Table storeTable, NdbOperation operation, ClusterTransactionImpl transaction) {
        this.ndbOperation = operation;
        this.clusterTransaction = transaction;
        TableImpl tableImpl = (TableImpl)storeTable;
        this.maximumColumnId = tableImpl.getMaximumColumnId();
        this.bufferSize = tableImpl.getBufferSize();
        this.offsets = tableImpl.getOffsets();
        this.lengths = tableImpl.getLengths();
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
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void getBlob(Column storeColumn) {
        NdbBlob ndbBlob = ndbOperation.getBlobHandleM(storeColumn.getColumnId());
        handleError(ndbBlob, ndbOperation);
    }

    public Blob getBlobHandle(Column storeColumn) {
        NdbBlob blobHandle = ndbOperation.getBlobHandleM(storeColumn.getColumnId());
        handleError(blobHandle, ndbOperation);
        return new BlobImpl(blobHandle);
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
        if (logger.isDetailEnabled()) logger.detail("storeColumns: " + Arrays.toString(storeColumns.toArray()));
        ResultDataImpl result;
        result = new ResultDataImpl(ndbOperation, storeColumns, maximumColumnId, bufferSize, offsets, lengths);
        clusterTransaction.executeNoCommit(false, true);
        NdbErrorConst error = ndbOperation.getNdbError();
        int errorCode = error.code();
        if (errorCode != 0)
            result.setNoResult();
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
        int returnCode = ndbOperation.setValue(storeColumn.getName(), storeValue);
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
        ByteBuffer inputByteBuffer = copyStringToByteBuffer(value);
        // assume output size is not larger than input size
        int sizeNeeded = inputByteBuffer.limit() - inputByteBuffer.position();
        boolean done = false;
        int offset = storeColumn.getPrefixLength();
        while (!done) {
            guaranteeStringStorageBufferSize(sizeNeeded);
            int returnCode = Utility.encode(storeColumn.getCharsetNumber(), offset,
                    sizeNeeded, inputByteBuffer, stringStorageBuffer);
            switch (returnCode) {
                case CharsetMapConst.RecodeStatus.RECODE_OK:
                    done = true;
                    break;
                case CharsetMapConst.RecodeStatus.RECODE_BUFF_TOO_SMALL:
                    logger.warn(local.message("WARN_Recode_Buffer_Too_Small", sizeNeeded));
                    // double output size and try again
                    sizeNeeded *= 2;
                    break;
                default:
                    throw new ClusterJFatalInternalException(local.message("ERR_Encode_Bad_Return_Code",
                        returnCode));
            }
        }
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), stringStorageBuffer);
        resetStringStorageBuffer();
        handleError(returnCode, ndbOperation);
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

    /** Guarantee the size of the string storage buffer to be a minimum size. If the current
     * string storage buffer is not big enough, allocate a bigger one. The current buffer
     * will be garbage collected.
     * @param size the minimum size required
     */
    protected void guaranteeStringStorageBufferSize(int sizeNeeded) {
        if (sizeNeeded > stringStorageBuffer.capacity()) {
            // the existing shared buffer will be garbage collected
            stringStorageBuffer = ByteBuffer.allocateDirect(sizeNeeded);
        }
    }

    /** Reset the string storage buffer so it can be used for another operation.
     * 
     */
    private void resetStringStorageBuffer() {
        int capacity = stringStorageBuffer.capacity();
        stringStorageBuffer.position(0);
        stringStorageBuffer.limit(capacity);
    }

    /** Copy the contents of the parameter String into a reused string buffer.
     * The ByteBuffer can subsequently be encoded into a ByteBuffer.
     * @param value the string
     * @return the byte buffer with the String in it
     */
    private ByteBuffer copyStringToByteBuffer(String value) {
        int sizeNeeded = value.length() * 2;
        if (sizeNeeded > stringByteBufferCurrentSize) {
            stringByteBufferCurrentSize = sizeNeeded;
            stringByteBuffer = ByteBuffer.allocateDirect(sizeNeeded);
            stringCharBuffer = stringByteBuffer.asCharBuffer();
        }
        if (stringByteBuffer == null) {
            stringByteBuffer = ByteBuffer.allocateDirect(STRING_BYTE_BUFFER_INITIAL_SIZE);
            stringCharBuffer = stringByteBuffer.asCharBuffer();
        } else {
            stringByteBuffer.clear();
            stringCharBuffer.clear();
        }
        stringCharBuffer.append(value);
        // characters in java are always two bytes (UCS-16)
        stringByteBuffer.limit(stringCharBuffer.position() * 2);
        return stringByteBuffer;
    }

}
