/*
   Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.core;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.LockMode;
import com.mysql.clusterj.Query;
import com.mysql.clusterj.Transaction;

import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.ValueHandler;

import com.mysql.clusterj.core.query.QueryDomainTypeImpl;
import com.mysql.clusterj.core.query.QueryBuilderImpl;
import com.mysql.clusterj.core.query.QueryImpl;

import com.mysql.clusterj.core.spi.SessionSPI;

import com.mysql.clusterj.core.store.ClusterTransaction;
import com.mysql.clusterj.core.store.Db;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.IndexOperation;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDefinition;
import com.mysql.clusterj.query.QueryDomainType;

import java.util.ArrayList;
import java.util.BitSet;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * This class implements Session, the main user interface to ClusterJ.
 * It also implements SessionSPI, the main component interface.
 */
public class SessionImpl implements SessionSPI, CacheManager, StoreManager {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(SessionImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(SessionImpl.class);

    /** My Factory. */
    protected SessionFactoryImpl factory;

    /** Db: one per session. */
    protected Db db;

    /** Dictionary: one per session. */
    protected Dictionary dictionary;

    /** One transaction at a time. */
    protected TransactionImpl transactionImpl;

    /** The partition key */
    protected PartitionKey partitionKey = null;

    /** Rollback only status */
    protected boolean rollbackOnly = false;

    /** The underlying ClusterTransaction */
    protected ClusterTransaction clusterTransaction;

    /** The transaction id to join */
    protected String joinTransactionId = null;

    /** The properties for this session */
    protected Map properties;

    /** Flags for iterating a scan */
    protected final int RESULT_READY = 0;
    protected final int SCAN_FINISHED = 1;
    protected final int CACHE_EMPTY = 2;

    /** The list of objects changed since the last flush */
    protected List<StateManager> changeList = new ArrayList<StateManager>();

    /** The list of pending operations, such as load operations, that need to be
     * processed after the operation is sent to the database via @see #executeNoCommit().
     */
    protected List<Runnable> postExecuteOperations = new ArrayList<Runnable>();

    /** The transaction state of this session. */
    protected TransactionState transactionState;

    /** The exception state of an internal transaction. */
    private ClusterJException transactionException;

    /** Nested auto transaction counter. */
    protected int nestedAutoTransactionCounter = 0;

    /** Number of retries for retriable exceptions */
    // TODO get this from properties
    protected int numberOfRetries = 5;

    /** The lock mode for read operations */
    private LockMode lockmode = LockMode.READ_COMMITTED;

    /** Our post-execute callback handler */
    private Runnable postExecuteCallbackHandler = new Runnable() {
        public void run() {
            for (Runnable postExecuteCallback: postExecuteOperations) {
                postExecuteCallback.run();
            }
            postExecuteOperations.clear();
        }
    };

    /** Create a SessionImpl with factory, properties, Db, and dictionary
     */
    SessionImpl(SessionFactoryImpl factory, Map properties, 
            Db db, Dictionary dictionary) {
        this.factory = factory;
        this.db = db;
        this.dictionary = dictionary;
        this.properties = properties;
        transactionImpl = new TransactionImpl(this);
        transactionState = transactionStateNotActive;
    }

    /** Create a query from a query definition.
     * 
     * @param qd the query definition
     * @return the query
     */
    public <T> Query<T> createQuery(QueryDefinition<T> qd) {
        if (!(qd instanceof QueryDomainTypeImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Exception_On_Method", "createQuery"));
        }
        return new QueryImpl<T>(this, (QueryDomainTypeImpl<T>)qd);
    }

    /** Find an instance by its class and primary key.
     * If there is a compound primary key, the key is an Object[] containing
     * all of the primary key fields in order of declaration in annotations.
     * 
     * @param cls the class
     * @param key the primary key
     * @return the instance
     */
    public <T> T find(Class<T> cls, Object key) {
        DomainTypeHandler<T> domainTypeHandler = getDomainTypeHandler(cls);
        T instance = (T) factory.newInstance(cls, dictionary);
        ValueHandler keyHandler = domainTypeHandler.createKeyValueHandler(key);
        ValueHandler instanceHandler = domainTypeHandler.getValueHandler(instance);
        // initialize from the database using the key
        return (T) initializeFromDatabase(
                domainTypeHandler, instance, instanceHandler, keyHandler);
    }

    /** Initialize fields from the database. The keyHandler must
     * contain the primary keys, none of which can be null.
     * The instanceHandler, which may be null, manages the values
     * of the instance. If it is null, both the instanceHandler and the
     * instance are created if the instance exists in the database.
     * The instance, which may be null, is the domain instance that is
     * returned after loading the values from the database.
     *
     * @param domainTypeHandler domain type handler for the class
     * @param keyHandler the primary key handler
     * @param instanceHandler the handler for the instance
     * (may be null if not yet initialized)
     * @param instance the instance (may be null)
     * @return the instance with fields initialized from the database
     */
    public <T> T initializeFromDatabase(DomainTypeHandler<T> domainTypeHandler,
            T instance,
            ValueHandler instanceHandler, ValueHandler keyHandler) {
        startAutoTransaction();
        try {
            ResultData rs = selectUnique(domainTypeHandler, keyHandler, null);
            if (rs.next()) {
                // we have a result; initialize the instance
                if (instanceHandler == null) {
                    if (logger.isDetailEnabled()) logger.detail("Creating instanceHandler for class " + domainTypeHandler.getName() + " table: " + domainTypeHandler.getTableName() + keyHandler.pkToString(domainTypeHandler));
                    // we need both a new instance and its handler
                    instance = domainTypeHandler.newInstance();
                    instanceHandler = domainTypeHandler.getValueHandler(instance);
                } else if (instance == null) {
                if (logger.isDetailEnabled()) logger.detail("Creating instance for class " + domainTypeHandler.getName() + " table: " + domainTypeHandler.getTableName() + keyHandler.pkToString(domainTypeHandler));
                    // we have a handler but no instance
                    instance = domainTypeHandler.getInstance(instanceHandler);
                }
                // found the instance in the datastore
                instanceHandler.found(Boolean.TRUE);
                // put the results into the instance
                domainTypeHandler.objectSetValues(rs, instanceHandler);
                // set the cache manager to track updates
                domainTypeHandler.objectSetCacheManager(this, instanceHandler);
                // reset modified bits in instance
                domainTypeHandler.objectResetModified(instanceHandler);
            } else {
                if (logger.isDetailEnabled()) logger.detail("No instance found in database for class " + domainTypeHandler.getName() + " table: " + domainTypeHandler.getTableName() + keyHandler.pkToString(domainTypeHandler));
                // no instance found in database
                if (instanceHandler != null) {
                    // mark the handler as not found
                    instanceHandler.found(Boolean.FALSE);
                }
                endAutoTransaction();
                return null;
            }
        } catch (ClusterJException ex) {
            failAutoTransaction();
            throw ex;
        }
        endAutoTransaction();
        return instance;
    }

    /** If a transaction is already enlisted, ignore. Otherwise, set
     * the partition key based on the key handler.
     * @param domainTypeHandler the domain type handler
     * @param keyHandler the value handler that holds the key values
     */
    private void setPartitionKey(DomainTypeHandler<?> domainTypeHandler,
            ValueHandler keyHandler) {
        if (!isEnlisted()) {
            // there is still time to set the partition key
            PartitionKey partitionKey = 
                domainTypeHandler.createPartitionKey(keyHandler);
            clusterTransaction.setPartitionKey(partitionKey);
        }
    }

    /** Create an instance of a class to be persisted.
     * 
     * @param cls the class
     * @return a new instance that can be used with makePersistent
     */
    public <T> T newInstance(Class<T> cls) {
        return factory.newInstance(cls, dictionary);
    }

    /** Create an instance of a class to be persisted and set the primary key.
     * 
     * @param cls the class
     * @return a new instance that can be used with makePersistent,
     * savePersistent, writePersistent, updatePersistent, or deletePersistent
     */
    public <T> T newInstance(Class<T> cls, Object key) {
        DomainTypeHandler<T> domainTypeHandler = getDomainTypeHandler(cls);
        T instance = factory.newInstance(cls, dictionary);
        domainTypeHandler.objectSetKeys(key, instance);
        return instance;
    }

    /** Load the instance from the database into memory. Loading
     * is asynchronous and will be executed when an operation requiring
     * database access is executed: find, flush, or query. The instance must
     * have been returned from find or query; or
     * created via session.newInstance and its primary key initialized.
     * @param object the instance to load
     * @return the instance
     * @see #found(Object)
     */
    public <T> T load(final T object) {
        if (object == null) {
            return null;
        }
        if (Iterable.class.isAssignableFrom(object.getClass())) {
            Iterable<?> instances = (Iterable<?>)object;
            for (Object instance:instances) {
                load(instance);
            }
            return object;
        }
        if (object.getClass().isArray()) {
            Object[] instances = (Object[])object;
            for (Object instance:instances) {
                load(instance);
            }
            return object;
        }
        // a transaction must already be active (autocommit is not supported)
        assertActive();
        final DomainTypeHandler<?> domainTypeHandler = getDomainTypeHandler(object);
        final ValueHandler instanceHandler = domainTypeHandler.getValueHandler(object);
        setPartitionKey(domainTypeHandler, instanceHandler);
        Table storeTable = domainTypeHandler.getStoreTable();
        // perform a primary key operation
        final Operation op = clusterTransaction.getSelectOperation(storeTable);
        // set the keys into the operation
        domainTypeHandler.operationSetKeys(instanceHandler, op);
        // set the expected columns into the operation
        domainTypeHandler.operationGetValues(op);
        final ResultData rs = op.resultData(false);
        final SessionImpl cacheManager = this;
        // defer execution of the key operation until the next find, flush, or query
        Runnable postExecuteOperation = new Runnable() {
            public void run() {
                if (rs.next()) {
                    // found row in database
                    instanceHandler.found(Boolean.TRUE);
                   // put the results into the instance
                    domainTypeHandler.objectSetValues(rs, instanceHandler);
                    // set the cache manager to track updates
                    domainTypeHandler.objectSetCacheManager(cacheManager, instanceHandler);
                    // reset modified bits in instance
                    domainTypeHandler.objectResetModified(instanceHandler);
                } else {
                    // mark instance as not found
                    instanceHandler.found(Boolean.FALSE);
                }
                
            }
        };
        postExecuteOperations.add(postExecuteOperation);
        return object;
    }

    /** Was this instance found in the database?
     * @param instance the instance
     * @return <ul><li>null if the instance is null or was created via newInstance and never loaded;
     * </li><li>true if the instance was returned from a find or query
     * or created via newInstance and successfully loaded;
     * </li><li>false if the instance was created via newInstance and not found.
     * </li></ul>
     */
    public Boolean found(Object instance) {
        if (instance == null) {
            return null;
        }
        if (instance instanceof DynamicObject) {
            return ((DynamicObject)instance).found();
        }
        // make sure the instance is a persistent type
        getDomainTypeHandler(instance);
        return true;
    }
    
    /** Make an instance persistent. Also recursively make an iterable collection or array persistent.
     * 
     * @param object the instance or array or iterable collection of instances
     * @return the instance
     */
    public <T> T makePersistent(T object) {
        if (object == null) {
            return null;
        }
        if (Iterable.class.isAssignableFrom(object.getClass())) {
            startAutoTransaction();
            Iterable<?> instances = (Iterable<?>)object;
            for (Object instance:instances) {
                makePersistent(instance);
            }
            endAutoTransaction();
            return object;
        }
        if (object.getClass().isArray()) {
            startAutoTransaction();
            Object[] instances = (Object[])object;
            for (Object instance:instances) {
                makePersistent(instance);
            }
            endAutoTransaction();
            return object;
        }
        DomainTypeHandler<T> domainTypeHandler = getDomainTypeHandler(object);
        ValueHandler valueHandler = domainTypeHandler.getValueHandler(object);
        insert(domainTypeHandler, valueHandler);
        return object;
    }

    public Operation insert(
            DomainTypeHandler<?> domainTypeHandler, ValueHandler valueHandler) {
        startAutoTransaction();
        setPartitionKey(domainTypeHandler, valueHandler);
        Operation op = null;
        Table storeTable = null;
        try {
            storeTable = domainTypeHandler.getStoreTable();
            op = clusterTransaction.getInsertOperation(storeTable);
            // set all values in the operation, keys first
            domainTypeHandler.operationSetKeys(valueHandler, op);
            domainTypeHandler.operationSetModifiedNonPKValues(valueHandler, op);
            // reset modified bits in instance
            domainTypeHandler.objectResetModified(valueHandler);
        } catch (ClusterJUserException cjuex) {
            failAutoTransaction();
            throw cjuex;
        } catch (ClusterJException cjex) {
            failAutoTransaction();
            logger.error(local.message("ERR_Insert", storeTable.getName()));
            throw new ClusterJException(
                    local.message("ERR_Insert", storeTable.getName()), cjex);
        } catch (RuntimeException rtex) {
            failAutoTransaction();
            logger.error(local.message("ERR_Insert", storeTable.getName()));
            throw new ClusterJException(
                    local.message("ERR_Insert", storeTable.getName()), rtex);
        }
        endAutoTransaction();
        return op;
    }

    /** Make a number of instances persistent.
     * 
     * @param instances a Collection or array of objects to persist
     * @return a Collection or array with the same order of iteration
     */
    public Iterable makePersistentAll(Iterable instances) {
        startAutoTransaction();
        List<Object> result = new ArrayList<Object>();
        for (Object instance:instances) {
            result.add(makePersistent(instance));
            }
        endAutoTransaction();
        return result;
    }

    /** Delete an instance of a class from the database given its primary key.
     * For single-column keys, the key parameter is a wrapper (e.g. Integer).
     * For multi-column keys, the key parameter is an Object[] in which
     * elements correspond to the primary keys in order as defined in the schema.
     * @param cls the class
     * @param key the primary key
     */
    public <T> void deletePersistent(Class<T> cls, Object key) {
        DomainTypeHandler<T> domainTypeHandler = getDomainTypeHandler(cls);
        ValueHandler keyValueHandler = domainTypeHandler.createKeyValueHandler(key);
        delete(domainTypeHandler, keyValueHandler);
    }

    /** Remove an instance from the database. Only the key field(s) 
     * are used to identify the instance.
     * 
     * @param object the instance to remove from the database
     */
    public void deletePersistent(Object object) {
        if (object == null) {
            return;
        }
        DomainTypeHandler domainTypeHandler = getDomainTypeHandler(object);
        ValueHandler valueHandler = domainTypeHandler.getValueHandler(object);
        delete(domainTypeHandler, valueHandler);
    }

    public Operation delete(DomainTypeHandler domainTypeHandler, ValueHandler valueHandler) {
        startAutoTransaction();
        Table storeTable = domainTypeHandler.getStoreTable();
        setPartitionKey(domainTypeHandler, valueHandler);
        Operation op = null;
        try {
            op = clusterTransaction.getDeleteOperation(storeTable);
            domainTypeHandler.operationSetKeys(valueHandler, op);
        } catch (ClusterJException ex) {
            failAutoTransaction();
            throw new ClusterJException(
                    local.message("ERR_Delete", storeTable.getName()), ex);
        }
        endAutoTransaction();
        return op;
    }

    /** Delete the instances corresponding to the parameters.
     * @param objects the objects to delete
     */
    public void deletePersistentAll(Iterable objects) {
        startAutoTransaction();
        for (Iterator it = objects.iterator(); it.hasNext();) {
            deletePersistent(it.next());
        }
        endAutoTransaction();
    }

    /** Delete all instances of the parameter class.
     * @param cls the class of instances to delete
     */
    public <T> int deletePersistentAll(Class<T> cls) {
        DomainTypeHandler<T> domainTypeHandler = getDomainTypeHandler(cls);
        return deletePersistentAll(domainTypeHandler);
    }

    /** Delete all instances of the parameter domainTypeHandler.
     * @param domainTypeHandler the domainTypeHandler of instances to delete
     * @return the number of instances deleted
     */
    public int deletePersistentAll(DomainTypeHandler<?> domainTypeHandler) {
        startAutoTransaction();
        Table storeTable = domainTypeHandler.getStoreTable();
        String tableName = storeTable.getName();
        ScanOperation op = null;
        int count = 0;
        try {
            op = clusterTransaction.getTableScanOperationLockModeExclusiveScanFlagKeyInfo(storeTable);
            count = deletePersistentAll(op, true);
        } catch (ClusterJException ex) {
            failAutoTransaction();
            // TODO add table name to the error message
            throw new ClusterJException(
                    local.message("ERR_Delete_All", tableName), ex);
        }
        endAutoTransaction();
        return count;
    }

    /** Delete all instances retrieved by the operation. The operation must have exclusive
     * access to the instances and have the ScanFlag.KEY_INFO flag set.
     * @param op the scan operation
     * @return the number of instances deleted
     */
    public int deletePersistentAll(ScanOperation op, boolean abort) {
        int cacheCount = 0;
        int count = 0;
        boolean done = false;
        boolean fetch = true;
        // cannot use early autocommit optimization here
        clusterTransaction.setAutocommit(false);
        // execute the operation
        clusterTransaction.executeNoCommit(true, true);
        while (!done ) {
            int result = op.nextResult(fetch);
            switch (result) {
                case RESULT_READY:
                    op.deleteCurrentTuple();
                    ++count;
                    ++cacheCount;
                    fetch = false;
                    break;
                case SCAN_FINISHED:
                    done = true;
                    if (cacheCount != 0) {
                        clusterTransaction.executeNoCommit(abort, true);
                    }
                    op.close();
                    break;
                case CACHE_EMPTY:
                    clusterTransaction.executeNoCommit(abort, true);
                    cacheCount = 0;
                    fetch = true;
                    break;
                default: 
                    throw new ClusterJException(
                            local.message("ERR_Next_Result_Illegal", result));
            }
        }
        return count;
    }

    /** Select a single row from the database. Only the fields requested
     * will be selected. A transaction must be active (either via begin
     * or startAutoTransaction).
     *
     * @param domainTypeHandler the domainTypeHandler to be selected
     * @param keyHandler the key supplier for the select
     * @param fields the fields to select; null to select all fields
     * @return the ResultData from the database
     */
    public ResultData selectUnique(DomainTypeHandler domainTypeHandler,
            ValueHandler keyHandler, BitSet fields) {
        assertActive();
        setPartitionKey(domainTypeHandler, keyHandler);
        Table storeTable = domainTypeHandler.getStoreTable();
        // perform a single select by key operation
        Operation op = clusterTransaction.getSelectOperation(storeTable);
        // set the keys into the operation
        domainTypeHandler.operationSetKeys(keyHandler, op);
        // set the expected columns into the operation
        domainTypeHandler.operationGetValues(op);
        // execute the select and get results
        ResultData rs = op.resultData();
        return rs;
    }

    /** Update an instance in the database. The key field(s)
     * are used to identify the instance; modified fields change the
     * values in the database.
     *
     * @param object the instance to update in the database
     */
    public void updatePersistent(Object object) {
        if (object == null) {
            return;
        }
        DomainTypeHandler domainTypeHandler = getDomainTypeHandler(object);
        if (logger.isDetailEnabled()) logger.detail("UpdatePersistent on object " + object);
        ValueHandler valueHandler = domainTypeHandler.getValueHandler(object);
        update(domainTypeHandler, valueHandler);
    }

    public Operation update(DomainTypeHandler domainTypeHandler, ValueHandler valueHandler) {
        startAutoTransaction();
        setPartitionKey(domainTypeHandler, valueHandler);
        Table storeTable = null;
        Operation op = null;
        try {
            storeTable = domainTypeHandler.getStoreTable();
            op = clusterTransaction.getUpdateOperation(storeTable);
            domainTypeHandler.operationSetKeys(valueHandler, op);
            domainTypeHandler.operationSetModifiedNonPKValues(valueHandler, op);
            if (logger.isDetailEnabled()) logger.detail("Updated object " +
                    valueHandler);
        } catch (ClusterJException ex) {
            failAutoTransaction();
            throw new ClusterJException(
                    local.message("ERR_Update", storeTable.getName()) ,ex);
        }
        endAutoTransaction();
        return op;
    }

    /** Update the instances corresponding to the parameters.
     * @param objects the objects to update
     */
    public void updatePersistentAll(Iterable objects) {
        startAutoTransaction();
        for (Iterator it = objects.iterator(); it.hasNext();) {
            updatePersistent(it.next());
        }
        endAutoTransaction();
    }

    /** Save the instance even if it does not exist.
     * @param instance the instance to save
     */
    public <T> T savePersistent(T instance) {
        DomainTypeHandler domainTypeHandler = getDomainTypeHandler(instance);
        if (logger.isDetailEnabled()) logger.detail("UpdatePersistent on object " + instance);
        ValueHandler valueHandler = domainTypeHandler.getValueHandler(instance);
        startAutoTransaction();
        setPartitionKey(domainTypeHandler, valueHandler);
        Table storeTable = null;
        try {
            storeTable = domainTypeHandler.getStoreTable();
            Operation op = null;
            op = clusterTransaction.getWriteOperation(storeTable);
            domainTypeHandler.operationSetKeys(valueHandler, op);
            domainTypeHandler.operationSetModifiedNonPKValues(valueHandler, op);
            if (logger.isDetailEnabled()) logger.detail("Wrote object " +
                    valueHandler);
        } catch (ClusterJException ex) {
            failAutoTransaction();
            throw new ClusterJException(
                    local.message("ERR_Write", storeTable.getName()) ,ex);
        }
        endAutoTransaction();
        return instance;
    }

    /** Save the instances even if they do not exist.
     * @param instances
     */
    public Iterable savePersistentAll(Iterable instances) {
        List<Object> result = new ArrayList<Object>();
        startAutoTransaction();
        for (Iterator it = instances.iterator(); it.hasNext();) {
            result.add(savePersistent(it.next()));
        }
        endAutoTransaction();
        return result;
    }

    /** Get the current transaction.
     * 
     * @return the transaction
     */
    public Transaction currentTransaction() {
        return transactionImpl;
    }

    /** Close this session and deallocate all resources.
     * 
     */
    public void close() {
        if (clusterTransaction != null) {
            clusterTransaction.close();
            clusterTransaction = null;
        }
        if (db != null) {
            db.close();
            db = null;
        }
    }

    public boolean isClosed() {
        return db==null;
    }

    /** Assert this session is not yet closed. */
    protected void assertNotClosed() {
        if (isClosed()) {
            throw new ClusterJUserException(
                    local.message("ERR_Session_Closed"));
        }
    }

    /** Begin the current transaction.
     * 
     */
    public void begin() {
        if (logger.isDebugEnabled()) logger.debug("begin transaction.");
        transactionState = transactionState.begin();
        handleTransactionException();
    }

    /** Internally begin the transaction.
     * Called by transactionState.begin().
     */
    protected void internalBegin() {
        try {
            clusterTransaction = db.startTransaction(joinTransactionId);
            clusterTransaction.setLockMode(lockmode);
            // if a transaction has already begun, tell the cluster transaction about the key
            if (partitionKey != null) {
                clusterTransaction.setPartitionKey(partitionKey);
            }
            // register our post-execute callback
            clusterTransaction.postExecuteCallback(postExecuteCallbackHandler);
        } catch (ClusterJException ex) {
            throw new ClusterJException(
                    local.message("ERR_Ndb_Start"), ex);
        }
    }

    /** Commit the current transaction.
     * 
     */
    public void commit() {
        if (logger.isDebugEnabled()) logger.debug("commit transaction.");
        transactionState = transactionState.commit();
        handleTransactionException();
    }

    /** Internally commit the transaction.
     * Called by transactionState.commit().
     */
    protected void internalCommit() {
        if (rollbackOnly) {
            try {
                internalRollback();
                throw new ClusterJException(
                        local.message("ERR_Transaction_Rollback_Only"));
            } catch (ClusterJException ex) {
                throw new ClusterJException(
                        local.message("ERR_Transaction_Rollback_Only"), ex);
            }
        }
        try {
            clusterTransaction.executeCommit(true, true);
        } finally {
            // always close the transaction
            clusterTransaction.close();
            clusterTransaction = null;
            partitionKey = null;
        }
    }

    /** Roll back the current transaction.
     *
     */
    public void rollback() {
        if (logger.isDebugEnabled()) logger.debug("roll back transaction.");
        transactionState = transactionState.rollback();
        handleTransactionException();
    }

    /** Internally roll back the transaction.
     * Called by transactionState.rollback() and
     * transactionState.commit() if the transaction is marked for rollback.
     *
     */
    protected void internalRollback() {
        try {
                clusterTransaction.executeRollback();
        } catch (ClusterJException ex) {
            throw new ClusterJException(
                    local.message("ERR_Transaction_Execute", "rollback"), ex);
        } finally {
            if (clusterTransaction != null) {
                clusterTransaction.close();
            }
            clusterTransaction = null;
            partitionKey = null;
        }
    }

    /** Start a transaction if there is not already an active transaction.
     * Throw a ClusterJException if there is any problem.
     */
    public void startAutoTransaction() {
        if (logger.isDebugEnabled()) logger.debug("start AutoTransaction");
        transactionState = transactionState.start();
        handleTransactionException();
    }

    /** End an auto transaction if it was started.
     * Throw a ClusterJException if there is any problem.
     */
    public void endAutoTransaction() {
        if (logger.isDebugEnabled()) logger.debug("end AutoTransaction");
        transactionState = transactionState.end();
        handleTransactionException();
    }

    /** Fail an auto transaction if it was started.
     * Throw a ClusterJException if there is any problem.
     */
    public void failAutoTransaction() {
        if (logger.isDebugEnabled()) logger.debug("fail AutoTransaction");
        transactionState = transactionState.fail();
    }

    protected void handleTransactionException() {
        if (transactionException == null) {
            return;
        } else {
            ClusterJException ex = transactionException;
            transactionException = null;
            throw ex;
        }
    }

    /** Mark the current transaction as rollback only.
     *
     */
    public void setRollbackOnly() {
        rollbackOnly = true;
    }

    /** Is the current transaction marked for rollback only?
     * @return true if the current transaction is marked for rollback only
     */
    public boolean getRollbackOnly() {
        return rollbackOnly;
    }

    /** Manage the state of the transaction associated with this
     * StateManager.
     */
    protected interface TransactionState {
        boolean isActive();

        TransactionState begin();
        TransactionState commit();
        TransactionState rollback();

        TransactionState start();
        TransactionState end();
        TransactionState fail();
    }

    /** This represents the state of Transaction Not Active. */
    protected TransactionState transactionStateNotActive =
            new TransactionState() {

        public boolean isActive() {
            return false;
        }

        public TransactionState begin() {
            try {
                internalBegin();
                return transactionStateActive;
            } catch (ClusterJException ex) {
                transactionException = ex;
                return transactionStateNotActive;
            }
        }

        public TransactionState commit() {
            transactionException = new ClusterJUserException(
                    local.message("ERR_Transaction_Must_Be_Active_For_Method",
                    "commit"));
            return transactionStateNotActive;
        }

        public TransactionState rollback() {
            transactionException = new ClusterJUserException(
                    local.message("ERR_Transaction_Must_Be_Active_For_Method",
                    "rollback"));
            return transactionStateNotActive;
        }

        public TransactionState start() {
            try {
                internalBegin();
                clusterTransaction.setAutocommit(true);
                nestedAutoTransactionCounter = 1;
                return transactionStateAutocommit;
            } catch (ClusterJException ex) {
                transactionException = ex;
                return transactionStateNotActive;
            }
        }

        public TransactionState end() {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Transaction_Auto_Start", "end"));
        }

        public TransactionState fail() {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Transaction_Auto_Start", "end"));
        }

    };

    /** This represents the state of Transaction Active. */
    protected TransactionState transactionStateActive =
            new TransactionState() {

        public boolean isActive() {
            return true;
        }

        public TransactionState begin() {
            transactionException = new ClusterJUserException(
                    local.message("ERR_Transaction_Must_Not_Be_Active_For_Method",
                    "begin"));
            return transactionStateActive;
        }

        public TransactionState commit() {
            try {
                // flush unwritten changes
                flush();
                internalCommit();
            } catch (ClusterJException ex) {
                transactionException = ex;
            }
            return transactionStateNotActive;
        }

        public TransactionState rollback() {
            try {
                internalRollback();
                return transactionStateNotActive;
            } catch (ClusterJException ex) {
                transactionException = ex;
                return transactionStateNotActive;
            }
        }

        public TransactionState start() {
            // nothing to do
            return transactionStateActive;
        }

        public TransactionState end() {
            // nothing to do
            return transactionStateActive;
        }

        public TransactionState fail() {
            // nothing to do
            return transactionStateActive;
        }

    };

    protected TransactionState transactionStateAutocommit =
            new TransactionState() {

        public boolean isActive() {
            return true;
        }

        public TransactionState begin() {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Transaction_Auto_End", "begin"));
        }

        public TransactionState commit() {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Transaction_Auto_End", "commit"));
        }

        public TransactionState rollback() {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Transaction_Auto_End", "rollback"));
        }

        public TransactionState start() {
            // nested start; increment counter
            nestedAutoTransactionCounter++;
            return transactionStateAutocommit;
        }

        public TransactionState end() {
            if (--nestedAutoTransactionCounter > 0) {
                return transactionStateAutocommit;
            } else if (nestedAutoTransactionCounter == 0) {
                try {
                    internalCommit();
                } catch (ClusterJException ex) {
                    transactionException = ex;
                }
                return transactionStateNotActive;
            } else {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Transaction_Auto_Start", "end"));
            }
        }

        public TransactionState fail() {
            try {
                nestedAutoTransactionCounter = 0;
                internalRollback();
                return transactionStateNotActive;
            } catch (ClusterJException ex) {
                // ignore failures caused by internal rollback
                return transactionStateNotActive;
            }
        }

    };

    /** Get the domain type handler for an instance.
     * 
     * @param object the instance for which to get the domain type handler
     * @return the domain type handler
     */
    protected synchronized <T> DomainTypeHandler<T> getDomainTypeHandler(T object) {
        DomainTypeHandler<T> domainTypeHandler =
                factory.getDomainTypeHandler(object, dictionary);
        return domainTypeHandler;
    }

    /** Get the domain type handler for a class.
     * 
     * @param cls the class
     * @return the domain type handler
     */
    public synchronized <T> DomainTypeHandler<T> getDomainTypeHandler(Class<T> cls) {
        DomainTypeHandler<T> domainTypeHandler =
                factory.getDomainTypeHandler(cls, dictionary);
        return domainTypeHandler;
    }

    public Dictionary getDictionary() {
        return dictionary;
    }

    /** Is there an active transaction.
     * 
     * @return true if there is an active transaction
     */
    boolean isActive() {
        return transactionState.isActive();
    }

    /** Is the transaction enlisted. A transaction is enlisted if and only if
     * an operation has been defined that requires an ndb transaction to be
     * started.
     * @return true if the transaction is enlisted
     */
    public boolean isEnlisted() {
        return clusterTransaction==null?false:clusterTransaction.isEnlisted();
    }

    /** Assert that there is an active transaction (the user has called begin
     * or an autotransaction has begun).
     * Throw a user exception if not.
     */
    private void assertActive() {
        if (!transactionState.isActive()) {
            throw new ClusterJUserException(
                    local.message("ERR_Transaction_Must_Be_Active"));
        }
    }

    /** Assert that there is not an active transaction.
     * Throw a user exception if there is an active transaction.
     * @param methodName the name of the method
     */
    private void assertNotActive(String methodName) {
        if (transactionState.isActive()) {
            throw new ClusterJUserException(
                    local.message("ERR_Transaction_Must_Not_Be_Active_For_Method",
                    methodName));
        }
    }

    /** Create a query from a class.
     * 
     * @param cls the class
     * @return the query
     */
    public Query createQuery(Class cls) {
        throw new UnsupportedOperationException(
                local.message("ERR_NotImplemented"));
    }

    /** Get a query builder.
     * 
     * @return the query builder
     */
    public QueryBuilder getQueryBuilder() {
        return new QueryBuilderImpl(this);
    }

    /** Create an index scan operation for an index and table.
     * 
     * @param storeIndex the index
     * @param storeTable the table
     * @return the index scan operation
     */
    public IndexScanOperation getIndexScanOperation(Index storeIndex, Table storeTable) {
        assertActive();
        try {
            IndexScanOperation result = clusterTransaction.getIndexScanOperation(storeIndex, storeTable);
            return result;
        } catch (ClusterJException ex) {
            throw new ClusterJException(
                    local.message("ERR_Index_Scan", storeTable.getName(), storeIndex.getName()), ex);
        }
    }

    /** Create an index scan operation for an index and table to be used for a multi-range scan.
     * 
     * @param storeIndex the index
     * @param storeTable the table
     * @return the index scan operation
     */
    public IndexScanOperation getIndexScanOperationMultiRange(Index storeIndex, Table storeTable) {
        assertActive();
        try {
            IndexScanOperation result = clusterTransaction.getIndexScanOperationMultiRange(storeIndex, storeTable);
            return result;
        } catch (ClusterJException ex) {
            throw new ClusterJException(
                    local.message("ERR_Index_Scan", storeTable.getName(), storeIndex.getName()), ex);
        }
    }

    /** Create an index scan delete operation for an index and table.
     * 
     * @param storeIndex the index
     * @param storeTable the table
     * @return the index scan operation
     */
    public IndexScanOperation getIndexScanDeleteOperation(Index storeIndex, Table storeTable) {
        assertActive();
        try {
            IndexScanOperation result = clusterTransaction.getIndexScanOperationLockModeExclusiveScanFlagKeyInfo(storeIndex, storeTable);
            return result;
        } catch (ClusterJException ex) {
            throw new ClusterJException(
                    local.message("ERR_Index_Scan", storeTable.getName(), storeIndex.getName()), ex);
        }
    }

    /** Create a table scan operation for a table.
     *
     * @param storeTable the table
     * @return the table scan operation
     */
    public ScanOperation getTableScanOperation(Table storeTable) {
        assertActive();
        try {
            ScanOperation result = clusterTransaction.getTableScanOperation(storeTable);
            return result;
        } catch (ClusterJException ex) {
            throw new ClusterJException(
                    local.message("ERR_Table_Scan", storeTable.getName()), ex);
        }
    }

    /** Create a table scan delete operation for a table.
    *
    * @param storeTable the table
    * @return the table scan operation
    */
   public ScanOperation getTableScanDeleteOperation(Table storeTable) {
       assertActive();
       try {
           ScanOperation result = clusterTransaction.getTableScanOperationLockModeExclusiveScanFlagKeyInfo(storeTable);
           return result;
       } catch (ClusterJException ex) {
           throw new ClusterJException(
                   local.message("ERR_Table_Scan", storeTable.getName()), ex);
       }
   }

    /** Create an index operation for an index and table.
     *
     * @param storeIndex the index
     * @param storeTable the table
     * @return the index operation
     */
    public IndexOperation getUniqueIndexOperation(Index storeIndex, Table storeTable) {
        assertActive();
        try {
            IndexOperation result = clusterTransaction.getUniqueIndexOperation(storeIndex, storeTable);
            return result;
        } catch (ClusterJException ex) {
            throw new ClusterJException(
                    local.message("ERR_Unique_Index", storeTable.getName(), storeIndex.getName()), ex);
        }
    }

    /** Create a select operation for a table.
     *
     * @param storeTable the table
     * @return the operation
     */
    public Operation getSelectOperation(Table storeTable) {
        assertActive();
        try {
            Operation result = clusterTransaction.getSelectOperation(storeTable);
            return result;
        } catch (ClusterJException ex) {
            throw new ClusterJException(
                    local.message("ERR_Select", storeTable), ex);
        }
    }

    /** Create a delete operation for a table.
     * 
     * @param storeTable the table
     * @return the operation
     */
    public Operation getDeleteOperation(Table storeTable) {
        assertActive();
        try {
            Operation result = clusterTransaction.getDeleteOperation(storeTable);
            return result;
        } catch (ClusterJException ex) {
            throw new ClusterJException(
                    local.message("ERR_Delete", storeTable), ex);
        }
    }

    /** Create an index delete operation for an index and table.
    *
    * @param storeIndex the index
    * @param storeTable the table
    * @return the index operation
    */
   public IndexOperation getUniqueIndexDeleteOperation(Index storeIndex, Table storeTable) {
       assertActive();
       try {
           IndexOperation result = clusterTransaction.getUniqueIndexDeleteOperation(storeIndex, storeTable);
           return result;
       } catch (ClusterJException ex) {
           throw new ClusterJException(
                   local.message("ERR_Unique_Index_Delete", storeTable.getName(), storeIndex.getName()), ex);
       }
   }

    public void flush() {
        if (logger.isDetailEnabled()) logger.detail("flush changes with changeList size: " + changeList.size());
        if (!changeList.isEmpty()) {
            for (StateManager sm: changeList) {
                sm.flush(this);
            }
            changeList.clear();
        }
        // now flush changes to the back end
        if (clusterTransaction != null) {
            executeNoCommit();
        }
    }

    public List getChangeList() {
        return Collections.unmodifiableList(changeList);
    }

    public void persist(Object instance) {
        makePersistent(instance);
    }

    public void remove(Object instance) {
        deletePersistent(instance);
    }

    public void markModified(StateManager instance) {
        changeList.add(instance);
    }

    public void setPartitionKey(Class<?> domainClass, Object key) {
        DomainTypeHandler<?> domainTypeHandler = getDomainTypeHandler(domainClass);
        String tableName = domainTypeHandler.getTableName();
        // if transaction is enlisted, throw a user exception
        if (isEnlisted()) {
            throw new ClusterJUserException(
                    local.message("ERR_Set_Partition_Key_After_Enlistment", tableName));
        }
        // if a partition key has already been set, throw a user exception
        if (this.partitionKey != null) {
            throw new ClusterJUserException(
                    local.message("ERR_Set_Partition_Key_Twice", tableName));
        }
        ValueHandler handler = domainTypeHandler.createKeyValueHandler(key);
        this.partitionKey= domainTypeHandler.createPartitionKey(handler);
        // if a transaction has already begun, tell the cluster transaction about the key
        if (clusterTransaction != null) {
            clusterTransaction.setPartitionKey(partitionKey);
        }
    }

    /** Mark the field in the instance as modified so it is flushed.
     *
     * @param instance the persistent instance
     * @param fieldName the field to mark as modified
     */
    public void markModified(Object instance, String fieldName) {
        DomainTypeHandler<?> domainTypeHandler = getDomainTypeHandler(instance);
        ValueHandler handler = domainTypeHandler.getValueHandler(instance);
        domainTypeHandler.objectMarkModified(handler, fieldName);
    }

    /** Execute any pending operations (insert, delete, update, load)
     * and then perform post-execute operations (for load) via
     * clusterTransaction.postExecuteCallback().
     * @param abort abort this transaction on error
     * @param force force the operation to be sent immediately
     */
    public void executeNoCommit(boolean abort, boolean force) {
        if (clusterTransaction != null) {
            clusterTransaction.executeNoCommit(abort, force);
        }
    }

    /** Execute any pending operations (insert, delete, update, load)
     * and then perform post-execute operations (for load) via
     * clusterTransaction.postExecuteCallback().
     * Abort the transaction on error. Force the operation to be sent immediately.
     */
    public void executeNoCommit() {
        executeNoCommit(true, true);
    }

    public <T> QueryDomainType<T> createQueryDomainType(DomainTypeHandler<T> domainTypeHandler) {
        QueryBuilderImpl builder = (QueryBuilderImpl)getQueryBuilder();
        return builder.createQueryDefinition(domainTypeHandler);
    }

    /** Return the coordinatedTransactionId of the current transaction.
     * The transaction might not have been enlisted.
     * @return the coordinatedTransactionId
     */
    public String getCoordinatedTransactionId() {
        return clusterTransaction.getCoordinatedTransactionId();
    }

    /** Set the coordinatedTransactionId for the next transaction. This
     * will take effect as soon as the transaction is enlisted.
     * @param coordinatedTransactionId the coordinatedTransactionId
     */
    public void setCoordinatedTransactionId(String coordinatedTransactionId) {
        clusterTransaction.setCoordinatedTransactionId(coordinatedTransactionId);
    }

    /** Set the lock mode for subsequent operations. The lock mode takes effect immediately
     * and continues until set again.
     * @param lockmode the lock mode
     */
    public void setLockMode(LockMode lockmode) {
        this.lockmode = lockmode;
        if (clusterTransaction != null) {
            clusterTransaction.setLockMode(lockmode);
        }
    }

}
