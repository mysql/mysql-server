/*
   Copyright (c) 2012, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.tie;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Table;

import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation;

/** NdbRecordIndexScanOperationImpl performs index scans using NdbRecord.
 * Two NdbRecordImpl instances are used: one to define the bounds (low and high)
 * and one to define the result. The superclass NdbRecordScanOperationImpl
 * holds the NdbRecordImpl and buffer that define and hold the result.
 * <p>
 * This instance declares and holds the bounds while they are being defined.
 * Bounds are handled by creating two bound buffers: one for the low bound
 * and a second for the high bound. While the bounds are being created, the
 * number of columns and the strictness of the bound are recorded.
 * <p>
 * Bounds are calculated elsewhere based on the query parameters and delivered, in sequence,
 * to this instance. Bounds are delivered for the most significant index column first,
 * followed by the next most significant index column, until all columns that have bounds
 * have been delivered. There may be more columns for the low bound versus the high bound,
 * or vice versa. For each bound that is delivered, the method (assignBoundBuffer) determines 
 * to which bound, low or high, the bound belongs. The column count is incremented
 * for the appropriate bound buffer. The value is then applied to the bound buffer using
 * the setXXX method of the NdbRecordImpl that manages the layout of the bound buffer.
 * <p>
 * The superclass declares and holds the filter while it is being defined.
 * <p>
 * At endDefinition, the filter is used to create the scanOptions which is
 * passed to create the NdbIndexScanOperation. Then the bounds are set into
 * the newly created NdbIndexScanOperation.
 * The resulting NdbIndexScanOperation is iterated (scanned) by the NdbRecordResultDataImpl.
 */
public class NdbRecordIndexScanOperationImpl extends NdbRecordScanOperationImpl implements IndexScanOperation {

    /** The ndb index scan operation */
    private NdbIndexScanOperation ndbIndexScanOperation;

    /** The range for this bound */
    private int indexBoundRange = 0;

    /** The buffer that contains low bounds for all index columns */
    private ByteBuffer indexBoundLowBuffer = null;

    /** The number of columns in the low bound */
    private int indexBoundLowCount = 0;

    /** Is the low bound strict? */
    private boolean indexBoundLowStrict = false;

    /** The buffer that contains high bounds for all index columns */
    private ByteBuffer indexBoundHighBuffer = null;

    /** The number of columns in the high bound */
    private int indexBoundHighCount = 0;

    /** Is the high bound strict? */
    private boolean indexBoundHighStrict = false;

    /** Is this an equal scan? */
    private boolean equalScan = true;

    /** The list of index bounds already defined; null for a single range */
    List<NdbIndexScanOperation.IndexBound> ndbIndexBoundList = null;

    /** The single index bound for a single range scan */
    NdbIndexScanOperation.IndexBound ndbIndexBound = null;

    /** The buffers used for bounds; held here to prevent garbage collection */
    List<ByteBuffer> buffers = new ArrayList<ByteBuffer>();

    private Index index;

    @Override
    public String toString() {
        return " NdbRecordIndexScanOperationImpl with index: " + index.getName() + " " + super.toString();
    }

    public NdbRecordIndexScanOperationImpl(ClusterTransactionImpl clusterTransaction,
            Index storeIndex, Table storeTable, int lockMode) {
        this(clusterTransaction, storeIndex, storeTable, false, lockMode);
    }

    public NdbRecordIndexScanOperationImpl(ClusterTransactionImpl clusterTransaction,
                Index storeIndex, Table storeTable, boolean multiRange, int lockMode) {
        super(clusterTransaction, storeTable, lockMode);
        this.index = storeIndex;
        this.multiRange = multiRange;
        if (this.multiRange) {
            ndbIndexBoundList = new ArrayList<NdbIndexScanOperation.IndexBound>();
        }
        ndbRecordKeys = clusterTransaction.getCachedNdbRecordImpl(storeIndex, storeTable);
        keyBufferSize = ndbRecordKeys.bufferSize;
        indexBoundLowBuffer = ndbRecordKeys.newBuffer();
        // hold a reference to the buffer to prevent garbage collection
        buffers.add(indexBoundLowBuffer);
        indexBoundHighBuffer = ndbRecordKeys.newBuffer();
        // hold a reference to the buffer to prevent garbage collection
        buffers.add(indexBoundHighBuffer);
    }

