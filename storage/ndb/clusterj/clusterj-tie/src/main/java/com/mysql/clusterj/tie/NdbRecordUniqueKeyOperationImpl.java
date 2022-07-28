/*
   Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.IndexOperation;
import com.mysql.clusterj.core.store.Table;

public class NdbRecordUniqueKeyOperationImpl extends NdbRecordOperationImpl implements IndexOperation {

    public NdbRecordUniqueKeyOperationImpl(ClusterTransactionImpl clusterTransaction, Index storeIndex, Table storeTable) {
        super(clusterTransaction, storeTable);
        this.valueBuffer = ndbRecordValues.newBuffer();
        this.ndbRecordKeys = clusterTransaction.getCachedNdbRecordImpl(storeIndex, storeTable);
        this.keyBufferSize = ndbRecordKeys.getBufferSize();
        // allocate a buffer for the key data
        keyBuffer = ndbRecordKeys.newBuffer();
    }

    public void endDefinition() {
        // position the key buffer at the beginning for ndbjtie
        keyBuffer.limit(keyBufferSize);
        keyBuffer.position(0);
        // position the value buffer at the beginning for ndbjtie
        valueBuffer.limit(valueBufferSize);
        valueBuffer.position(0);
        // create the key operation
        ndbOperation = clusterTransaction.readTuple(ndbRecordKeys.getNdbRecord(), keyBuffer,
                ndbRecordValues.getNdbRecord(), valueBuffer, mask, null);
        // mark this operation as a read and add this to operationsToCheck list
        isReadOp = true;
        clusterTransaction.addOperationToCheck(this);
        // set the NdbBlob for all active blob columns
        activateBlobs();
        clusterTransaction.postExecuteCallback(new Runnable() {
            public void run() {
                freeResourcesAfterExecute();
                if (ndbOperation.getNdbError().code() == 0) {
                    loadBlobValues();
                }
            }
        });
    }

    @Override
    public String toString() {
        return " unique key " + tableName;
    }

}
