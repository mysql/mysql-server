/*
   Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.Table;

/** NdbRecordTableScanOperationImpl performs table scans using NdbRecord.
 * Most methods are implemented in the superclass.
 */
public class NdbRecordTableScanOperationImpl extends NdbRecordScanOperationImpl implements ScanOperation {

    public NdbRecordTableScanOperationImpl(ClusterTransactionImpl clusterTransaction, Table storeTable,
            int lockMode) {
        super(clusterTransaction, storeTable, lockMode);
    }

    public void endDefinition() {
        // get the scan options which also sets the filter
        getScanOptions();
        // create the ndb scan operation
        ndbOperation = clusterTransaction.scanTable(ndbRecordValues.getNdbRecord(), mask, scanOptions);
        // set the NdbBlob for all active blob columns so that scans will read blob columns
        activateBlobs();
        clusterTransaction.postExecuteCallback(new Runnable() {
            public void run() {
                freeResourcesAfterExecute();
            }
        });
    }

}
