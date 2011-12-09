/*
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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

import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation;

import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Table;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.math.BigDecimal;
import java.math.BigInteger;

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

    public IndexScanOperationImpl(Table storeTable, NdbIndexScanOperation ndbIndexScanOperation,
            ClusterTransactionImpl transaction) {
        super(storeTable, ndbIndexScanOperation, transaction);
        this.ndbIndexScanOperation = ndbIndexScanOperation;
    }

    public void setBoundBigInteger(Column storeColumn, BoundType type, BigInteger value) {
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundByte(Column storeColumn, BoundType type, byte value) {
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundBytes(Column storeColumn, BoundType type, byte[] value) {
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
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
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundInt(Column storeColumn, BoundType type, Integer value) {
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
        handleError(returnCode, ndbIndexScanOperation);
    }

    public void setBoundLong(Column storeColumn, BoundType type, long value) {
        int returnCode = ndbIndexScanOperation.setBound(storeColumn.getName(), convertBoundType(type),
                Utility.convertValue(storeColumn, value));
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
