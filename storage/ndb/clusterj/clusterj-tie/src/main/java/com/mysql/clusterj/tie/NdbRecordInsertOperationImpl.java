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

public class NdbRecordInsertOperationImpl extends NdbRecordOperationImpl {

    /** The number of columns for this operation */
    protected int numberOfColumns;

    public NdbRecordInsertOperationImpl(ClusterTransactionImpl clusterTransaction, Table storeTable) {
        super(clusterTransaction);
        this.ndbRecordValues = clusterTransaction.getCachedNdbRecordImpl(storeTable);
        this.ndbRecordKeys = ndbRecordValues;
        this.valueBufferSize = ndbRecordValues.getBufferSize();
        this.numberOfColumns = ndbRecordValues.getNumberOfColumns();
        this.blobs = new NdbRecordBlobImpl[this.numberOfColumns];
    }

    public void beginDefinition() {
        // allocate a buffer for the operation data
        valueBuffer = ByteBuffer.allocateDirect(valueBufferSize);
        // use platform's native byte ordering
        valueBuffer.order(ByteOrder.nativeOrder());
        // use value buffer for key buffer also
        keyBuffer = valueBuffer;
        mask = new byte[1 + (numberOfColumns/8)];
    }

    public void endDefinition() {
        // position the buffer at the beginning for ndbjtie
        valueBuffer.position(0);
        valueBuffer.limit(valueBufferSize);
        // create the insert operation
        ndbOperation = clusterTransaction.insertTuple(ndbRecordValues.getNdbRecord(), valueBuffer, mask, null);
        // now set the NdbBlob into the blobs
        for (NdbRecordBlobImpl blob: activeBlobs) {
            if (blob != null) {
                blob.setNdbBlob();
            }
        }
    }

}
