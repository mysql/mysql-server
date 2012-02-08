/*
 *  Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
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

    /** The number of columns for this operation */
    protected int numberOfColumns;

    public NdbRecordDeleteOperationImpl(
            ClusterTransactionImpl clusterTransaction, Table storeTable) {
        super(clusterTransaction);
        this.ndbRecordKeys = clusterTransaction.getCachedNdbRecordImpl(storeTable);
        this.keyBufferSize = ndbRecordKeys.getBufferSize();
        this.numberOfColumns = ndbRecordKeys.getNumberOfColumns();
    }

    public void beginDefinition() {
        // allocate a buffer for the operation data
        keyBuffer = ByteBuffer.allocateDirect(keyBufferSize);
        // use platform's native byte ordering
        keyBuffer.order(ByteOrder.nativeOrder());
        mask = new byte[1 + (numberOfColumns/8)];
    }

    public void endDefinition() {
        // position the buffer at the beginning for ndbjtie
        keyBuffer.position(0);
        keyBuffer.limit(keyBufferSize);
        // create the delete operation
        ndbOperation = clusterTransaction.deleteTuple(ndbRecordKeys.getNdbRecord(), keyBuffer, mask, null);
    }

}
