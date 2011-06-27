/*
 *  Copyright 2010 Sun Microsystems, Inc.
 *  All rights reserved. Use is subject to license terms.
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

    private NdbBlob ndbBlob;

    public BlobImpl(NdbBlob blob) {
        this.ndbBlob = blob;
    }

    public Long getLength() {
        long[] length = new long[1];
        int returnCode = ndbBlob.getLength(length);
        handleError(returnCode, ndbBlob);
        return length[0];
    }

    public void readData(byte[] array, int length) {
        // int[1] is an artifact of ndbjtie to pass an in/out parameter
        int[] lengthRead = new int[] {length};
        ByteBuffer buffer = ByteBuffer.allocateDirect(array.length);
        int returnCode = ndbBlob.readData(buffer, lengthRead);
        handleError(returnCode, ndbBlob);
        if (lengthRead[0] != length) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Blob_Read_Data", length, lengthRead[0]));
        }
        // now copy into user space
        buffer.get(array);
    }

    public void writeData(byte[] array) {
        // TODO can we really skip this when updating to an empty blob?
        if (array.length == 0) return;
        ByteBuffer buffer = null;
        if (array.length > 0) {
            buffer = ByteBuffer.allocateDirect(array.length);
            buffer.put(array);
            buffer.flip();
        }
        int returnCode = ndbBlob.writeData(buffer, array.length);
        handleError(returnCode, ndbBlob);
    }

    public void setValue(byte[] array) {
        // TODO can we really skip this when updating to an empty blob?
        if (array.length == 0) return;
        ByteBuffer buffer = null;
        if (array.length > 0) {
            buffer = ByteBuffer.allocateDirect(array.length);
            buffer.put(array);
            buffer.flip();
        }
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
