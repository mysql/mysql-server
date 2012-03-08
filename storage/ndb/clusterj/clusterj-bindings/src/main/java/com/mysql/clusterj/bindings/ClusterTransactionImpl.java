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
import com.mysql.cluster.ndbj.NdbOperation.AbortOption;
import com.mysql.cluster.ndbj.NdbOperation;
import com.mysql.cluster.ndbj.NdbScanOperation;
import com.mysql.cluster.ndbj.NdbTransaction;
import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.core.store.ClusterTransaction;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.IndexOperation;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.Table;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import java.util.ArrayList;
import java.util.List;

/**
 *
 */
class ClusterTransactionImpl implements ClusterTransaction {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(ClusterTransactionImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(ClusterTransactionImpl.class);

    protected NdbTransaction ndbTransaction;
    private List<Runnable> postExecuteCallbacks = new ArrayList<Runnable>();

    public ClusterTransactionImpl(NdbTransaction ndbTransaction) {
        this.ndbTransaction = ndbTransaction;
    }

    public void close() {
        ndbTransaction.close();
    }

    public void executeCommit() {
        handlePendingPostExecuteCallbacks();
        try {
            ndbTransaction.executeCommit();
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void executeCommit(boolean abort, boolean force) {
        handlePendingPostExecuteCallbacks();
        AbortOption abortOption = abort?AbortOption.AbortOnError:AbortOption.AO_IgnoreError;
        try {
            ndbTransaction.execute(NdbTransaction.ExecType.Commit,
                    abortOption, force);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void executeNoCommit(boolean abort, boolean force) {
        AbortOption abortOption = abort?AbortOption.AbortOnError:AbortOption.AO_IgnoreError;
        try {
            ndbTransaction.execute(NdbTransaction.ExecType.NoCommit,
                    abortOption, force);
            performPostExecuteCallbacks();
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void executeNoCommit() {
        try {
            ndbTransaction.executeNoCommit();
            performPostExecuteCallbacks();
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void executeRollback() {
        try {
            clearPostExecuteCallbacks();
            ndbTransaction.executeRollback();
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public Operation getDeleteOperation(Table storeTable) {
        try {
            return new OperationImpl(ndbTransaction.getDeleteOperation(storeTable.getName()), this);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public Operation getInsertOperation(Table storeTable) {
        try {
            return new OperationImpl(ndbTransaction.getInsertOperation(storeTable.getName()), this);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public IndexScanOperation getSelectIndexScanOperation(Index storeIndex, Table storeTable) {
        try {
            return new IndexScanOperationImpl(
                    ndbTransaction.getSelectIndexScanOperation(storeIndex.getName(), storeTable.getName()), this);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public Operation getSelectOperation(Table storeTable) {
        try {
            return new OperationImpl(ndbTransaction.getSelectOperation(storeTable.getName()), this);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public ScanOperation getSelectScanOperation(Table storeTable) {
        try {
            return new ScanOperationImpl(
                    ndbTransaction.getSelectScanOperation(storeTable.getName()), this);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public ScanOperation getSelectScanOperationLockModeExclusiveScanFlagKeyInfo(Table storeTable) {
        try {
            return new ScanOperationImpl(
                    ndbTransaction.getSelectScanOperation(storeTable.getName(),
                    NdbOperation.LockMode.LM_Exclusive,
                    NdbScanOperation.ScanFlag.KEY_INFO, 0,0), this);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public IndexOperation getSelectUniqueOperation(Index storeIndex, Table storeTable) {
        try {
            return new IndexOperationImpl(
                    ndbTransaction.getSelectUniqueOperation(storeIndex.getName(), storeTable.getName()), this);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public Operation getUpdateOperation(Table storeTable) {
        try {
            return new OperationImpl(ndbTransaction.getUpdateOperation(storeTable.getName()), this);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public Operation getWriteOperation(Table storeTable) {
        try {
            return new OperationImpl(ndbTransaction.getWriteOperation(storeTable.getName()), this);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void postExecuteCallback(Runnable callback) {
        postExecuteCallbacks.add(callback);
    }

    private void clearPostExecuteCallbacks() {
        postExecuteCallbacks.clear();
    }

    private void handlePendingPostExecuteCallbacks() {
        // if any pending postExecuteCallbacks, flush via executeNoCommit
        if (!postExecuteCallbacks.isEmpty()) {
            executeNoCommit();
        }
    }

    private void performPostExecuteCallbacks() {
        try {
            for (Runnable runnable: postExecuteCallbacks) {
                try {
                    runnable.run();
                } catch (Throwable t) {
                    throw new ClusterJDatastoreException(
                            local.message("ERR_Datastore"), t);
                }
            }
        } finally {
            clearPostExecuteCallbacks();
        }
    }

}