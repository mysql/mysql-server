/*
 *  Copyright (C) 2009-2010 Sun Microsystems, Inc.
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

import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.ClusterTransaction;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.IndexOperation;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbIndexOperation;
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbOperation;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbTransaction;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.IndexConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst.AbortOption;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation.ScanFlag;

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

    /** The DbImpl associated with this NdbTransaction */
    protected DbImpl db;

    /** The partition key; by default it doesn't do anything */
    protected PartitionKeyImpl partitionKey = PartitionKeyImpl.getInstance();

    public ClusterTransactionImpl(DbImpl db) {
        this.db = db;
    }

    public void close() {
        if (ndbTransaction != null) {
            ndbTransaction.close();
            ndbTransaction = null;
        }
    }

    public void executeCommit() {
        executeCommit(true, false);
    }

    public boolean isEnlisted() {
        return ndbTransaction != null;
    }

    private void enlist() {
        if (ndbTransaction == null) {
            ndbTransaction = partitionKey.enlist(db);
        }
    }

    public void executeCommit(boolean abort, boolean force) {
        if (logger.isTraceEnabled()) logger.trace("");
        if (!isEnlisted()) {
            // nothing to do if no ndbTransaction was ever enlisted
            return;
        }
        handlePendingPostExecuteCallbacks();
        int abortOption = abort?AbortOption.AbortOnError:AbortOption.AO_IgnoreError;
        int forceOption = force?0:1;
        int returnCode = ndbTransaction.execute(NdbTransaction.ExecType.Commit,
                abortOption, forceOption);
        handleError(returnCode, ndbTransaction);
    }

    public void executeNoCommit() {
        executeNoCommit(true, false);
    }

    public void executeNoCommit(boolean abort, boolean force) {
        if (logger.isTraceEnabled()) logger.trace("");
        if (!isEnlisted()) {
            // nothing to do if no ndbTransaction was ever enlisted
            return;
        }
        int abortOption = abort?AbortOption.AbortOnError:AbortOption.AO_IgnoreError;
        int forceOption = force?0:1;
        int returnCode = ndbTransaction.execute(NdbTransaction.ExecType.NoCommit,
                abortOption, forceOption);
        handleError(returnCode, ndbTransaction);
        performPostExecuteCallbacks();
    }

    public void executeRollback() {
        if (!isEnlisted()) {
            // nothing to do if no ndbTransaction was ever enlisted
            return;
        }
        int abortOption = AbortOption.AO_IgnoreError;
        int forceOption = 1;
        int returnCode = ndbTransaction.execute(NdbTransaction.ExecType.Rollback,
                abortOption, forceOption);
        handleError(returnCode, ndbTransaction);
    }

    public Operation getDeleteOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ((TableImpl)storeTable).getNdbTable();
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int returnCode = ndbOperation.deleteTuple();
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());;
        return new OperationImpl(ndbOperation, this);
    }

    public Operation getInsertOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ((TableImpl)storeTable).getNdbTable();
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int returnCode = ndbOperation.insertTuple();
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new OperationImpl(ndbOperation, this);
    }

    public IndexScanOperation getSelectIndexScanOperation(Index storeIndex, Table storeTable) {
        enlist();
        IndexConst ndbIndex = ((IndexImpl)storeIndex).getNdbIndex();
        NdbIndexScanOperation ndbOperation = ndbTransaction.getNdbIndexScanOperation(ndbIndex);
        handleError(ndbOperation, ndbTransaction);
        int lockMode = 0;
        int scanFlags = 0;
        int parallel = 0;
        int batch = 0;
        int returnCode = ndbOperation.readTuples(lockMode, scanFlags, parallel, batch);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName() + " index: " + storeIndex.getName());
        return new IndexScanOperationImpl(ndbOperation, this);
    }

    public Operation getSelectOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ((TableImpl)storeTable).getNdbTable();
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int lockMode = LockMode.LM_Read;
        int returnCode = ndbOperation.readTuple(lockMode);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new OperationImpl(ndbOperation, this);
    }

    public ScanOperation getSelectScanOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ((TableImpl)storeTable).getNdbTable();
        NdbScanOperation ndbScanOperation = ndbTransaction.getNdbScanOperation(ndbTable);
        handleError(ndbScanOperation, ndbTransaction);
        int lockMode = LockMode.LM_Read;
        int scanFlags = 0;
        int parallel = 0;
        int batch = 0;
        int returnCode = ndbScanOperation.readTuples(lockMode, scanFlags, parallel, batch);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new ScanOperationImpl(ndbScanOperation, this);
    }

    public ScanOperation getSelectScanOperationLockModeExclusiveScanFlagKeyInfo(Table storeTable) {
        enlist();
        TableConst ndbTable = ((TableImpl)storeTable).getNdbTable();
        NdbScanOperation ndbScanOperation = ndbTransaction.getNdbScanOperation(ndbTable);
        handleError(ndbScanOperation, ndbTransaction);
        int lockMode = LockMode.LM_Exclusive;
        int scanFlags = ScanFlag.SF_KeyInfo;
        int parallel = 0;
        int batch = 0;
        int returnCode = ndbScanOperation.readTuples(lockMode, scanFlags, parallel, batch);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new ScanOperationImpl(ndbScanOperation, this);
    }

    public IndexOperation getSelectUniqueOperation(Index storeIndex, Table storeTable) {
        enlist();
        IndexConst index = ((IndexImpl)storeIndex).getNdbIndex();
        NdbIndexOperation ndbIndexOperation = ndbTransaction.getNdbIndexOperation(index);
        handleError(ndbIndexOperation, ndbTransaction);
        int lockMode = LockMode.LM_Read;
        int returnCode = ndbIndexOperation.readTuple(lockMode);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName() + " index: " + storeIndex.getName());
        return new IndexOperationImpl(ndbIndexOperation, this);
    }

    public Operation getUpdateOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ((TableImpl)storeTable).getNdbTable();
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int returnCode = ndbOperation.updateTuple();
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new OperationImpl(ndbOperation, this);
    }

    public Operation getWriteOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ((TableImpl)storeTable).getNdbTable();
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int returnCode = ndbOperation.writeTuple();
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new OperationImpl(ndbOperation, this);
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
        // TODO this will abort on the first postExecute failure
        // TODO should this set rollback only?
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

    /** Handle errors from ScanOperation where the error returnCode is -1.
     * 
     * @param returnCode the return code from the nextResult operation
     */
    protected void handleError(int returnCode) {
        if (returnCode == -1) {
            NdbErrorConst ndbError = ndbTransaction.getNdbError();
            String detail = db.getNdbErrorDetail(ndbError);
            Utility.throwError(returnCode, ndbError, detail);
        }
    }

    protected void handleError(int returnCode, NdbTransaction ndbTransaction) {
        if (returnCode == 0) {
            return;
        } else {
            NdbErrorConst ndbError = ndbTransaction.getNdbError();
            String detail = db.getNdbErrorDetail(ndbError);
            Utility.throwError(returnCode, ndbError, detail);
        }
    }

    protected void handleError(Object object, NdbTransaction ndbTransaction) {
        if (object != null) {
            return;
        } else {
            NdbErrorConst ndbError = ndbTransaction.getNdbError();
            String detail = db.getNdbErrorDetail(ndbError);
            Utility.throwError(null, ndbError, detail);
        }
    }

    public void setPartitionKey(PartitionKey partitionKey) {
        if (partitionKey == null) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Partition_Key_Null"));
        }
        this.partitionKey = (PartitionKeyImpl)partitionKey;
    }

}
