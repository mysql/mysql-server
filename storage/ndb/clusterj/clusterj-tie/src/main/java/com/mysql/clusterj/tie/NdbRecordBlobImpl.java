/*
 *  Copyright (c) 2012, 2022, Oracle and/or its affiliates.
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

import com.mysql.clusterj.core.store.Column;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 * NdbRecord blob handling defers the acquisition of an NdbBlob until the NdbOperation
 * is created. At that time, this implementation will get the NdbBlob from its NdbOperation.
 * Operations on the NdbBlob are delegated to the parent (by inheritance).
 */
class NdbRecordBlobImpl extends BlobImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(NdbRecordBlobImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(NdbRecordBlobImpl.class);

    /** The store column for this blob */
    private Column storeColumn;

    public NdbRecordBlobImpl(NdbRecordOperationImpl operation, Column storeColumn, VariableByteBufferPoolImpl byteBufferPool) {
        super(byteBufferPool);
        this.storeColumn = storeColumn;
        this.operation = operation;
    }

    /** Copy the data and column from the other NdbRecordBlobImpl but replace the operation.
     * This constructor is used for scans to copy the data to the newly created NdbRecordOperation.
     * While scanning, the operation used to fetch the blob data is the scan operation. But the
     * operation for the new NdbRecordBlobImpl is a new operation that is not currently bound to
     * an NdbOperation. Subsequent use of the blob will require a new NdbBlob with the NdbOperation.
     * @param operation the new operation that is not connected to the database
     * @param ndbRecordBlobImpl2 the other NdbRecordBlobImpl that is connected to the database
     */
    public NdbRecordBlobImpl(NdbRecordOperationImpl operation, NdbRecordBlobImpl ndbRecordBlobImpl2) {
        super(ndbRecordBlobImpl2.byteBufferPool);
        this.operation = operation;
        this.storeColumn = ndbRecordBlobImpl2.storeColumn;
        this.data = ndbRecordBlobImpl2.data;
    }

    public int getColumnId() {
        return storeColumn.getColumnId();
    }

    protected void setNdbBlob() {
        this.ndbBlob = operation.getNdbBlob(storeColumn);
    }

    public void setValue() {
        setValue(data);
    }

    public void setData(byte[] bytes) {
        data = bytes;
    }

    public void setData(String string) {
        data = storeColumn.encode(string);
    }

    public byte[] getBytesData() {
        return data;
    }

    public String getStringData() {
        return storeColumn.decode(data);
    }

    /** Read data from the NdbBlob into the data holder.
     * 
     */
    public void readData() {
        int length = getLength().intValue();
        if (logger.isDetailEnabled()) {
            logger.detail("reading: " + length + " bytes.");
        }
        data = new byte[length];
        readData(data, length);
    }

}
