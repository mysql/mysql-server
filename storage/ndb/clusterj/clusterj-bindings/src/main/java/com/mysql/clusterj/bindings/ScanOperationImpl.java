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

package com.mysql.clusterj.bindings;

import com.mysql.cluster.ndbj.NdbApiException;
import com.mysql.cluster.ndbj.NdbScanOperation;
import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;

/**
 *
 */
class ScanOperationImpl extends OperationImpl implements ScanOperation {

    private NdbScanOperation scanOperation;

    ScanOperationImpl(NdbScanOperation operation, ClusterTransactionImpl transaction) {
        super(operation, transaction);
        this.scanOperation = operation;
    }

    public void close() {
        scanOperation.close();
    }

    public void deleteCurrentTuple() {
        try {
            scanOperation.deleteCurrentTuple();
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public ScanFilter getScanFilter() {
        try {
            return new ScanFilterImpl(scanOperation.getNdbScanFilter());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public int nextResult(boolean fetch) {
        try {
            return scanOperation.nextResult(fetch);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

}
