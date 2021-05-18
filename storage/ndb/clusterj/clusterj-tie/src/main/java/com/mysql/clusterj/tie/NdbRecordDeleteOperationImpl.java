/*
 *  Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.
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
