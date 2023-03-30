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

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.Query.Ordering;

import com.mysql.ndbjtie.ndbapi.NdbInterpretedCode;
import com.mysql.ndbjtie.ndbapi.NdbInterpretedCodeConst;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst;
import com.mysql.ndbjtie.ndbapi.NdbOperation;
import com.mysql.ndbjtie.ndbapi.NdbScanFilter;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation.ScanFlag;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation.ScanOptions;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation.ScanOptionsConst.Type;

/** NdbRecordScanOperationImpl performs table and index scans using NdbRecord.
 * The scans are set up via subclasses. After executing, the NdbRecordScanOperationImpl instance
 * is owned and iterated (scanned) by NdbRecordScanResultDataImpl.
 */
public abstract class NdbRecordScanOperationImpl extends NdbRecordOperationImpl implements ScanOperation {

    /** The ndb scan options */
    ScanOptions scanOptions = null;

    /** The ndb scan filter */
    NdbScanFilter ndbScanFilter = null;

    /** The ndb interpreted code used for filters */
    NdbInterpretedCode ndbInterpretedCode = null;

    /** Is this scan multi-range? */
    protected boolean multiRange = false;

    /** The lock mode for this operation */
    int lockMode;

    /** The ordering for this operation */
    Ordering ordering = null;

    public NdbRecordScanOperationImpl(ClusterTransactionImpl clusterTransaction, Table storeTable,
            int lockMode) {
        super(clusterTransaction, storeTable);
        this.ndbRecordKeys = clusterTransaction.getCachedNdbRecordImpl(storeTable);
        this.keyBufferSize = ndbRecordKeys.getBufferSize();
        this.ndbRecordValues = clusterTransaction.getCachedNdbRecordImpl(storeTable);
        this.valueBufferSize = ndbRecordValues.getBufferSize();
        this.numberOfColumns = ndbRecordValues.getNumberOfColumns();
        this.blobs = new NdbRecordBlobImpl[this.numberOfColumns];
        this.lockMode = lockMode;
        resetMask();
    }

    /** Construct a new ResultData and if requested, execute the operation.
     */
    @Override
    public ResultData resultData(boolean execute) {
        return resultData(execute, 0, Long.MAX_VALUE);
    }

    /** Construct a new ResultData and if requested, execute the operation.
     */
    public ResultData resultData(boolean execute, long skip, long limit) {
        NdbRecordResultDataImpl result =
            new NdbRecordScanResultDataImpl(clusterTransaction, this, skip, limit);
        if (execute) {
            clusterTransaction.executeNoCommit(false, true);
        }
        return result;
    }

    @Override
    public String toString() {
        return " NdbRecordScanOperationImpl with table: " + tableName + " " + super.toString();
    }

    /** Close the ndbOperation used by this scan after the scan is complete.
     * 
     */
    public void close() {
        ((NdbScanOperation)ndbOperation).close(true, true);
    }

    /** Deallocate resources used by this scan after the scan is executed */
    public void freeResourcesAfterExecute() {
        if (ndbInterpretedCode != null) {
            db.delete(ndbInterpretedCode);
            ndbInterpretedCode = null;
        }
        if (ndbScanFilter != null) {
            db.delete(ndbScanFilter);
            ndbScanFilter = null;
        }
        if (scanOptions != null) {
            db.delete(scanOptions);
            scanOptions = null;
        }
        super.freeResourcesAfterExecute();
    }

    public void deleteCurrentTuple() {
        int returnCode = ((NdbScanOperation)ndbOperation).deleteCurrentTuple();
        handleError(returnCode, ndbOperation);
    }

    /** Create scan options for this scan. 
     * Scan options are used to set a filter into the NdbScanOperation,
     * set the key info flag if using a lock mode that requires lock takeover, and set the multi range flag.
     * set either SF_OrderBy or SF_Descending to get ordered scans.
     */
    protected void getScanOptions() {
        long options = 0;
        int flags = 0;
        if (ordering != null
                || multiRange
                || lockMode != com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead
                || ndbScanFilter != null) {
            // create scan options only if we have scan options to set
            scanOptions = db.createScanOptions();
            if (ordering != null) {
                options |= Type.SO_SCANFLAGS;
                switch (ordering) {
                    case ASCENDING:
                        flags |= ScanFlag.SF_OrderBy;
                        break;
                    case DESCENDING:
                        flags |= ScanFlag.SF_Descending;
                        flags |= ScanFlag.SF_OrderBy;
                        break;
                    default:
                        throw new ClusterJFatalInternalException(local.message("ERR_Invalid_Ordering", ordering));
                }
            }
            if (multiRange) {
                options |= Type.SO_SCANFLAGS;
                flags |= ScanFlag.SF_MultiRange;
                flags |= ScanFlag.SF_ReadRangeNo;
            }
            if (lockMode != com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead) {
                options |= Type.SO_SCANFLAGS;
                flags |= ScanFlag.SF_KeyInfo;
            }
            if (ndbScanFilter != null) {
                options |= (long)Type.SO_INTERPRETED;
                scanOptions.interpretedCode(ndbScanFilter.getInterpretedCode());
            }
            if (flags != 0) {
                scanOptions.scan_flags(flags);
            }
            scanOptions.optionsPresent(options);
        }
        if (logger.isDebugEnabled()) logger.debug("ScanOptions: " + dumpScanOptions(options, flags));
    }