    public void endDefinition() {
        // get the scan options which also sets the filter
        getScanOptions();
        if (logger.isDetailEnabled()) logger.detail("scan index '" + index.getName() + "' with options " + dumpScanOptions(scanOptions.optionsPresent(), scanOptions.scan_flags()));

        // create the scan operation
        ndbIndexScanOperation = clusterTransaction.scanIndex(
                ndbRecordKeys.getNdbRecord(), ndbRecordValues.getNdbRecord(), mask, scanOptions);
        ndbOperation = ndbIndexScanOperation;

        // set the bounds, either from the single indexBound or from multiple ranges
        if (ndbIndexBoundList != null) {
            if (logger.isDetailEnabled()) logger.detail("list size " + ndbIndexBoundList.size());
            // apply all of the bounds to the operation
            for (NdbIndexScanOperation.IndexBound ndbIndexBound: ndbIndexBoundList) {
                int returnCode = ndbIndexScanOperation.setBound(ndbRecordKeys.getNdbRecord(), ndbIndexBound);
                handleError(returnCode, ndbIndexScanOperation);
            }
        } else {
            // zero or one range defined
            ndbIndexBound = getNdbIndexBound();
            if (ndbIndexBound != null) {
                int returnCode = ndbIndexScanOperation.setBound(ndbRecordKeys.getNdbRecord(), ndbIndexBound);
                handleError(returnCode, ndbIndexScanOperation);
            }
        }
        // set the NdbBlob for all active blob columns so that scans will read blob columns
        activateBlobs();
        clusterTransaction.postExecuteCallback(new Runnable() {
            // free structures used to define operation            
            public void run() {
                freeResourcesAfterExecute();
            }
        });
    }

    public void setBoundBigInteger(Column storeColumn, BoundType type, BigInteger value) {
        if (logger.isDetailEnabled()) logger.detail(storeColumn.getName() + " " + type + " " + value);
        // calculate the bound data, the buffer, and the strictness
        ByteBuffer keyBuffer = assignBoundBuffer(type);
        if (keyBuffer == null) {
            // BoundEQ put data into both buffers
            ndbRecordKeys.setBigInteger(indexBoundLowBuffer, storeColumn, value);
            ndbRecordKeys.setBigInteger(indexBoundHighBuffer, storeColumn, value);
        } else {
            ndbRecordKeys.setBigInteger(keyBuffer, storeColumn, value);
        }
    }

    public void setBoundByte(Column storeColumn, BoundType type, byte value) {
        if (logger.isDetailEnabled()) logger.detail(storeColumn.getName() + " " + type + " " + value);
        // calculate the bound data, the buffer, and the strictness
        ByteBuffer keyBuffer = assignBoundBuffer(type);
        if (keyBuffer == null) {
            // BoundEQ put data into both buffers
            ndbRecordKeys.setByte(indexBoundLowBuffer, storeColumn, value);
            ndbRecordKeys.setByte(indexBoundHighBuffer, storeColumn, value);
        } else {
            ndbRecordKeys.setByte(keyBuffer, storeColumn, value);
        }
    }

    public void setBoundBytes(Column storeColumn, BoundType type, byte[] value) {
        if (logger.isDetailEnabled()) logger.detail(storeColumn.getName() + " " + type + " " + value);
        // calculate the bound data, the buffer, and the strictness
        ByteBuffer keyBuffer = assignBoundBuffer(type);
        if (keyBuffer == null) {
            // BoundEQ put data into both buffers
            ndbRecordKeys.setBytes(indexBoundLowBuffer, storeColumn, value);
            ndbRecordKeys.setBytes(indexBoundHighBuffer, storeColumn, value);
        } else {
            ndbRecordKeys.setBytes(keyBuffer, storeColumn, value);
        }
    }

    public void setBoundDecimal(Column storeColumn, BoundType type, BigDecimal value) {
        if (logger.isDetailEnabled()) logger.detail(storeColumn.getName() + " " + type + " " + value);
        // calculate the bound data, the buffer, and the strictness
        ByteBuffer keyBuffer = assignBoundBuffer(type);
        if (keyBuffer == null) {
            // BoundEQ put data into both buffers
            ndbRecordKeys.setDecimal(indexBoundLowBuffer, storeColumn, value);
            ndbRecordKeys.setDecimal(indexBoundHighBuffer, storeColumn, value);
        } else {
            ndbRecordKeys.setDecimal(keyBuffer, storeColumn, value);
        }
    }

