/*
   Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.core.store.Table;

public class NdbRecordKeyOperationImpl extends NdbRecordOperationImpl {

    public NdbRecordKeyOperationImpl(ClusterTransactionImpl clusterTransaction, Table storeTable) {
        super(clusterTransaction, storeTable);
        this.ndbRecordKeys = this.ndbRecordValues;
        this.keyBufferSize = this.valueBufferSize;
        this.keyBuffer = valueBuffer;
    }

    public void endDefinition() {
        // position the value buffer at the beginning for ndbjtie
        valueBuffer.position(0);
        valueBuffer.limit(valueBufferSize);
        // create the key operation
        ndbOperation = clusterTransaction.readTuple(ndbRecordKeys.getNdbRecord(), keyBuffer,
                ndbRecordValues.getNdbRecord(), valueBuffer, mask, null);
        // set the NdbBlob for all active blob columns
        activateBlobs();
        clusterTransaction.postExecuteCallback(new Runnable() {
            public void run() {
                freeResourcesAfterExecute();
                loadBlobValues();
            }
        });
    }

    @Override
    public String toString() {
        return " primary key " + tableName;
    }

}
