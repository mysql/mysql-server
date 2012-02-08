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
import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.Blob;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.ResultData;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.clusterj.tie.DbImpl.BufferManager;

/**
 *
 */
class NdbRecordResultDataImpl implements ResultData {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(NdbRecordResultDataImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(NdbRecordResultDataImpl.class);

    /** Flags for iterating a scan */
    protected final int RESULT_READY = 0;
    protected final int SCAN_FINISHED = 1;
    protected final int CACHE_EMPTY = 2;

    /** The NdbOperation that defines the result */
    private NdbRecordOperationImpl operation = null;

    /** The NdbRecordImpl that defines the buffer layout */
    private NdbRecordImpl record = null;

    /** The flag indicating that there are no more results */
    private boolean nextDone;

    /** The ByteBuffer containing the results */
    private ByteBuffer buffer = null;

    /** The buffer manager */
    private BufferManager bufferManager;

    /** Construct the ResultDataImpl based on an NdbRecordOperationImpl, and the 
     * buffer manager to help with string columns.
     * @param operation the NdbRecordOperationImpl
     * @param bufferManager the buffer manager
     */
    public NdbRecordResultDataImpl(NdbRecordOperationImpl operation, NdbRecordImpl ndbRecordImpl,
            ByteBuffer buffer, BufferManager bufferManager) {
        this.operation = operation;
        this.record = ndbRecordImpl;
        this.bufferManager = bufferManager;
        this.buffer = buffer;
    }

    public boolean next() {
        // NdbOperation has exactly zero or one result. ScanResultDataImpl handles scans...
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
        return record.getBoolean(buffer, columnId);
    }

    public boolean getBoolean(Column storeColumn) {
        return record.getBoolean(buffer, storeColumn.getColumnId());
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
        return record.getByte(buffer, columnId);
    }

    public byte getByte(Column storeColumn) {
        return record.getByte(buffer, storeColumn.getColumnId());
    }

    public short getShort(int columnId) {
        return record.getShort(buffer, columnId);
    }

    public short getShort(Column storeColumn) {
        return record.getShort(buffer, storeColumn.getColumnId());
     }

    public int getInt(int columnId) {
        return record.getInt(buffer, columnId);
    }

    public int getInt(Column storeColumn) {
        return getInt(storeColumn.getColumnId());
    }

    public long getLong(int columnId) {
        return record.getLong(buffer, columnId);
    }

    public long getLong(Column storeColumn) {
        return getLong(storeColumn.getColumnId());
     }

    public float getFloat(int columnId) {
        return record.getFloat(buffer, columnId);
    }

    public float getFloat(Column storeColumn) {
        return getFloat(storeColumn.getColumnId());
    }

    public double getDouble(int columnId) {
        return record.getDouble(buffer, columnId);
    }

    public double getDouble(Column storeColumn) {
        return getDouble(storeColumn.getColumnId());
    }

    public String getString(int columnId) {
        return record.getString(buffer, columnId, bufferManager);
    }

    public String getString(Column storeColumn) {
        return record.getString(buffer, storeColumn.getColumnId(), bufferManager);
    }

    public byte[] getBytes(int column) {
        return record.getBytes(buffer, column);
    }

    public byte[] getBytes(Column storeColumn) {
        return record.getBytes(buffer, storeColumn);
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
        return record.getObjectBoolean(buffer, column);
    }

    public Boolean getObjectBoolean(Column storeColumn) {
        return record.getObjectBoolean(buffer, storeColumn.getColumnId());
    }

    public Byte getObjectByte(int columnId) {
        return record.getObjectByte(buffer, columnId);
    }

    public Byte getObjectByte(Column storeColumn) {
        return record.getObjectByte(buffer, storeColumn.getColumnId());
    }

    public Float getObjectFloat(int column) {
        return record.getObjectFloat(buffer, column);
    }

    public Float getObjectFloat(Column storeColumn) {
        return record.getObjectFloat(buffer, storeColumn.getColumnId());
    }

    public Double getObjectDouble(int column) {
        return record.getObjectDouble(buffer, column);
    }

    public Double getObjectDouble(Column storeColumn) {
        return record.getObjectDouble(buffer, storeColumn.getColumnId());
    }

    public Integer getObjectInteger(int columnId) {
        return record.getObjectInteger(buffer, columnId);
    }

    public Integer getObjectInteger(Column storeColumn) {
        return record.getObjectInteger(buffer, storeColumn.getColumnId());
    }

    public Long getObjectLong(int column) {
        return record.getObjectLong(buffer, column);
    }

    public Long getObjectLong(Column storeColumn) {
        return record.getObjectLong(buffer, storeColumn.getColumnId());
    }

    public Short getObjectShort(int columnId) {
        return record.getObjectShort(buffer, columnId);
    }

    public Short getObjectShort(Column storeColumn) {
        return record.getObjectShort(buffer, storeColumn.getColumnId());
    }

    public BigInteger getBigInteger(int column) {
        return record.getBigInteger(buffer, column);
    }

    public BigInteger getBigInteger(Column storeColumn) {
        return record.getBigInteger(buffer, storeColumn);
    }

    public BigDecimal getDecimal(int column) {
        return record.getDecimal(buffer, column);
    }

    public BigDecimal getDecimal(Column storeColumn) {
        return record.getDecimal(buffer, storeColumn);
    }

    public void setNoResult() {
        nextDone = true;
    }

    public Column[] getColumns() {
        return null;
    }

}