    protected String dumpScanOptions(long optionsPresent, int flags) {
        StringBuilder builder = new StringBuilder();
        if (0L != (optionsPresent & (long)Type.SO_BATCH)) builder.append("SO_BATCH ");
        if (0L != (optionsPresent & (long)Type.SO_GETVALUE)) builder.append("SO_GETVALUE ");
        if (0L != (optionsPresent & (long)Type.SO_PARALLEL)) builder.append("SO_PARALLEL ");
        if (0L != (optionsPresent & (long)Type.SO_CUSTOMDATA)) builder.append("SO_CUSTOMDATA ");
        if (0L != (optionsPresent & (long)Type.SO_INTERPRETED)) builder.append("SO_INTERPRETED ");
        if (0L != (optionsPresent & (long)Type.SO_PARTITION_ID)) builder.append("SO_PARTITION_ID ");
        if (0L != (optionsPresent & (long)Type.SO_SCANFLAGS)) {
            builder.append("SO_SCANFLAGS(");
            if (0 != (flags & ScanFlag.SF_KeyInfo)) builder.append("SF_KeyInfo ");
            if (0 != (flags & ScanFlag.SF_Descending)) builder.append("SF_Descending ");
            if (0 != (flags & ScanFlag.SF_DiskScan)) builder.append("SF_DiskScan ");
            if (0 != (flags & ScanFlag.SF_MultiRange)) builder.append("SF_MultiRange ");
            if (0 != (flags & ScanFlag.SF_OrderBy)) builder.append("SF_OrderBy ");
            if (0 != (flags & ScanFlag.SF_ReadRangeNo)) builder.append("SF_ReadRangeNo ");
            if (0 != (flags & ScanFlag.SF_TupScan)) builder.append("SF_TupScan ");
            builder.append(")");
        }
        return builder.toString();
    }

    /** Create a scan filter for this scan.
     * @param context the query execution context
     * @return the ScanFilter to build the filter for the scan
     */
    public ScanFilter getScanFilter(QueryExecutionContext context) {
        
        ndbInterpretedCode = db.createInterpretedCode(ndbRecordValues.getNdbTable(), 0);
        ndbScanFilter = db.createScanFilter(ndbInterpretedCode);
        ScanFilter scanFilter = new ScanFilterImpl(ndbScanFilter, db);
        context.addFilter(scanFilter);
        return scanFilter;
    }

    /** Get the next result from the scan.
     * Only used for deletePersistentAll to scan the table and delete all rows.
     */
    public int nextResult(boolean fetch) {
        if (!clusterTransaction.isEnlisted()) {
            throw new ClusterJUserException(local.message("ERR_Db_Is_Closing"));
        }
        int result = ((NdbScanOperation)ndbOperation).nextResult(fetch, false);
        clusterTransaction.handleError(result);
        return result;
    }

    /** Get the next result from the scan. Copy the data into a newly allocated result buffer.
     * 
     */
    public int nextResultCopyOut(boolean fetch, boolean force) {
        if (!clusterTransaction.isEnlisted()) {
            throw new ClusterJUserException(local.message("ERR_Db_Is_Closing"));
        }
        allocateValueBuffer(false);
        int result = ((NdbScanOperation)ndbOperation).nextResultCopyOut(valueBuffer, fetch, force);
        return result;
    }

    /** Transfer the lock on the current tuple to the original transaction.
     * This allows the original transaction to keep the results locked until
     * the original transaction completes.
     * Only transfer the lock if the lock mode is not committed read
     * (there is no lock held for committed read).
     */
    public NdbOperationConst lockCurrentTuple() {
        NdbOperationConst result = null;
        if (lockMode != com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode.LM_CommittedRead) {
            result = ((NdbScanOperation)ndbOperation).lockCurrentTuple(
                    clusterTransaction.ndbTransaction, ndbRecordValues.getNdbRecord(),
                    null, null, null, 0);
            if (result == null) {
                Utility.throwError(result, ndbOperation.getNdbError());
            }
        }
        return result;
    }

    /** Transform this NdbRecordOperationImpl into one that can be used to back a SmartValueHandler.
     * For instances that are used in scans, create a new instance and allocate a new buffer
     * to continue the scan.
     * 
     * @return the NdbRecordOperationImpl
     */
    @Override
    public NdbRecordOperationImpl transformNdbRecordOperationImpl() {
        NdbRecordOperationImpl result = new NdbRecordOperationImpl(this);
        // we gave away our buffers; get new ones when needed
        this.valueBuffer = null;
        this.keyBuffer = null;
        return result;
    }

    public void setOrdering(Ordering ordering) {
        this.ordering = ordering;
    }

}
