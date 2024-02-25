/*
 *  Copyright (c) 2009, 2023, Oracle and/or its affiliates.
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
import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJHelper;
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
import com.mysql.ndbjtie.ndbapi.NdbErrorConst.Classification;
import com.mysql.ndbjtie.ndbapi.NdbIndexOperation;
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbOperation;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst;
import com.mysql.ndbjtie.ndbapi.NdbRecordConst;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbTransaction;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.IndexConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;
import com.mysql.ndbjtie.ndbapi.NdbOperation.OperationOptionsConst;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst.AbortOption;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation.ScanFlag;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation.ScanOptions;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation.ScanOptionsConst;

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

    protected final static String USE_NDBRECORD_NAME = "com.mysql.clusterj.UseNdbRecord";
    private static boolean USE_NDBRECORD = ClusterJHelper.getBooleanProperty(USE_NDBRECORD_NAME, "true");

    protected NdbTransaction ndbTransaction;
    private List<Runnable> postExecuteCallbacks = new ArrayList<Runnable>();

    /** The cluster connection for this transaction */
    protected ClusterConnectionImpl clusterConnectionImpl;

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

    private List<Operation> operationsToCheck = new ArrayList<Operation>();

    public ClusterTransactionImpl(ClusterConnectionImpl clusterConnectionImpl,
            DbImpl db, Dictionary ndbDictionary, String joinTransactionId) {
        this.db = db;
        this.clusterConnectionImpl = clusterConnectionImpl;
        this.ndbDictionary = ndbDictionary;
        this.joinTransactionId = joinTransactionId;
        this.bufferManager = db.getBufferManager();
    }

    public ClusterConnectionImpl getClusterConnection() {
        return this.clusterConnectionImpl;
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
        db.assertNotClosed("ClusterTransactionImpl.enlist");
        if (logger.isTraceEnabled()) logger.trace("ndbTransaction: " + ndbTransaction
                + " with joinTransactionId: " + joinTransactionId);
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
        db.assertNotClosed("ClusterTransactionImpl.executeCommit");
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
        db.assertNotClosed("ClusterTransactionImpl.executeNoCommit");
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
        db.assertNotClosed("ClusterTransactionImpl.executeRollback");
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
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        if (USE_NDBRECORD) {
            return new NdbRecordDeleteOperationImpl(this, storeTable);
        }
        TableConst ndbTable = ndbDictionary.getTable(storeTable.getName());
        handleError(ndbTable, ndbDictionary);
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int returnCode = ndbOperation.deleteTuple();
        handleError(returnCode, ndbTransaction);
        return new OperationImpl(ndbOperation, this);
    }

    public Operation getInsertOperation(Table storeTable) {
        enlist();
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName());
        if (USE_NDBRECORD) {
            return new NdbRecordInsertOperationImpl(this, storeTable);
        }
        TableConst ndbTable = ndbDictionary.getTable(storeTable.getName());
        handleError(ndbTable, ndbDictionary);
        NdbOperation ndbOperation = ndbTransaction.getNdbOperation(ndbTable);
        handleError(ndbOperation, ndbTransaction);
        int returnCode = ndbOperation.insertTuple();
        handleError(returnCode, ndbTransaction);
        return new OperationImpl(ndbOperation, this);
    }

    public IndexScanOperation getIndexScanOperation(Index storeIndex, Table storeTable) {
        enlist();
        if (USE_NDBRECORD) {
            return new NdbRecordIndexScanOperationImpl(this, storeIndex, storeTable, indexScanLockMode);
        }
        IndexConst ndbIndex = ndbDictionary.getIndex(storeIndex.getInternalName(), storeTable.getName());
        handleError(ndbIndex, ndbDictionary);
        NdbIndexScanOperation ndbOperation = ndbTransaction.getNdbIndexScanOperation(ndbIndex);
        handleError(ndbOperation, ndbTransaction);
        int scanFlags = 0;
        int lockMode = indexScanLockMode;
        if (lockMode != com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead) {
            scanFlags = ScanFlag.SF_KeyInfo;
        }
        int parallel = 0;
        int batch = 0;
        int returnCode = ndbOperation.readTuples(lockMode, scanFlags, parallel, batch);
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName() + " index: " + storeIndex.getName());
        return new IndexScanOperationImpl(storeTable, ndbOperation, this);
    }

    public IndexScanOperation getIndexScanOperationMultiRange(Index storeIndex, Table storeTable) {
        enlist();
        if (USE_NDBRECORD) {
            return new NdbRecordIndexScanOperationImpl(this, storeIndex, storeTable, true, indexScanLockMode);
        }
        IndexConst ndbIndex = ndbDictionary.getIndex(storeIndex.getInternalName(), storeTable.getName());
        handleError(ndbIndex, ndbDictionary);
        NdbIndexScanOperation ndbOperation = ndbTransaction.getNdbIndexScanOperation(ndbIndex);
        handleError(ndbOperation, ndbTransaction);
        int scanFlags = ScanFlag.SF_OrderBy;
        int lockMode = indexScanLockMode;
        if (lockMode != com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead) {
            scanFlags |= ScanFlag.SF_KeyInfo;
        }
        scanFlags |= ScanFlag.SF_MultiRange;
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
        if (USE_NDBRECORD) {
            return new NdbRecordKeyOperationImpl(this, storeTable);
        }
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
        if (USE_NDBRECORD) {
            return new NdbRecordTableScanOperationImpl(this, storeTable, tableScanLockMode);
        }
        TableConst ndbTable = ndbDictionary.getTable(storeTable.getName());
        handleError(ndbTable, ndbDictionary);
        NdbScanOperation ndbScanOperation = ndbTransaction.getNdbScanOperation(ndbTable);
        handleError(ndbScanOperation, ndbTransaction);
        int lockMode = tableScanLockMode;
        int scanFlags = 0;
        if (lockMode != com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead) {
            scanFlags = ScanFlag.SF_KeyInfo;
        }
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
        if (USE_NDBRECORD) {
            return new NdbRecordUniqueKeyOperationImpl(this, storeIndex, storeTable);
        }
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

    public IndexOperation getUniqueIndexUpdateOperation(Index storeIndex, Table storeTable) {
        enlist();
        IndexConst ndbIndex = ndbDictionary.getIndex(storeIndex.getInternalName(), storeTable.getName());
        handleError(ndbIndex, ndbDictionary);
        NdbIndexOperation ndbIndexOperation = ndbTransaction.getNdbIndexOperation(ndbIndex);
        handleError(ndbIndexOperation, ndbTransaction);
        int returnCode = ndbIndexOperation.updateTuple();
        handleError(returnCode, ndbTransaction);
        if (logger.isTraceEnabled()) logger.trace("Table: " + storeTable.getName() + " index: " + storeIndex.getName());
        return new IndexOperationImpl(storeTable, ndbIndexOperation, this);
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

    /** Create an NdbOperation for insert using NdbRecord.
     * 
     * @param ndbRecord the NdbRecord
     * @param buffer the buffer with data for the operation
     * @param mask the mask of column values already set in the buffer
     * @param options the OperationOptions for this operation
     * @return the insert operation
     */
    public NdbOperationConst insertTuple(NdbRecordConst ndbRecord,
            ByteBuffer buffer, byte[] mask, OperationOptionsConst options) {
        enlist();
        NdbOperationConst operation = ndbTransaction.insertTuple(ndbRecord, buffer, mask, options, 0);
        handleError(operation, ndbTransaction);
        return operation;
    }

    /** Create a table scan operation using NdbRecord.
     * 
     * @param ndbRecord the NdbRecord for the result
     * @param mask the columns to read
     * @param options the scan options
     * @return
     */
    public NdbScanOperation scanTable(NdbRecordConst ndbRecord, byte[] mask, ScanOptionsConst options) {
        enlist();
        int lockMode = tableScanLockMode;
        NdbScanOperation operation = ndbTransaction.scanTable(ndbRecord, lockMode, mask, options, 0);
        handleError(operation, ndbTransaction);
        return operation;
    }

    /** Create a scan operation on the index using NdbRecord. 
     * 
     * @param ndbRecord the ndb record
     * @param mask the mask that specifies which columns to read
     * @param object scan options // TODO change this
     * @return
     */
    public NdbIndexScanOperation scanIndex(NdbRecordConst key_record, NdbRecordConst result_record,
            byte[] result_mask, ScanOptions scanOptions) {
        enlist();
        NdbIndexScanOperation operation =
                ndbTransaction.scanIndex(key_record, result_record, indexScanLockMode, result_mask, null, scanOptions, 0);
        handleError(operation, ndbTransaction);
        return operation;
    }

    /** Create an NdbOperation for delete using NdbRecord.
     * 
     * @param ndbRecord the NdbRecord
     * @param buffer the buffer with data for the operation
     * @param mask the mask of column values already set in the buffer
     * @param options the OperationOptions for this operation
     * @return the delete operation
     */
    public NdbOperationConst deleteTuple(NdbRecordConst ndbRecord,
            ByteBuffer buffer, byte[] mask, OperationOptionsConst options) {
        enlist();
        NdbOperationConst operation = ndbTransaction.deleteTuple(ndbRecord, buffer, ndbRecord, null, mask, options, 0);
        handleError(operation, ndbTransaction);
        return operation;
    }

    /** Create an NdbOperation for update using NdbRecord.
     * 
     * @param ndbRecord the NdbRecord
     * @param buffer the buffer with data for the operation
     * @param mask the mask of column values already set in the buffer
     * @param options the OperationOptions for this operation
     * @return the update operation
     */
    public NdbOperationConst updateTuple(NdbRecordConst ndbRecord,
            ByteBuffer buffer, byte[] mask, OperationOptionsConst options) {
        enlist();
        NdbOperationConst operation = ndbTransaction.updateTuple(ndbRecord, buffer, ndbRecord, buffer, mask, options, 0);
        handleError(operation, ndbTransaction);
        return operation;
    }

    /** Create an NdbOperation for write using NdbRecord.
     * 
     * @param ndbRecord the NdbRecord
     * @param buffer the buffer with data for the operation
     * @param mask the mask of column values already set in the buffer
     * @param options the OperationOptions for this operation
     * @return the update operation
     */
    public NdbOperationConst writeTuple(NdbRecordConst ndbRecord,
            ByteBuffer buffer, byte[] mask, OperationOptionsConst options) {
        enlist();
        NdbOperationConst operation = ndbTransaction.writeTuple(ndbRecord, buffer, ndbRecord, buffer, mask, options, 0);
        handleError(operation, ndbTransaction);
        return operation;
    }

    /** Create an NdbOperation for key read using NdbRecord. The 'find' lock mode is used.
     * 
     * @param ndbRecordKeys the NdbRecord for the key
     * @param keyBuffer the buffer with the key for the operation
     * @param ndbRecordValues the NdbRecord for the value
     * @param valueBuffer the buffer with the value returned by the operation
     * @param mask the mask of column values to be read
     * @param options the OperationOptions for this operation
     * @return the ndb operation for key read
     */
    public NdbOperationConst readTuple(NdbRecordConst ndbRecordKeys, ByteBuffer keyBuffer,
            NdbRecordConst ndbRecordValues, ByteBuffer valueBuffer,
            byte[] mask, OperationOptionsConst options) {
        enlist();
        NdbOperationConst operation = ndbTransaction.readTuple(ndbRecordKeys, keyBuffer, 
                ndbRecordValues, valueBuffer, findLockMode, mask, options, 0);
        handleError(operation, ndbTransaction);
        return operation;
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
            executeNoCommit(false, true);
        }
    }

    private void performPostExecuteCallbacks() {
        // check completed operations
        StringBuilder exceptionMessages = new StringBuilder();
        for (Operation op: operationsToCheck) {
            int code = op.getErrorCode();
            int classification = op.getClassification();
            // Read operations can return data not found errors.
            // Ignore them and report everything else.
            if (code != 0 &&
                !(op.isReadOperation() &&
                  classification == Classification.NoDataFound)) {
                int mysqlCode = op.getMysqlCode();
                int status = op.getStatus();
                String message = local.message("ERR_Datastore", -1, code, mysqlCode, status, classification,
                        op.toString());
                exceptionMessages.append(message);
                exceptionMessages.append('\n');
            }
        }
        operationsToCheck.clear();
        // TODO should this set rollback only?
        try {
            for (Runnable runnable: postExecuteCallbacks) {
                try {
                    runnable.run();
                } catch (Throwable t) {
                    t.printStackTrace();
                    exceptionMessages.append(t.getMessage());
                    exceptionMessages.append('\n');
                }
            }
        } finally {
            clearPostExecuteCallbacks();
        }
        if (exceptionMessages.length() > 0) {
            throw new ClusterJDatastoreException(exceptionMessages.toString());
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

    /** Get the cached NdbRecordImpl for this table. The NdbRecordImpl is cached in the
     * cluster connection.
     * @param storeTable the table
     * @return
     */
    protected NdbRecordImpl getCachedNdbRecordImpl(Table storeTable) {
        return clusterConnectionImpl.getCachedNdbRecordImpl(storeTable);
    }

    /** Get the cached NdbRecordImpl for this index and table. The NdbRecordImpl is cached in the
     * cluster connection.
     * @param storeTable the table
     * @param storeIndex the index
     * @return
     */
    protected NdbRecordImpl getCachedNdbRecordImpl(Index storeIndex, Table storeTable) {
        return clusterConnectionImpl.getCachedNdbRecordImpl(storeIndex, storeTable);
    }

    /** 
     * Add an operation to check for errors after execute.
     * @param op the operation to check
     */
    public void addOperationToCheck(Operation op) {
        operationsToCheck.add(op);
    }

}
