/*
 *  Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.
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

import java.nio.ByteBuffer;

import com.mysql.ndbjtie.ndbapi.NdbBlob;

import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.Blob;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 *
 */
class BlobImpl implements Blob {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(BlobImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(BlobImpl.class);

    protected NdbBlob ndbBlob;

    /** The data holder for this blob */
    protected byte[] data;

    /** The operation */
    protected NdbRecordOperationImpl operation;

    /** The direct byte buffer pool */
    protected VariableByteBufferPoolImpl byteBufferPool;

    /** The direct byte buffer used for setValue, which must be preserved until the operation completes */
    protected ByteBuffer byteBufferForSetValue = null;
    protected int byteBufferForSetValueSize = 0;

    public BlobImpl(VariableByteBufferPoolImpl byteBufferPool) {
        // this is only for NdbRecordBlobImpl constructor when there is no ndbBlob available yet
        this.byteBufferPool = byteBufferPool;
    }

    public BlobImpl(NdbBlob blob, VariableByteBufferPoolImpl byteBufferPool) {
        this.ndbBlob = blob;
        this.byteBufferPool = byteBufferPool;
    }

    /** Release any resources associated with this object.
     * This method is called by the owner of this object when it is being finalized by garbage collection.
     */
    public void release() {
        if (logger.isDetailEnabled()) logger.detail("BlobImpl.release");
        this.data = null;
        this.operation = null;
        // return buffer to pool
        if (byteBufferForSetValue != null) {
            this.byteBufferPool.returnBuffer(byteBufferForSetValueSize, byteBufferForSetValue);
        }
    }

    public Long getLength() {
        long[] length = new long[1];
        int returnCode = ndbBlob.getLength(length);
        handleError(returnCode, ndbBlob);
        return length[0];
    }

    public void readData(byte[] array, int length) {
        if (length == 0) {
            // there is no data to read
            return;
        }
        // int[1] is an artifact of ndbjtie to pass an in/out parameter
        // this depends on java allocating the int[1] on the stack so it can't move while reading blob data
        // we add one to length to trap the case where the int[1] moved so the proper length was not set
        int[] lengthRead = new int[] {length + 1}; // length will be filled by readData
        ByteBuffer buffer = null;
        try {
            buffer = this.byteBufferPool.borrowBuffer(length);
            int returnCode = ndbBlob.readData(buffer, lengthRead);
            handleError(returnCode, ndbBlob);
            if (lengthRead[0] != length) {
                // this will occur if the int[1] moves while reading blob data
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Blob_Read_Data", length, lengthRead[0]));
            }
            // copy the data into user space (provided by the user)
            buffer.get(array);
        } finally {
            // return buffer to pool
            if (buffer != null) {
                this.byteBufferPool.returnBuffer(length, buffer);
            }
        }
    }

    public void writeData(byte[] array) {
        if (array == null) {
            setNull();
            return;
        }
        if (array.length == 0) return;
        ByteBuffer buffer = null;
        int length = array.length;
        try {
            if (length > 0) {
                buffer = this.byteBufferPool.borrowBuffer(length);
                buffer.put(array);
                buffer.flip();
            }
            int returnCode = ndbBlob.writeData(buffer, length);
            handleError(returnCode, ndbBlob);
        } finally {
            // return buffer to pool
            if (buffer != null) {
                this.byteBufferPool.returnBuffer(length, buffer);
            }
        }
    }

    public void setValue(byte[] array) {
        if (array == null) {
            setNull();
            return;
        }
        if (array.length == 0) return;
        ByteBuffer buffer = null;
        buffer = this.byteBufferPool.borrowBuffer(array.length);
        buffer.put(array);
        buffer.flip();
        if (byteBufferForSetValue != null) {
            // free any existing buffer first (setValue was called again -- not likely)
            byteBufferPool.returnBuffer(byteBufferForSetValueSize, byteBufferForSetValue);
        }
        // the buffer will be returned to the pool when release is called
        byteBufferForSetValueSize = array.length;
        byteBufferForSetValue = buffer;
        // the buffer must remain attached to this BlobImpl until the operation is completed
        int returnCode = ndbBlob.setValue(buffer, array.length);
        handleError(returnCode, ndbBlob);
    }

    public void setNull() {
        int returnCode = ndbBlob.setNull();
        handleError(returnCode, ndbBlob);
    }

    public void close() {
        int returnCode = ndbBlob.close(true);
        handleError(returnCode, ndbBlob);
    }

    protected static void handleError(int returnCode, NdbBlob ndbBlob) {
        if (returnCode == 0) {
            return;
        } else {
            Utility.throwError(returnCode, ndbBlob.getNdbError());
        }
    }

}
