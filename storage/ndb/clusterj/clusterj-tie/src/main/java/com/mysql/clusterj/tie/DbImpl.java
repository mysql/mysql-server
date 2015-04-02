/*
 *  Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
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

    // TODO change the allocation to a constant in ndbjtie
    private int errorBufferSize = 300;

    /** The ndb error detail buffer */
    private ByteBuffer errorBuffer = ByteBuffer.allocateDirect(errorBufferSize);

    // TODO change the allocation to a constant in ndbjtie
    /** The size of the coordinated transaction identifier buffer */
    private final static int COORDINATED_TRANSACTION_ID_SIZE = 44;

    /** The coordinated transaction identifier buffer */
    private ByteBuffer coordinatedTransactionIdBuffer =
            ByteBuffer.allocateDirect(COORDINATED_TRANSACTION_ID_SIZE);

    // TODO change the allocation to something reasonable
    /** The partition key scratch buffer */
    private ByteBuffer partitionKeyScratchBuffer = ByteBuffer.allocateDirect(10000);

    /** The BufferManager for this instance, used for all operations for the session */
    private BufferManager bufferManager = new BufferManager();

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
    private int numberOfInterpretedCodeCreated;

    /** The number of InterpretedCode deleted */
    private int numberOfInterpretedCodeDeleted;

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
        int returnCode = ndb.init(maxTransactions);
        handleError(returnCode, ndb);
        ndbDictionary = ndb.getDictionary();
        handleError(ndbDictionary, ndb);
        this.dictionary = new DictionaryImpl(ndbDictionary, clusterConnection);
    }

    protected void assertOpen(String where) {
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
        if (numberOfInterpretedCodeCreated != numberOfInterpretedCodeDeleted) {
            logger.warn("numberOfInterpretedCodeCreated " + numberOfInterpretedCodeCreated + 
                    " != numberOfInterpretedCodeDeleted " + numberOfInterpretedCodeDeleted);
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
            if (clusterTransaction.isEnlisted()) {
                throw new ClusterJUserException(local.message("ERR_Cannot_close_active_transaction")); 
            }
            clusterTransaction.close();
            clusterTransaction = null;
        }
        if (ndb != null) {
            Ndb.delete(ndb);
            ndb = null;
        }
        clusterConnection.close(this);
    }

    public com.mysql.clusterj.core.store.Dictionary getDictionary() {
        return dictionary;
    }

    public Dictionary getNdbDictionary() {
        return ndbDictionary;
    }

    public ClusterTransaction startTransaction(String joinTransactionId) {
        assertOpen("startTransaction");
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
        handleError(table, ndb);
        Key_part_ptrArray key_part_ptrArray = null;
        if (keyPartsSize == 1) {
            // extract the ByteBuffer and length from the keyPart
            ByteBuffer buffer = keyParts.get(0).buffer;
            int length = keyParts.get(0).length;
            ndbTransaction = ndb.startTransaction(table, buffer, length);
            if (ndbTransaction == null) {
                logger.warn(local.message("ERR_Transaction_Start_Failed",
                        tableName, buffer.position(), buffer.limit(), buffer.capacity(), length));
            }
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

        /** Shared buffer for string output operations */
        private ByteBuffer stringStorageBuffer = ByteBuffer.allocateDirect(STRING_STORAGE_BUFFER_INITIAL_SIZE);

        /** Result data buffer initial size */
        private static final int RESULT_DATA_BUFFER_INITIAL_SIZE = 8000;

        /** Buffer to hold result data */
        private ByteBuffer resultDataBuffer = ByteBuffer.allocateDirect(RESULT_DATA_BUFFER_INITIAL_SIZE);

        /** Guarantee the size of the string storage buffer to be a minimum size. If the current
         * string storage buffer is not big enough, allocate a bigger one. The current buffer
         * will be garbage collected.
         * @param size the minimum size required
         */
        public void guaranteeStringStorageBufferSize(int sizeNeeded) {
            if (sizeNeeded > stringStorageBuffer.capacity()) {
                if (logger.isDebugEnabled()) logger.debug(local.message("MSG_Reallocated_Byte_Buffer",
                        "string storage", stringStorageBuffer.capacity(), sizeNeeded));
                // the existing shared buffer will be garbage collected
                stringStorageBuffer = ByteBuffer.allocateDirect(sizeNeeded);
            }
            stringStorageBuffer.limit(stringStorageBuffer.capacity());
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

        /** Guarantee the size of the string byte buffer to be a minimum size. If the current
         * string byte buffer is not big enough, allocate a bigger one. The current buffer
         * will be garbage collected.
         * @param size the minimum size required
         */
        protected void guaranteeStringByteBufferSize(int sizeNeeded) {
            if (sizeNeeded > stringByteBufferCurrentSize) {
                stringByteBufferCurrentSize = sizeNeeded;
                stringByteBuffer = ByteBuffer.allocateDirect(stringByteBufferCurrentSize);
                stringCharBuffer = stringByteBuffer.asCharBuffer();
            }
            if (stringByteBuffer == null) {
                stringByteBuffer = ByteBuffer.allocateDirect(stringByteBufferCurrentSize);
                stringCharBuffer = stringByteBuffer.asCharBuffer();
            } else {
                stringByteBuffer.clear();
                stringCharBuffer.clear();
            }
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
            if (sizeNeeded > resultDataBuffer.capacity()) {
                if (logger.isDebugEnabled()) logger.debug(local.message("MSG_Reallocated_Byte_Buffer",
                        "result data", resultDataBuffer.capacity(), sizeNeeded));
                // the existing result data buffer will be garbage collected
                resultDataBuffer = ByteBuffer.allocateDirect(sizeNeeded);
            }
            resultDataBuffer.clear();
            return resultDataBuffer;
        }

    }

    public NdbRecordOperationImpl newNdbRecordOperationImpl(Table storeTable) {
        return clusterConnection.newNdbRecordOperationImpl(this, storeTable);
    }

    public IndexBound createIndexBound() {
        ++numberOfIndexBoundCreated;
        return IndexBound.create();
    }

    public void delete(IndexBound ndbIndexBound) {
        ++numberOfIndexBoundDeleted;
        IndexBound.delete(ndbIndexBound);
    }

    public NdbInterpretedCode createInterpretedCode(TableConst ndbTable, int i) {
        ++numberOfInterpretedCodeCreated;
        return NdbInterpretedCode.create(ndbTable, null, i);
    }

    public void delete(NdbInterpretedCode ndbInterpretedCode) {
        ++numberOfInterpretedCodeDeleted;
        NdbInterpretedCode.delete(ndbInterpretedCode);
    }

    public NdbScanFilter createScanFilter(NdbInterpretedCode ndbInterpretedCode) {
        ++numberOfNdbScanFilterCreated;
        return NdbScanFilter.create(ndbInterpretedCode);
    }

    public void delete(NdbScanFilter ndbScanFilter) {
        ++numberOfNdbScanFilterDeleted;
        NdbScanFilter.delete(ndbScanFilter);
    }

    public ScanOptions createScanOptions() {
        ++numberOfScanOptionsCreated;
        return ScanOptions.create();
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
