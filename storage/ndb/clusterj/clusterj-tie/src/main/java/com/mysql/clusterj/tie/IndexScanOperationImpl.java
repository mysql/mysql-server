/*
 *  Copyright (c) 2010, 2022, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.tie;

import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation;

import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Table;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.clusterj.tie.DbImpl.BufferManager;

import java.math.BigDecimal;
import java.math.BigInteger;

import java.nio.ByteBuffer;

/**
 *
 */
class IndexScanOperationImpl extends ScanOperationImpl implements IndexScanOperation {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(IndexScanOperationImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(IndexScanOperationImpl.class);

    private NdbIndexScanOperation ndbIndexScanOperation;
    private BufferManager bufferManager;

    public IndexScanOperationImpl(Table storeTable, NdbIndexScanOperation ndbIndexScanOperation,
            ClusterTransactionImpl transaction) {
        super(storeTable, ndbIndexScanOperation, transaction);
        this.ndbIndexScanOperation = ndbIndexScanOperation;
        this.bufferManager = transaction.getBufferManager();
    }

    public void setBoundBigInteger(Column storeColumn, BoundType type, BigInteger value) {
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundByte(Column storeColumn, BoundType type, byte value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(1);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                buffer);
        bufferManager.returnBuffer(1, buffer);
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundBytes(Column storeColumn, BoundType type, byte[] value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(value.length + 3);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                buffer);
        bufferManager.returnBuffer(value.length + 3, buffer);
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundDecimal(Column storeColumn, BoundType type, BigDecimal value) {
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundDouble(Column storeColumn, BoundType type, Double value) {
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundFloat(Column storeColumn, BoundType type, Float value) {
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundShort(Column storeColumn, BoundType type, short value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(2);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                buffer);
        bufferManager.returnBuffer(2, buffer);
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundInt(Column storeColumn, BoundType type, Integer value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(4);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                buffer);
        bufferManager.returnBuffer(4, buffer);
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundInt(Column storeColumn, BoundType type, int value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(4);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                buffer);
        bufferManager.returnBuffer(4, buffer);
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundLong(Column storeColumn, BoundType type, long value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(8);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                buffer);
        bufferManager.returnBuffer(8, buffer);
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundString(Column storeColumn, BoundType type, String value) {
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
        handleError(returnCode, ndbIndexScanOperation);
    }

    private int convertBoundType(BoundType type) {
        switch (type) {
            case BoundEQ:
                return NdbIndexScanOperation.BoundType.BoundEQ;
            case BoundGE:
                return NdbIndexScanOperation.BoundType.BoundGE;
            case BoundGT:
                return NdbIndexScanOperation.BoundType.BoundGT;
            case BoundLE:
                return NdbIndexScanOperation.BoundType.BoundLE;
            case BoundLT:
                return NdbIndexScanOperation.BoundType.BoundLT;
            default:
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Implementation_Should_Not_Occur"));
        }
    }

    public void endBound(int rangeNumber) {
        if (logger.isDetailEnabled()) logger.detail("IndexScanOperationImpl.endBound(" + rangeNumber + ")");
        int returnCode = ndbIndexScanOperation.end_of_bound(rangeNumber);
        handleError(returnCode, ndbIndexScanOperation);
    }

}
