/*
 *  Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.
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
import java.nio.CharBuffer;
import java.util.List;

import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.Ndb.Key_part_ptr;
import com.mysql.ndbjtie.ndbapi.Ndb.Key_part_ptrArray;

import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbInterpretedCode;
import com.mysql.ndbjtie.ndbapi.NdbScanFilter;
import com.mysql.ndbjtie.ndbapi.NdbTransaction;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary;
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation.IndexBound;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation.ScanOptions;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.store.ClusterTransaction;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 *
 */
class DbImpl implements com.mysql.clusterj.core.store.Db {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(DbImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(DbImpl.class);

    /** The Ndb instance that this instance is wrapping */
    private Ndb ndb;

    /** The ndb error detail buffer */
    private ByteBuffer errorBuffer;

    /** The coordinated transaction identifier buffer */
    private ByteBuffer coordinatedTransactionIdBuffer;

    /** The partition key scratch buffer */
    private ByteBuffer partitionKeyScratchBuffer;

    /** The BufferManager for this instance, used for all operations for the session */
    private BufferManager bufferManager;

    /** The NdbDictionary for this Ndb */
    private Dictionary ndbDictionary;

    /** The Dictionary for this DbImpl */
    private DictionaryImpl dictionary;

    /** The ClusterConnection */
    private ClusterConnectionImpl clusterConnection;

    /** This db is closing */
    private boolean closing = false;

    /** The ClusterTransaction */
    private ClusterTransaction clusterTransaction;

    /** The number of IndexBound created */
    private int numberOfIndexBoundCreated;

    /** The number of IndexBound deleted */
    private int numberOfIndexBoundDeleted;

    /** The number of InterpretedCode created */
    private int numberOfNdbInterpretedCodeCreated;

    /** The number of InterpretedCode deleted */
    private int numberOfNdbInterpretedCodeDeleted;

    /** The number of NdbScanFilters created */
    private int numberOfNdbScanFilterCreated;

    /** The number of NdbScanFilters deleted */
    private int numberOfNdbScanFilterDeleted;

    /** The number of ScanOptions created */
    private int numberOfScanOptionsCreated;

    /** The number of ScanOptions deleted */
    private int numberOfScanOptionsDeleted;

    /** The autoincrement batch size */
    private int autoIncrementBatchSize;

    /** The autoincrement step */
    private long autoIncrementStep;

    /** The autoincrement start */
    private long autoIncrementStart;

    public DbImpl(ClusterConnectionImpl clusterConnection, Ndb ndb, int maxTransactions) {
        this.clusterConnection = clusterConnection;
        this.ndb = ndb;
        this.errorBuffer =
                this.clusterConnection.byteBufferPoolForDBImplError.borrowBuffer();
        this.partitionKeyScratchBuffer =
                this.clusterConnection.byteBufferPoolForPartitionKey.borrowBuffer();
        this.coordinatedTransactionIdBuffer =
                this.clusterConnection.byteBufferPoolForCoordinatedTransactionId.borrowBuffer();
        this.bufferManager = new BufferManager(this.clusterConnection.byteBufferPool);
        int returnCode = ndb.init(maxTransactions);
        handleError(returnCode, ndb);
        ndbDictionary = ndb.getDictionary();
        handleError(ndbDictionary, ndb);
        this.dictionary = new DictionaryImpl(ndbDictionary, clusterConnection);
    }

    public void assertNotClosed(String where) {
        if (closing || ndb == null) {
            throw new ClusterJUserException(local.message("ERR_Db_Is_Closing", where));
        }
    }

    protected void closing() {
        closing = true;
    }

    public void close() {
        // check the counts of interface objects created versus deleted
        if (numberOfIndexBoundCreated != numberOfIndexBoundDeleted) {
            logger.warn("numberOfIndexBoundCreated " + numberOfIndexBoundCreated + 
                    " != numberOfIndexBoundDeleted " + numberOfIndexBoundDeleted);
        }
        if (numberOfNdbInterpretedCodeCreated != numberOfNdbInterpretedCodeDeleted) {
            logger.warn("numberOfNdbInterpretedCodeCreated " + numberOfNdbInterpretedCodeCreated +
                    " != numberOfNdbInterpretedCodeDeleted " + numberOfNdbInterpretedCodeDeleted);
        }
        if (numberOfNdbScanFilterCreated != numberOfNdbScanFilterDeleted) {
            logger.warn("numberOfNdbScanFilterCreated " + numberOfNdbScanFilterCreated + 
                    " != numberOfNdbScanFilterDeleted " + numberOfNdbScanFilterDeleted);
        }
        if (numberOfScanOptionsCreated != numberOfScanOptionsDeleted) {
            logger.warn("numberOfScanOptionsCreated " + numberOfScanOptionsCreated + 
                    " != numberOfScanOptionsDeleted " + numberOfScanOptionsDeleted);
        }
        if (clusterTransaction != null) {
            clusterTransaction.close();
            clusterTransaction = null;
        }
        if (ndb != null) {
            Ndb.delete(ndb);
            ndb = null;
        }
        this.clusterConnection.byteBufferPoolForDBImplError.returnBuffer(this.errorBuffer);
        this.clusterConnection.byteBufferPoolForPartitionKey.returnBuffer(this.partitionKeyScratchBuffer);
        this.clusterConnection.byteBufferPoolForCoordinatedTransactionId.returnBuffer(this.coordinatedTransactionIdBuffer);
        this.bufferManager.release();
        clusterConnection.close(this);
    }

    public com.mysql.clusterj.core.store.Dictionary getDictionary() {
        return dictionary;
    }

    public Dictionary getNdbDictionary() {
        return ndbDictionary;
    }

    public ClusterTransaction startTransaction(String joinTransactionId) {
        assertNotClosed("DbImpl.startTransaction()");
        clusterTransaction = new ClusterTransactionImpl(clusterConnection, this, ndbDictionary, joinTransactionId);
        return clusterTransaction;
    }

    protected void handleError(int returnCode, Ndb ndb) {
        if (returnCode == 0) {
            return;
        } else {
            NdbErrorConst ndbError = ndb.getNdbError();
            String detail = getNdbErrorDetail(ndbError);
            Utility.throwError(returnCode, ndbError, detail);
        }
    }

    protected void handleError(Object object, Ndb ndb) {
        if (object != null) {
            return;
        } else {
            NdbErrorConst ndbError = ndb.getNdbError();
            String detail = getNdbErrorDetail(ndbError);
            Utility.throwError(null, ndbError, detail);
        }
    }

    protected void handleError(Object object, Dictionary ndbDictionary) {
        if (object != null) {
            return;
        } else {
            NdbErrorConst ndbError = ndbDictionary.getNdbError();
            String detail = getNdbErrorDetail(ndbError);
            Utility.throwError(null, ndbError, detail);
        }
    }

    public boolean isRetriable(ClusterJDatastoreException ex) {
        return Utility.isRetriable(ex);
    }

    public String getNdbErrorDetail(NdbErrorConst ndbError) {
        return ndb.getNdbErrorDetail(ndbError, errorBuffer, errorBuffer.capacity());
    }

    /** Enlist an NdbTransaction using table and key data to specify 
     * the transaction coordinator.
     * 
     * @param tableName the name of the table
     * @param keyParts the list of partition key parts
     * @return the ndbTransaction
     */
    public NdbTransaction enlist(String tableName, List<KeyPart> keyParts) {
        if (keyParts == null || keyParts.size() <= 0) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Key_Parts_Must_Not_Be_Null_Or_Zero_Length",
                            tableName));
        }
        int keyPartsSize = keyParts.size();
        NdbTransaction ndbTransaction = null;
        TableConst table = ndbDictionary.getTable(tableName);
        handleError(table, ndbDictionary);
        Key_part_ptrArray key_part_ptrArray = null;
        if (keyPartsSize == 1) {
            // extract the ByteBuffer and length from the keyPart
            KeyPart keyPart = keyParts.get(0);
            ByteBuffer buffer = keyPart.buffer;
            int length = keyPart.length;
            ndbTransaction = ndb.startTransaction(table, buffer, length);
            if (ndbTransaction == null) {
                logger.warn(local.message("ERR_Transaction_Start_Failed",
                        tableName, buffer.position(), buffer.limit(), buffer.capacity(), length));
            }
            bufferManager.returnPartitionKeyPartBuffer(length, buffer);
            handleError (ndbTransaction, ndb);
            return ndbTransaction;
        }
        key_part_ptrArray = Key_part_ptrArray.create(keyPartsSize + 1);
        try {
            // the key part pointer array has one entry for each key part
            // plus one extra for "null-terminated array concept"
            Key_part_ptr key_part_ptr;
            for (int i = 0; i < keyPartsSize; ++i) {
                // each key part ptr consists of a ByteBuffer (char *) and length
                key_part_ptr = key_part_ptrArray.at(i);
                key_part_ptr.ptr(keyParts.get(i).buffer);
                key_part_ptr.len(keyParts.get(i).length);
            }
            // the last key part needs to be initialized to (char *)null
            key_part_ptr = key_part_ptrArray.at(keyPartsSize);
            key_part_ptr.ptr(null);
            key_part_ptr.len(0);
            ndbTransaction = ndb.startTransaction(
                table, key_part_ptrArray, 
                partitionKeyScratchBuffer, partitionKeyScratchBuffer.capacity());
            handleError (ndbTransaction, ndb);
            return ndbTransaction;
        } finally {
            // even if error, delete the key part array to avoid memory leaks
            Key_part_ptrArray.delete(key_part_ptrArray);
            // return the borrowed buffers for the partition key
            for (int i = 0; i < keyPartsSize; ++i) {
                KeyPart keyPart = keyParts.get(i);
                bufferManager.returnPartitionKeyPartBuffer(keyPart.length, keyPart.buffer);
            }
        }
    }

    /** Enlist an NdbTransaction using table and partition id to specify 
     * the transaction coordinator. This method is also used if
     * the key data is null.
     * 
     * @param tableName the name of the table
     * @param partitionId the partition id
     * @return the ndbTransaction
     */
    public NdbTransaction enlist(String tableName, int partitionId) {
        NdbTransaction result = null;
        if (tableName == null) {
            result = ndb.startTransaction(null, null, 0);
        } else {
            TableConst table= ndbDictionary.getTable(tableName);
            result = ndb.startTransaction(table, partitionId);
        }
        handleError (result, ndb);
        return result;
    }

    /** Return the coordinated transaction id buffer. 
     * The buffer is allocated here because there is only one buffer 
     * ever needed and it is only needed in one place, after the
     * transaction is enlisted.
     * @return the coordinated transaction id buffer
     */
    public ByteBuffer getCoordinatedTransactionIdBuffer() {
        return coordinatedTransactionIdBuffer;
    }

    /** Join a transaction already in progress. The transaction might be
     * on the same or a different node from this node. The usual case is
     * for the transaction to be joined is on a different node.
     * @param coordinatedTransactionId
     * (from ClusterTransaction.getCoordinatedTransactionId())
     * @return a transaction joined to the existing transaction
     */
    public NdbTransaction joinTransaction(String coordinatedTransactionId) {
        if (logger.isDetailEnabled()) logger.detail("CoordinatedTransactionId: "
                + coordinatedTransactionId);
//        NdbTransaction result = ndb.joinTransaction(coordinatedTransactionId);
//        handleError(result, ndb);
//        return result;
        throw new ClusterJFatalInternalException("Not Implemented");
    }

    /** Get the buffer manager for this DbImpl. All operations that need byte buffers
     * use this instance to manage the shared buffers.
     * @return the buffer manager
     */
    public BufferManager getBufferManager() {
        return bufferManager;
    }

    public class BufferManager {
        /** String byte buffer initial size */
        public static final int STRING_BYTE_BUFFER_INITIAL_SIZE = 1000;

        /** String byte buffer current size */
        private int stringByteBufferCurrentSize = STRING_BYTE_BUFFER_INITIAL_SIZE;

        /** Buffers for String encoding; reused for each String column in the operation.
         * These buffers share common data but have their own position and limit. */
        ByteBuffer stringByteBuffer = null;
        CharBuffer stringCharBuffer = null;

        /** String storage buffer initial size (used for non-primitive output data) */
        public static final int STRING_STORAGE_BUFFER_INITIAL_SIZE = 500;

        /** String storage buffer current size */
        private int stringStorageBufferCurrentSize = STRING_BYTE_BUFFER_INITIAL_SIZE;

        /** Shared buffer for string output operations */
        private ByteBuffer stringStorageBuffer;

        /** Result data buffer initial size; only needed for non-NdbRecord operations */
        private int resultDataBufferCurrentSize = 0;

        /** Buffer to hold result data */
        private ByteBuffer resultDataBuffer;

        /** Buffer pool */
        private VariableByteBufferPoolImpl pool;

        protected BufferManager(VariableByteBufferPoolImpl pool) {
            this.pool = pool;
            this.stringStorageBuffer = pool.borrowBuffer(STRING_STORAGE_BUFFER_INITIAL_SIZE);
            this.stringByteBuffer = pool.borrowBuffer(STRING_BYTE_BUFFER_INITIAL_SIZE);
            this.stringCharBuffer = stringByteBuffer.asCharBuffer();
        }

        /** Release resources for this buffer manager. */
        protected void release() {
            if (this.resultDataBuffer != null) {
                pool.returnBuffer(resultDataBufferCurrentSize, this.resultDataBuffer);
            }
            pool.returnBuffer(stringStorageBufferCurrentSize, stringStorageBuffer);
            pool.returnBuffer(stringByteBufferCurrentSize, stringByteBuffer);
        }

        /** Guarantee the size of the string storage buffer to be a minimum size. If the current
         * string storage buffer is not big enough, allocate a bigger one. The current buffer
         * will be garbage collected.
         * @param size the minimum size required
         */
        public void guaranteeStringStorageBufferSize(int sizeNeeded) {
            if (sizeNeeded > stringStorageBufferCurrentSize) {
                if (logger.isDebugEnabled()) logger.debug(local.message("MSG_Reallocated_Byte_Buffer",
                        "string storage", stringStorageBufferCurrentSize, sizeNeeded));
                // return the existing shared buffer to the pool
                pool.returnBuffer(stringStorageBufferCurrentSize, stringStorageBuffer);
                stringStorageBuffer = pool.borrowBuffer(sizeNeeded);
                stringStorageBufferCurrentSize = sizeNeeded;
            }
            stringStorageBuffer.limit(stringStorageBufferCurrentSize);
        }

        /** Copy the contents of the parameter String into a reused string buffer.
         * The ByteBuffer can subsequently be encoded into a ByteBuffer.
         * @param value the string
         * @return the byte buffer with the String in it
         */
        public ByteBuffer copyStringToByteBuffer(CharSequence value) {
            if (value == null) {
                stringByteBuffer.limit(0);
                return stringByteBuffer;
            }
            int sizeNeeded = value.length() * 2;
            guaranteeStringByteBufferSize(sizeNeeded);
            stringCharBuffer.append(value);
            // characters in java are always two bytes (UCS-16)
            stringByteBuffer.limit(stringCharBuffer.position() * 2);
            return stringByteBuffer;
        }

        /** Reset the string storage buffer so it can be used for another operation.
         * 
         */
        public void clearStringStorageBuffer() {
            stringStorageBuffer.clear();
        }

        public ByteBuffer getStringStorageBuffer(int sizeNeeded) {
            guaranteeStringStorageBufferSize(sizeNeeded);
            return stringStorageBuffer;
        }

        public ByteBuffer getStringByteBuffer(int sizeNeeded) {
            guaranteeStringByteBufferSize(sizeNeeded);
            return stringByteBuffer;
        }

        /** Borrow a buffer */
        public ByteBuffer borrowBuffer(int length) {
            return pool.borrowBuffer(length);
         }

        /** Return a buffer */
        public void returnBuffer(int length, ByteBuffer buffer) {
            pool.returnBuffer(length, buffer);
        }

        /** Guarantee the size of the string byte buffer to be a minimum size. If the current
         * string byte buffer is not big enough, return the current buffer to the pool and get 
         * another buffer.
         * @param size the minimum size required
         */
        protected void guaranteeStringByteBufferSize(int sizeNeeded) {
            if (sizeNeeded > stringByteBufferCurrentSize) {
                pool.returnBuffer(stringByteBufferCurrentSize, stringByteBuffer);
                stringByteBufferCurrentSize = sizeNeeded;
                stringByteBuffer = pool.borrowBuffer(sizeNeeded);
                stringCharBuffer = stringByteBuffer.asCharBuffer();
            }
            // reset the buffers to the proper position and limit
            stringByteBuffer.limit(stringByteBufferCurrentSize);
            stringByteBuffer.position(0);
            // characters in java are always two bytes (UCS-16)
            stringCharBuffer.limit(stringByteBufferCurrentSize / 2);
            stringCharBuffer.position(0);
        }

        /** Get the string char buffer. This buffer is paired with the string byte buffer.
         * They share the same data but have independent position and limit.
         * @return the string char buffer
         */
        public CharBuffer getStringCharBuffer() {
            return stringCharBuffer;
        }

        /** Get the result data buffer. This buffer is used to hold the result of a
         * key or scan operation.
         * @param sizeNeeded the size that the buffer must be able to hold
         * @return the result data buffer
         */
        public ByteBuffer getResultDataBuffer(int sizeNeeded) {
            if (sizeNeeded > resultDataBufferCurrentSize) {
                if (logger.isDebugEnabled()) logger.debug(local.message("MSG_Reallocated_Byte_Buffer",
                        "result data", resultDataBufferCurrentSize, sizeNeeded));
                // return the existing result data buffer to the pool
                if (resultDataBuffer != null) {
                    pool.returnBuffer(resultDataBufferCurrentSize, resultDataBuffer);
                }
                resultDataBuffer = pool.borrowBuffer(sizeNeeded);
                resultDataBufferCurrentSize = sizeNeeded;
            }
            resultDataBuffer.limit(resultDataBufferCurrentSize);
            resultDataBuffer.position(0);
            return resultDataBuffer;
        }

        /** Borrow a buffer for a partition key part */
        public ByteBuffer borrowPartitionKeyPartBuffer(int length) {
            return pool.borrowBuffer(length);
        }

        /** Return a buffer used for a partition key part */
        public void returnPartitionKeyPartBuffer(int length, ByteBuffer buffer) {
            pool.returnBuffer(length, buffer);
        }
    }

    public NdbRecordOperationImpl newNdbRecordOperationImpl(Table storeTable) {
        return clusterConnection.newNdbRecordOperationImpl(this, storeTable);
    }

    public IndexBound createIndexBound() {
        IndexBound result = null;
        int attempts = 0;
        Object syncObject = null;
        if (syncObject != null) {
            attempts++;
            synchronized(syncObject) {
                result = IndexBound.create();
            }
        } else {
            while (result == null && attempts++ < 10) {
                result = IndexBound.create();
            }
        }
        if (result == null) {
            throw new ClusterJFatalInternalException(local.message(
                    "ERR_Constructor", "IndexBound", "failed", attempts));
        }
        if (attempts != 1) {
            logger.warn(local.message(
                    "ERR_Constructor", "IndexBound", "succeeded", attempts));
        }
        ++numberOfIndexBoundCreated;
        return result;
    }

    public void delete(IndexBound ndbIndexBound) {
        ++numberOfIndexBoundDeleted;
        IndexBound.delete(ndbIndexBound);
    }

    public NdbInterpretedCode createInterpretedCode(TableConst ndbTable, int i) {
        NdbInterpretedCode result = null;
        int attempts = 0;
        Object syncObject = null;
        if (syncObject != null) {
            attempts++;
            synchronized(syncObject) {
                result = NdbInterpretedCode.create(ndbTable, null, i);
            }
        } else {
            while (result == null && attempts++ < 10) {
                result = NdbInterpretedCode.create(ndbTable, null, i);
            }
        }
        if (result == null) {
            throw new ClusterJFatalInternalException(local.message(
                    "ERR_NdbInterpretedCode_Constructor", "failed", attempts));
        }
        if (attempts != 1) {
            logger.warn(local.message(
                    "ERR_Constructor", "NdbInterpretedCode", "succeeded", attempts));
        }
        ++numberOfNdbInterpretedCodeCreated;
        return result;
    }

    public void delete(NdbInterpretedCode ndbInterpretedCode) {
        ++numberOfNdbInterpretedCodeDeleted;
        NdbInterpretedCode.delete(ndbInterpretedCode);
    }

    public NdbScanFilter createScanFilter(NdbInterpretedCode ndbInterpretedCode) {
        NdbScanFilter result = null;
        int attempts = 0;
        Object syncObject = null;
        if (syncObject != null) {
            attempts++;
            synchronized(syncObject) {
                result = NdbScanFilter.create(ndbInterpretedCode);
            }
        } else {
            while (result == null && attempts++ < 10) {
                result = NdbScanFilter.create(ndbInterpretedCode);
            }
        }
        if (result == null) {
            throw new ClusterJFatalInternalException(local.message(
                    "ERR_Constructor", "NdbScanFilter", "failed", attempts));
        }
        if (attempts != 1) {
            logger.warn(local.message(
                    "ERR_Constructor", "NdbScanFilter", "succeeded", attempts));
        }
        ++numberOfNdbScanFilterCreated;
        return result;
    }

    public void delete(NdbScanFilter ndbScanFilter) {
        ++numberOfNdbScanFilterDeleted;
        NdbScanFilter.delete(ndbScanFilter);
    }

    public ScanOptions createScanOptions() {
        ScanOptions result = null;
        int attempts = 0;
        Object syncObject = null;
        if (syncObject != null) {
            attempts++;
            synchronized(syncObject) {
                result = ScanOptions.create();
            }
        } else {
            while (result == null && attempts++ < 10) {
                result = ScanOptions.create();
            }
        }
        if (result == null) {
            throw new ClusterJFatalInternalException(local.message(
                    "ERR_Constructor", "ScanOptions", "failed", attempts));
        }
        if (attempts != 1) {
            logger.warn(local.message(
                    "ERR_Constructor", "ScanOptions", "succeeded", attempts));
        }
        ++numberOfScanOptionsCreated;
        return result;
    }

    public void delete(ScanOptions scanOptions) {
        ++numberOfScanOptionsDeleted;
        ScanOptions.delete(scanOptions);
    }

    /** Get the autoincrement value for the table. This method is called from NdbRecordOperationImpl.insert
     * to get the next autoincrement value.
     */
    public long getAutoincrementValue(Table table) {
        long autoIncrementValue;
        // get a new autoincrement value
        long[] ret = new long[] {0L, autoIncrementBatchSize, autoIncrementStep, autoIncrementStart};
        int returnCode = ndb.getAutoIncrementValue(((TableImpl)table).ndbTable, ret,
                autoIncrementBatchSize, autoIncrementStep, autoIncrementStart);
        handleError(returnCode, ndb);
        autoIncrementValue = ret[0];
        if (logger.isDetailEnabled()) {
            logger.detail("getAutoIncrementValue(...batchSize: " + autoIncrementBatchSize +
                " step: " + autoIncrementStep + " start: " + autoIncrementStart + ") returned " + autoIncrementValue);
        }
        return autoIncrementValue;
    }

    public void initializeAutoIncrement(long[] autoIncrement) {
        this.autoIncrementBatchSize = (int)autoIncrement[0];
        this.autoIncrementStep = autoIncrement[1];
        this.autoIncrementStart = autoIncrement[2];
    }

}