    public void setBoundDouble(Column storeColumn, BoundType type, Double value) {
        if (logger.isDetailEnabled()) logger.detail(storeColumn.getName() + " " + type + " " + value);
        // calculate the bound data, the buffer, and the strictness
        ByteBuffer keyBuffer = assignBoundBuffer(type);
        if (keyBuffer == null) {
            // BoundEQ put data into both buffers
            ndbRecordKeys.setDouble(indexBoundLowBuffer, storeColumn, value);
            ndbRecordKeys.setDouble(indexBoundHighBuffer, storeColumn, value);
        } else {
            ndbRecordKeys.setDouble(keyBuffer, storeColumn, value);
        }
    }

    public void setBoundFloat(Column storeColumn, BoundType type, Float value) {
        if (logger.isDetailEnabled()) logger.detail(storeColumn.getName() + " " + type + " " + value);
        // calculate the bound data, the buffer, and the strictness
        ByteBuffer keyBuffer = assignBoundBuffer(type);
        if (keyBuffer == null) {
            // BoundEQ put data into both buffers
            ndbRecordKeys.setFloat(indexBoundLowBuffer, storeColumn, value);
            ndbRecordKeys.setFloat(indexBoundHighBuffer, storeColumn, value);
        } else {
            ndbRecordKeys.setFloat(keyBuffer, storeColumn, value);
        }
    }

    public void setBoundShort(Column storeColumn, BoundType type, short value) {
        if (logger.isDetailEnabled()) logger.detail(storeColumn.getName() + " " + type + " " + value);
        // calculate the bound data, the buffer, and the strictness
        ByteBuffer keyBuffer = assignBoundBuffer(type);
        if (keyBuffer == null) {
            // BoundEQ put data into both buffers
            ndbRecordKeys.setShort(indexBoundLowBuffer, storeColumn, value);
            ndbRecordKeys.setShort(indexBoundHighBuffer, storeColumn, value);
        } else {
            ndbRecordKeys.setShort(keyBuffer, storeColumn, value);
        }
    }

    public void setBoundInt(Column storeColumn, BoundType type, Integer value) {
        if (logger.isDetailEnabled()) logger.detail(storeColumn.getName() + " " + type + " " + value);
        // calculate the bound data, the buffer, and the strictness
        ByteBuffer keyBuffer = assignBoundBuffer(type);
        if (keyBuffer == null) {
            // BoundEQ put data into both buffers
            ndbRecordKeys.setInt(indexBoundLowBuffer, storeColumn, value);
            ndbRecordKeys.setInt(indexBoundHighBuffer, storeColumn, value);
        } else {
            ndbRecordKeys.setInt(keyBuffer, storeColumn, value);
        }
    }

    public void setBoundLong(Column storeColumn, BoundType type, long value) {
        if (logger.isDetailEnabled()) logger.detail(storeColumn.getName() + " " + type + " " + value);
        // calculate the bound data, the buffer, and the strictness
        ByteBuffer keyBuffer = assignBoundBuffer(type);
        if (keyBuffer == null) {
            // BoundEQ put data into both buffers
            ndbRecordKeys.setLong(indexBoundLowBuffer, storeColumn, value);
            ndbRecordKeys.setLong(indexBoundHighBuffer, storeColumn, value);
        } else {
            ndbRecordKeys.setLong(keyBuffer, storeColumn, value);
        }
    }

    public void setBoundString(Column storeColumn, BoundType type, String value) {
        if (logger.isDetailEnabled()) logger.detail(storeColumn.getName() + " " + type + " " + value);
        // calculate the bound data, the buffer, and the strictness
        ByteBuffer keyBuffer = assignBoundBuffer(type);
        if (keyBuffer == null) {
            // BoundEQ put data into both buffers
            ndbRecordKeys.setString(indexBoundLowBuffer, bufferManager, storeColumn, value);
            ndbRecordKeys.setString(indexBoundHighBuffer, bufferManager, storeColumn, value);
        } else {
            ndbRecordKeys.setString(keyBuffer, bufferManager, storeColumn, value);
        }
    }

    public void endBound(int rangeNumber) {
        if (logger.isDetailEnabled()) logger.detail("range: " + rangeNumber);
        indexBoundRange = rangeNumber;
        ndbIndexBoundList.add(getNdbIndexBound());
    }

