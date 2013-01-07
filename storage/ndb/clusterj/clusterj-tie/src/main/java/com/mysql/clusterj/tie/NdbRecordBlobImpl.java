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

    /** The data holder for this blob */
    private byte[] data;

    /** The operation */
    private NdbRecordOperationImpl operation;

    public NdbRecordBlobImpl(NdbRecordOperationImpl operation, Column storeColumn) {
        this.storeColumn = storeColumn;
        this.operation = operation;
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
        data = new byte[length];
        readData(data, length);
    }

}
