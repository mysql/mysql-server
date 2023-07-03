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

import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbOperation;
import com.mysql.ndbjtie.ndbapi.NdbScanFilter;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;

import com.mysql.clusterj.Query.Ordering;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.Table;

/**
 *
 */
class ScanOperationImpl extends OperationImpl implements ScanOperation {

    private NdbScanOperation ndbScanOperation;

    private Ordering ordering = null;

    ScanOperationImpl(Table storeTable, NdbScanOperation operation,
            ClusterTransactionImpl clusterTransaction) {
        super(storeTable, operation, clusterTransaction);
        this.ndbScanOperation = operation;
    }

    public void close() {
        ndbScanOperation.close(true, true);
    }

    public void deleteCurrentTuple() {
        int returnCode = ndbScanOperation.deleteCurrentTuple();
        handleError(returnCode, ndbScanOperation);
    }

    public ScanFilter getScanFilter(QueryExecutionContext context) {
        NdbScanFilter ndbScanFilter = NdbScanFilter.create(ndbScanOperation);
        handleError(ndbScanFilter, ndbScanOperation);
        ScanFilter scanFilter = new ScanFilterImpl(ndbScanFilter, clusterTransaction.getBufferManager());
        context.addFilter(scanFilter);
        return scanFilter;
    }

    public int nextResult(boolean fetch) {
        int result = ndbScanOperation.nextResult(fetch, false);
        clusterTransaction.handleError(result);
        return result;
    }

    @Override
    public ResultData resultData() {
        return resultData(true, 0, Long.MAX_VALUE);
    }

    public ResultData resultData(boolean execute, long skip, long limit) {
        ResultData result = new ScanResultDataImpl(clusterTransaction, ndbScanOperation, storeColumns,
                maximumColumnId, bufferSize, offsets, lengths, maximumColumnLength, bufferManager, skip, limit);
        clusterTransaction.executeNoCommit(false, true);
        return result;
    }

    @Override
    protected void handleError(int returnCode, NdbOperation ndbOperation) {
        if (returnCode == 0) {
            return;
        } else {
            // first check if the error is reported in the NdbOperation
            NdbErrorConst ndbError = ndbOperation.getNdbError();
            if (ndbError != null) {
                // the error is in NdbOperation
                Utility.throwError(returnCode, ndbError);
            } else {
                // the error must be in the NdbTransaction
                clusterTransaction.handleError(returnCode);
            }
        }
    }

    public void setOrdering(Ordering ordering) {
        this.ordering = ordering;
    }

}
