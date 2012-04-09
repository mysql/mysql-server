/*
 *  Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
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
import com.mysql.clusterj.LockMode;

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
import com.mysql.clusterj.tie.DbImpl.BufferManager;

import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbIndexOperation;
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbOperation;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbTransaction;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.IndexConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst.AbortOption;
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

    /** The NdbDictionary */
    private Dictionary ndbDictionary;

    /** The coordinated transaction identifier */
    private String coordinatedTransactionId = null;

    /** Is getCoordinatedTransactionId supported? True until proven false. */
    private static boolean supportsGetCoordinatedTransactionId = true;

    /** Lock mode for find operations */
    private int findLockMode = com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead;

    /** Lock mode for index lookup operations */
    private int lookupLockMode = com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead;

    /** Lock mode for index scan operations */
    private int indexScanLockMode = com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead;

    /** Lock mode for table scan operations */
    private int tableScanLockMode = com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead;

    /** Autocommit flag if we are in an autocommit transaction */
    private boolean autocommit = false;

    /** Autocommitted flag if we autocommitted early */
    private boolean autocommitted = false;

    /** The transaction id to join this transaction to */
    private String joinTransactionId;

    private BufferManager bufferManager;

    public ClusterTransactionImpl(DbImpl db, Dictionary ndbDictionary, String joinTransactionId) {
        this.db = db;
        this.ndbDictionary = ndbDictionary;
        this.joinTransactionId = joinTransactionId;
        this.bufferManager = db.getBufferManager();
    }

    public void close() {
        if (ndbTransaction != null) {
            ndbTransaction.close();
            ndbTransaction = null;
        }
    }

    public void executeCommit() {
        executeCommit(true, true);
    }

    public boolean isEnlisted() {
        return ndbTransaction != null;
    }

    /**
     * Enlist the ndb transaction if not already enlisted.
     * If the coordinated transaction id is set, join an existing transaction.
     * Otherwise, use the partition key to enlist the transaction.
     */
    private void enlist() {
        if (ndbTransaction == null) {
            if (coordinatedTransactionId != null) {
                ndbTransaction = db.joinTransaction(coordinatedTransactionId);
            } else {
                ndbTransaction = partitionKey.enlist(db);
                getCoordinatedTransactionId(db);
            }
        }
    }

    public void executeCommit(boolean abort, boolean force) {
        if (logger.isTraceEnabled()) logger.trace("");
        // nothing to do if no ndbTransaction was ever enlisted or already autocommitted
        if (isEnlisted() && !autocommitted) {
            handlePendingPostExecuteCallbacks();
            int abortOption = abort?AbortOption.AbortOnError:AbortOption.AO_IgnoreError;
            int forceOption = force?1:0;
            int returnCode = ndbTransaction.execute(NdbTransaction.ExecType.Commit,
                    abortOption, forceOption);
            handleError(returnCode, ndbTransaction);
        }
        autocommitted = false;
        autocommit = false;
    }

    public void executeNoCommit() {
        executeNoCommit(true, true);
    }

    public void executeNoCommit(boolean abort, boolean force) {
        if (logger.isTraceEnabled()) logger.trace("");
        if (!isEnlisted()) {
            // nothing to do if no ndbTransaction was ever enlisted
            return;
        }
        if (autocommit && postExecuteCallbacks.size() == 0) {
            // optimization to commit now because no blob columns
            executeCommit(abort, force);
            autocommitted = true;
            return;
        }
        int abortOption = abort?AbortOption.AbortOnError:AbortOption.AO_IgnoreError;
        int forceOption = force?1:0;
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
        TableConst ndbTable = ndbDictionary.getTable(storeTable.getName());
        handleError(ndbTable, ndbDictionary);
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int returnCode = ndbOperation.deleteTuple();
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());;
        return new OperationImpl(ndbOperation, this);
    }

    public Operation getInsertOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ndbDictionary.getTable(storeTable.getName());
        handleError(ndbTable, ndbDictionary);
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int returnCode = ndbOperation.insertTuple();
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new OperationImpl(ndbOperation, this);
    }

    public IndexScanOperation getIndexScanOperation(Index storeIndex, Table storeTable) {
        enlist();
        IndexConst ndbIndex = ndbDictionary.getIndex(storeIndex.getInternalName(), storeTable.getName());
        handleError(ndbIndex, ndbDictionary);
        NdbIndexScanOperation ndbOperation = ndbTransaction.getNdbIndexScanOperation(ndbIndex);
        handleError(ndbOperation, ndbTransaction);
        int lockMode = indexScanLockMode;
        int scanFlags = 0;
        int parallel = 0;
        int batch = 0;
        int returnCode = ndbOperation.readTuples(lockMode, scanFlags, parallel, batch);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName() + " index: " + storeIndex.getName());
        return new IndexScanOperationImpl(storeTable, ndbOperation, this);
    }

    public IndexScanOperation getIndexScanOperationMultiRange(Index storeIndex, Table storeTable) {
        enlist();
        IndexConst ndbIndex = ndbDictionary.getIndex(storeIndex.getInternalName(), storeTable.getName());
        handleError(ndbIndex, ndbDictionary);
        NdbIndexScanOperation ndbOperation = ndbTransaction.getNdbIndexScanOperation(ndbIndex);
        handleError(ndbOperation, ndbTransaction);
        int lockMode = indexScanLockMode;
        int scanFlags = ScanFlag.SF_MultiRange;
        int parallel = 0;
        int batch = 0;
        int returnCode = ndbOperation.readTuples(lockMode, scanFlags, parallel, batch);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName() + " index: " + storeIndex.getName());
        return new IndexScanOperationImpl(storeTable, ndbOperation, this);
    }

    public IndexScanOperation getIndexScanOperationLockModeExclusiveScanFlagKeyInfo(Index storeIndex, Table storeTable) {
        enlist();
        IndexConst ndbIndex = ndbDictionary.getIndex(storeIndex.getInternalName(), storeTable.getName());
        handleError(ndbIndex, ndbDictionary);
        NdbIndexScanOperation ndbOperation = ndbTransaction.getNdbIndexScanOperation(ndbIndex);
        handleError(ndbOperation, ndbTransaction);
        int lockMode = com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_Exclusive;
        int scanFlags = ScanFlag.SF_KeyInfo;
        int parallel = 0;
        int batch = 0;
        int returnCode = ndbOperation.readTuples(lockMode, scanFlags, parallel, batch);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName() + " index: " + storeIndex.getName());
        return new IndexScanOperationImpl(storeTable, ndbOperation, this);
    }

    public Operation getSelectOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ndbDictionary.getTable(storeTable.getName());
        handleError(ndbTable, ndbDictionary);
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int lockMode = findLockMode;
        int returnCode = ndbOperation.readTuple(lockMode);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new OperationImpl(storeTable, ndbOperation, this);
    }

    public ScanOperation getTableScanOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ndbDictionary.getTable(storeTable.getName());
        handleError(ndbTable, ndbDictionary);
        NdbScanOperation ndbScanOperation = ndbTransaction.getNdbScanOperation(ndbTable);
        handleError(ndbScanOperation, ndbTransaction);
        int lockMode = tableScanLockMode;
        int scanFlags = 0;
        int parallel = 0;
        int batch = 0;
        int returnCode = ndbScanOperation.readTuples(lockMode, scanFlags, parallel, batch);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new ScanOperationImpl(storeTable, ndbScanOperation, this);
    }

    public ScanOperation getTableScanOperationLockModeExclusiveScanFlagKeyInfo(Table storeTable) {
        enlist();
        TableConst ndbTable = ndbDictionary.getTable(storeTable.getName());
        handleError(ndbTable, ndbDictionary);
        NdbScanOperation ndbScanOperation = ndbTransaction.getNdbScanOperation(ndbTable);
        handleError(ndbScanOperation, ndbTransaction);
        int lockMode = com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_Exclusive;
        int scanFlags = ScanFlag.SF_KeyInfo;
        int parallel = 0;
        int batch = 0;
        int returnCode = ndbScanOperation.readTuples(lockMode, scanFlags, parallel, batch);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new ScanOperationImpl(storeTable, ndbScanOperation, this);
    }

    public IndexOperation getUniqueIndexOperation(Index storeIndex, Table storeTable) {
        enlist();
        IndexConst ndbIndex = ndbDictionary.getIndex(storeIndex.getInternalName(), storeTable.getName());
        handleError(ndbIndex, ndbDictionary);
        NdbIndexOperation ndbIndexOperation = ndbTransaction.getNdbIndexOperation(ndbIndex);
        handleError(ndbIndexOperation, ndbTransaction);
        int lockMode = lookupLockMode;
        int returnCode = ndbIndexOperation.readTuple(lockMode);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName() + " index: " + storeIndex.getName());
        return new IndexOperationImpl(storeTable, ndbIndexOperation, this);
    }

    public IndexOperation getUniqueIndexDeleteOperation(Index storeIndex, Table storeTable) {
        enlist();
        IndexConst ndbIndex = ndbDictionary.getIndex(storeIndex.getInternalName(), storeTable.getName());
        handleError(ndbIndex, ndbDictionary);
        NdbIndexOperation ndbIndexOperation = ndbTransaction.getNdbIndexOperation(ndbIndex);
        handleError(ndbIndexOperation, ndbTransaction);
        int returnCode = ndbIndexOperation.deleteTuple();
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName() + " index: " + storeIndex.getName());
        return new IndexOperationImpl(storeTable, ndbIndexOperation, this);
    }

    public Operation getUpdateOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ndbDictionary.getTable(storeTable.getName());
        handleError(ndbTable, ndbDictionary);
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int returnCode = ndbOperation.updateTuple();
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new OperationImpl(storeTable, ndbOperation, this);
    }

    public Operation getWriteOperation(Table storeTable) {
        enlist();
        TableConst ndbTable = ndbDictionary.getTable(storeTable.getName());
        handleError(ndbTable, ndbDictionary);
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int returnCode = ndbOperation.writeTuple();
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        return new OperationImpl(storeTable, ndbOperation, this);
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
            if (ndbError.code() == 0) {
                return;
            }
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

    protected void handleError(Object object, Dictionary ndbDictionary) {
        if (object != null) {
            return;
        } else {
            NdbErrorConst ndbError = ndbDictionary.getNdbError();
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

    public String getCoordinatedTransactionId() {
        return coordinatedTransactionId;
    }

    /** Get the coordinated transaction id if possible and update the field with
     * the id. If running on a back level system (prior to 7.1.6 for the ndbjtie
     * and native library) the ndbTransaction.getCoordinatedTransactionId() method
     * will throw an Error of some kind (java.lang.NoSuchMethodError or
     * java.lang.UnsatisfiedLinkError) and this will cause this instance
     * (and any other instance with access to the new value of the static variable 
     * supportsGetCoordinatedTransactionId) to never try again.
     * @param db the DbImpl instance
     */
    private void getCoordinatedTransactionId(DbImpl db) {
        try {
            if (supportsGetCoordinatedTransactionId) {
// not implemented quite yet...
//                ByteBuffer buffer = db.getCoordinatedTransactionIdBuffer();
//                coordinatedTransactionId = ndbTransaction.
//                        getCoordinatedTransactionId(buffer, buffer.capacity());
                if (logger.isDetailEnabled()) logger.detail("CoordinatedTransactionId: "
                        + coordinatedTransactionId);
                throw new ClusterJFatalInternalException("Not Implemented");
            }
        } catch (Throwable t) {
            // oops, don't do this again
            supportsGetCoordinatedTransactionId = false;
        }
    }

    public void setCoordinatedTransactionId(String coordinatedTransactionId) {
        this.coordinatedTransactionId = coordinatedTransactionId;
    }

    public void setLockMode(LockMode lockmode) {
        findLockMode = translateLockMode(lockmode);
        lookupLockMode = findLockMode;
        indexScanLockMode = findLockMode;
        tableScanLockMode = findLockMode;
    }

    private int translateLockMode(LockMode lockmode) {
        switch(lockmode) {
            case READ_COMMITTED:
                return com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead;
            case SHARED:
                return com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_Read;
            case EXCLUSIVE:
                return com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_Exclusive;
            default:
                throw new ClusterJFatalInternalException(local.message("ERR_Unknown_Lock_Mode", lockmode));
        }
    }

    public void setAutocommit(boolean autocommit) {
        this.autocommit = autocommit;
    }

    public BufferManager getBufferManager() {
        return bufferManager;
    }

}
