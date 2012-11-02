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

import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.Blob;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.ResultData;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 * Handle the results of an operation using NdbRecord. 
 */
class NdbRecordResultDataImpl implements ResultData {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(NdbRecordResultDataImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(NdbRecordResultDataImpl.class);

    /** The NdbOperation that defines the result */
    protected NdbRecordOperationImpl operation = null;

    /** The flag indicating that there are no more results */
    private boolean nextDone;

    /** Construct the ResultDataImpl based on an NdbRecordOperationImpl.
     * @param operation the NdbRecordOperationImpl
     */
    public NdbRecordResultDataImpl(NdbRecordOperationImpl operation) {
        this.operation = operation;
    }

    public boolean next() {
        // NdbOperation has exactly zero or one result. NdbRecordScanResultDataImpl handles scans...
        // if the ndbOperation reports an error there is no result
        int errorCode = operation.errorCode();
        if (errorCode != 0) {
            setNoResult();
        }
        if (nextDone) {
            return false;
        } else {
            nextDone = true;
            return true;
        }
    }

    public Blob getBlob(int columnId) {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented",
                "NdbRecordResultDataImpl.getBlob(int)"));
    }

    public Blob getBlob(Column storeColumn) {
        return operation.getBlobHandle(storeColumn);
    }

    public boolean getBoolean(int columnId) {
        return operation.getBoolean(columnId);
    }

    public boolean getBoolean(Column storeColumn) {
        return operation.getBoolean(storeColumn.getColumnId());
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
        return operation.getByte(columnId);
    }

    public byte getByte(Column storeColumn) {
        return operation.getByte(storeColumn.getColumnId());
    }

    public short getShort(int columnId) {
        return operation.getShort(columnId);
    }

    public short getShort(Column storeColumn) {
        return operation.getShort(storeColumn.getColumnId());
     }

    public int getInt(int columnId) {
        return operation.getInt(columnId);
    }

    public int getInt(Column storeColumn) {
        return getInt(storeColumn.getColumnId());
    }

    public long getLong(int columnId) {
        return operation.getLong(columnId);
    }

    public long getLong(Column storeColumn) {
        return getLong(storeColumn.getColumnId());
     }

    public float getFloat(int columnId) {
        return operation.getFloat(columnId);
    }

    public float getFloat(Column storeColumn) {
        return getFloat(storeColumn.getColumnId());
    }

    public double getDouble(int columnId) {
        return operation.getDouble(columnId);
    }

    public double getDouble(Column storeColumn) {
        return getDouble(storeColumn.getColumnId());
    }

    public String getString(int columnId) {
        return operation.getString(columnId);
    }

    public String getString(Column storeColumn) {
        return operation.getString(storeColumn.getColumnId());
    }

    public byte[] getBytes(int column) {
        return operation.getBytes(column);
    }

    public byte[] getBytes(Column storeColumn) {
        return operation.getBytes(storeColumn);
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
        return operation.getObjectBoolean(column);
    }

    public Boolean getObjectBoolean(Column storeColumn) {
        return operation.getObjectBoolean(storeColumn.getColumnId());
    }

    public Byte getObjectByte(int columnId) {
        return operation.getObjectByte(columnId);
    }

    public Byte getObjectByte(Column storeColumn) {
        return operation.getObjectByte(storeColumn.getColumnId());
    }

    public Float getObjectFloat(int column) {
        return operation.getObjectFloat(column);
    }

    public Float getObjectFloat(Column storeColumn) {
        return operation.getObjectFloat(storeColumn.getColumnId());
    }

    public Double getObjectDouble(int column) {
        return operation.getObjectDouble(column);
    }

    public Double getObjectDouble(Column storeColumn) {
        return operation.getObjectDouble(storeColumn.getColumnId());
    }

    public Integer getObjectInteger(int columnId) {
        return operation.getObjectInteger(columnId);
    }

    public Integer getObjectInteger(Column storeColumn) {
        return operation.getObjectInteger(storeColumn.getColumnId());
    }

    public Long getObjectLong(int column) {
        return operation.getObjectLong(column);
    }

    public Long getObjectLong(Column storeColumn) {
        return operation.getObjectLong(storeColumn.getColumnId());
    }

    public Short getObjectShort(int columnId) {
        return operation.getObjectShort(columnId);
    }

    public Short getObjectShort(Column storeColumn) {
        return operation.getObjectShort(storeColumn.getColumnId());
    }

    public BigInteger getBigInteger(int column) {
        return operation.getBigInteger(column);
    }

    public BigInteger getBigInteger(Column storeColumn) {
        return operation.getBigInteger(storeColumn);
    }

    public BigDecimal getDecimal(int column) {
        return operation.getDecimal(column);
    }

    public BigDecimal getDecimal(Column storeColumn) {
        return operation.getDecimal(storeColumn);
    }

    public void setNoResult() {
        nextDone = true;
    }

    public Column[] getColumns() {
        return null;
    }

    /** Return an operation that can be used by SmartValueHandler.
     * The operation contains the buffer with the row data from the operation.
     * @return the operation
     */
    public NdbRecordOperationImpl transformOperation() {
        return operation.transformNdbRecordOperationImpl();
    }

}
