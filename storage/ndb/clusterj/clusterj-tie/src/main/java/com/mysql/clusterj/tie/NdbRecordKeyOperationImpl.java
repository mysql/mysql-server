/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.Table;

public class NdbRecordKeyOperationImpl extends NdbRecordOperationImpl {

    /** The number of columns in the table */
    protected int numberOfColumns;

    public NdbRecordKeyOperationImpl(ClusterTransactionImpl clusterTransaction, Table storeTable) {
        super(clusterTransaction);
        this.ndbRecordKeys = clusterTransaction.getCachedNdbRecordImpl(storeTable);
        this.keyBufferSize = ndbRecordKeys.getBufferSize();
        this.ndbRecordValues = clusterTransaction.getCachedNdbRecordImpl(storeTable);
        this.valueBufferSize = ndbRecordValues.getBufferSize();
        this.numberOfColumns = ndbRecordValues.getNumberOfColumns();
        this.blobs = new NdbRecordBlobImpl[this.numberOfColumns];
    }

    public void beginDefinition() {
        // allocate a buffer for the key data
        keyBuffer = ByteBuffer.allocateDirect(keyBufferSize);
        keyBuffer.order(ByteOrder.nativeOrder());
        // allocate a buffer for the value result data
        valueBuffer = ByteBuffer.allocateDirect(valueBufferSize);
        valueBuffer.order(ByteOrder.nativeOrder());
        mask = new byte[1 + (numberOfColumns/8)];
    }

    /** Specify the columns to be used for the operation.
     */
    public void getValue(Column storeColumn) {
        int columnId = storeColumn.getColumnId();
        columnSet(columnId);
    }

    /**
     * Mark this blob column to be read.
     * @param storeColumn the store column
     */
    @Override
    public void getBlob(Column storeColumn) {
        // create an NdbRecordBlobImpl for the blob
        int columnId = storeColumn.getColumnId();
        columnSet(columnId);
        NdbRecordBlobImpl blob = new NdbRecordBlobImpl(this, storeColumn);
        blobs[columnId] = blob;
    }

    public void endDefinition() {
        // position the key buffer at the beginning for ndbjtie
        keyBuffer.position(0);
        keyBuffer.limit(keyBufferSize);
        // position the value buffer at the beginning for ndbjtie
        valueBuffer.position(0);
        valueBuffer.limit(valueBufferSize);
        // create the key operation
        ndbOperation = clusterTransaction.readTuple(ndbRecordKeys.getNdbRecord(), keyBuffer,
                ndbRecordValues.getNdbRecord(), valueBuffer, mask, null);
        // set up a callback when this operation is executed
        clusterTransaction.postExecuteCallback(new Runnable() {
            public void run() {
                for (int columnId = 0; columnId < numberOfColumns; ++columnId) {
                    NdbRecordBlobImpl blob = blobs[columnId];
                    if (blob != null) {
                        blob.setNdbBlob();
                    }
                }
            }
        });
    }

    /** Construct a new ResultData using the saved column data and then execute the operation.
     */
    @Override
    public ResultData resultData() {
        return resultData(true);
    }

    /** Construct a new ResultData and if requested, execute the operation.
     */
    @Override
    public ResultData resultData(boolean execute) {
        NdbRecordResultDataImpl result =
            new NdbRecordResultDataImpl(this, ndbRecordValues, valueBuffer, bufferManager);
        if (execute) {
            clusterTransaction.executeNoCommit(false, true);
        }
        return result;
    }

}
