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

import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbOperation;
import com.mysql.ndbjtie.ndbapi.NdbScanFilter;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;

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
        ScanFilter scanFilter = new ScanFilterImpl(ndbScanFilter);
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
        ResultData result = new ScanResultDataImpl(ndbScanOperation, storeColumns,
                maximumColumnId, bufferSize, offsets, lengths, maximumColumnLength, bufferManager);
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

}