    private ByteBuffer assignBoundBuffer(BoundType type) {
        switch (type) {
            case BoundEQ:
                indexBoundHighCount++;
                indexBoundLowCount++;
                return null;
            case BoundGE:
                equalScan = false;
                indexBoundHighCount++;
                return indexBoundHighBuffer;
            case BoundGT:
                equalScan = false;
                indexBoundHighStrict = true;
                indexBoundHighCount++;
                return indexBoundHighBuffer;
            case BoundLE:
                equalScan = false;
                indexBoundLowCount++;
                return indexBoundLowBuffer;
            case BoundLT:
                equalScan = false;
                indexBoundLowStrict = true;
                indexBoundLowCount++;
                return indexBoundLowBuffer;
            default:
                throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }
    }

    /** Create an ndb index bound for the current bounds and clear the current bounds
     * 
     */
    private NdbIndexScanOperation.IndexBound getNdbIndexBound() {
        ByteBuffer reclaimed = null;
        if (indexBoundLowCount + indexBoundHighCount > 0) {
            if (indexBoundLowCount == 0) {
                indexBoundLowBuffer =  null;
            } else {
                indexBoundLowBuffer.limit(keyBufferSize);
                indexBoundLowBuffer.position(0);
            }
            if (indexBoundHighCount == 0) {
                indexBoundHighBuffer =  null;
            } else {
                indexBoundHighBuffer.limit(keyBufferSize);
                indexBoundHighBuffer.position(0);
            }
            if (equalScan) {
                reclaimed = indexBoundLowBuffer;
                indexBoundLowBuffer = indexBoundHighBuffer;
            }
            // create the index bound; use a local variable that will be used for either the single or list of bounds
            NdbIndexScanOperation.IndexBound ndbindexBound = db.createIndexBound();
            ndbindexBound.low_key(indexBoundLowBuffer);
            ndbindexBound.high_key(indexBoundHighBuffer);
            ndbindexBound.low_key_count(indexBoundLowCount);
            ndbindexBound.high_key_count(indexBoundHighCount);
            ndbindexBound.low_inclusive(!indexBoundLowStrict);
            ndbindexBound.high_inclusive(!indexBoundHighStrict);
            ndbindexBound.range_no(indexBoundRange);
            if (logger.isDetailEnabled()) logger.detail(
                    " indexBoundLowCount: " + indexBoundLowCount + " indexBoundHighCount: " + indexBoundHighCount +
                    " indexBoundLowStrict: " + indexBoundLowStrict + " indexBoundHighStrict: " + indexBoundHighStrict +
                    " range: " + indexBoundRange
                    );
            // reset the index bound for the next range
            // if equal bound, initialize and reuse previous buffer
            if (reclaimed != null) {
                indexBoundLowBuffer = reclaimed;
                ndbRecordKeys.initializeBuffer(reclaimed);
            } else {
                indexBoundLowBuffer = ndbRecordKeys.newBuffer();
                // hold a reference to the buffer to prevent garbage collection
                buffers.add(indexBoundLowBuffer);
            }
            indexBoundHighBuffer = ndbRecordKeys.newBuffer();
            // hold a reference to the buffer to prevent garbage collection
            buffers.add(indexBoundHighBuffer);
            indexBoundLowCount = 0;
            indexBoundHighCount = 0;
            indexBoundLowStrict = false;
            indexBoundHighStrict = false;
            indexBoundRange = 0;
            equalScan = true;
            return ndbindexBound;
        } else {
            return null;
        }
    }

    /** Release resources used by this after the scan is closed
     */
    @Override
    public void close() {
        super.close();
    }

    /** Free resources used by this scan after the scan is executed.
     * 
     */
    @Override
    public void freeResourcesAfterExecute() {
        if (ndbIndexBound != null) {
            db.delete(ndbIndexBound);
            ndbIndexBound = null;
        }
        if (ndbIndexBoundList != null) {
            for (NdbIndexScanOperation.IndexBound ndbindexBound: ndbIndexBoundList) {
                db.delete(ndbindexBound);
            }
            ndbIndexBoundList = null;
        }
        // return the index buffers used in the operation to the ByteBuffer pool
        if (buffers != null) {
            for (ByteBuffer buffer: buffers) {
                ndbRecordKeys.returnBuffer(buffer);
            }
        }
        super.freeResourcesAfterExecute();
    }

}
