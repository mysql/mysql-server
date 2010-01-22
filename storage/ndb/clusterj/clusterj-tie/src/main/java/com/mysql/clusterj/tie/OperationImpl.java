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
import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.core.store.Blob;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ResultData;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

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

    private NdbOperation ndbOperation;

    protected List<Column> storeColumns = new ArrayList<Column>();

    protected ClusterTransactionImpl clusterTransaction;

    public OperationImpl(NdbOperation operation, ClusterTransactionImpl transaction) {
        this.ndbOperation = operation;
        this.clusterTransaction = transaction;
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
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), buffer);
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

    public void equalInt(Column storeColumn, int value) {
        int returnCode = ndbOperation.equal(storeColumn.getName(), value);
        handleError(returnCode, ndbOperation);
    }

    public void equalShort(Column storeColumn, short value) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void equalLong(Column storeColumn, long value) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.equal(storeColumn.getName(), buffer);
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
        ResultDataImpl result;
        result = new ResultDataImpl(ndbOperation, storeColumns);
        clusterTransaction.executeNoCommit(false, false);
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
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void setBytes(Column storeColumn, byte[] value) {
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
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void setNull(Column storeColumn) {
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), null);
        handleError(returnCode, ndbOperation);
    }

    public void setShort(Column storeColumn, Short value) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), buffer);
        handleError(returnCode, ndbOperation);
    }

    public void setString(Column storeColumn, String value) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, value);
        int returnCode = ndbOperation.setValue(storeColumn.getColumnId(), buffer);
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

}
