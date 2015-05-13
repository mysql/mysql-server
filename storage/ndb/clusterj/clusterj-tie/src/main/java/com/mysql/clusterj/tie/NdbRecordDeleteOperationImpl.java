/*
 *  Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.
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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import com.mysql.clusterj.core.store.Table;

public class NdbRecordDeleteOperationImpl extends NdbRecordOperationImpl {

    public NdbRecordDeleteOperationImpl(
            ClusterTransactionImpl clusterTransaction, Table storeTable) {
        super(clusterTransaction, storeTable);
        this.ndbRecordKeys = clusterTransaction.getCachedNdbRecordImpl(storeTable);
        this.keyBufferSize = ndbRecordKeys.getBufferSize();
        this.numberOfColumns = ndbRecordKeys.getNumberOfColumns();
        resetMask();
    }

    public void beginDefinition() {
        // allocate a buffer for the operation data
        keyBuffer = ndbRecordKeys.newBuffer();
        // use platform's native byte ordering
        keyBuffer.order(ByteOrder.nativeOrder());
    }

    public void endDefinition() {
        // create the delete operation
        ndbOperation = delete(clusterTransaction);
        clusterTransaction.postExecuteCallback(new Runnable() {
            public void run() {
                freeResourcesAfterExecute();
            }
        });
    }

    @Override
    public String toString() {
        return " delete " + tableName;
    }

}
