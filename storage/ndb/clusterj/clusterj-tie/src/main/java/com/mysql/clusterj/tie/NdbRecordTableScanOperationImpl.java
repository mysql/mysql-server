/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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
        clusterTransaction.postExecuteCallback(new Runnable() {
            public void run() {
                freeResourcesAfterExecute();
            }
        });
    }

}
