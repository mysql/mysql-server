/*
   Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode;

/**
 *
 */
class NdbRecordScanResultDataImpl extends NdbRecordResultDataImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(NdbRecordScanResultDataImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(NdbRecordScanResultDataImpl.class);

    /** Flags for iterating a scan */
    protected final int RESULT_READY = 0;
    protected final int SCAN_FINISHED = 1;
    protected final int CACHE_EMPTY = 2;

    /** The ClusterTransaction only for executing the lock takeover if any */
    private final ClusterTransactionImpl clusterTransaction;

    /** The NdbOperation that defines the result */
    private final NdbRecordScanOperationImpl scanOperation;

    /** The NdbScanOperation */
    private final NdbScanOperation ndbScanOperation;

    /** The number to skip */
    protected final long skip;

    /** The limit */
    protected final long limit;

    /** The record counter during the scan */
    protected long recordCounter = 0;

    /** True if records are locked during this scan */
    private final boolean lockRecordsDuringScan;

    /** True if any records have been locked while scanning the cache */
    private List<NdbOperationConst> recordsLocked = new ArrayList<NdbOperationConst>();

    /** Construct the ResultDataImpl based on an NdbRecordOperationImpl.
     * When used with the compatibility operations, delegate to the NdbRecordOperation
     * to copy data.
     * @param clusterTransaction the cluster transaction used to take over locks
     * @param operation the NdbRecordOperationImpl
     * @param skip the number of rows to skip
     * @param limit the last row number
     */
    public NdbRecordScanResultDataImpl(ClusterTransactionImpl clusterTransaction,
            NdbRecordScanOperationImpl scanOperation, long skip, long limit) {
        super(scanOperation);
        this.clusterTransaction = clusterTransaction;
        this.scanOperation = scanOperation;
        this.ndbScanOperation = (NdbScanOperation)scanOperation.ndbOperation;
        this.skip = skip;
        this.limit = limit;
        this.lockRecordsDuringScan = ndbScanOperation.getLockMode() != LockMode.LM_CommittedRead;
    }

    /** If any locks were taken over, execute the takeover operations
     */
    private void executeIfRecordsLocked() {
        if (recordsLocked.size() != 0) {
            clusterTransaction.executeNoCommit(false, true);
            for (NdbOperationConst ndbOperation: recordsLocked) {
                NdbErrorConst ndbError = ndbOperation.getNdbError();
                if (ndbError.code() == 0) {
                    continue;
                } else {
                    String detail = clusterTransaction.db.getNdbErrorDetail(ndbError);
                    Utility.throwError(null, ndbError, detail);
                }
            }
            recordsLocked.clear();
        }
    }

    @Override
    public boolean next() {
        if (!clusterTransaction.isEnlisted()) {
            throw new ClusterJUserException(local.message("ERR_Db_Is_Closing"));
        }
        if (recordCounter >= limit) {
            // the next record is past the limit; we have delivered all the rows
            executeIfRecordsLocked();
            ndbScanOperation.close(true, true);
            return false;
        }
        // NdbScanOperation may have many results.
        boolean done = false;
        boolean fetch = false;
        boolean force = true; // always true for scans
        while (!done) {
            int result = scanOperation.nextResultCopyOut(fetch, force);
            switch (result) {
                case RESULT_READY:
                    if (++recordCounter > skip) {
                        // this record is past the skip
                        // if scanning with locks, grab the lock for the current transaction
                        if (lockRecordsDuringScan) {
                            NdbOperationConst lockedRecord = scanOperation.lockCurrentTuple();
                            if (lockedRecord != null) {
                                recordsLocked.add(lockedRecord);
                            }
                        }
                        // check the NdbRecord buffer guard
                        // load blob data into the operation
                        scanOperation.loadBlobValues();
                        return true;
                    } else {
                        // skip this record
                        scanOperation.returnValueBuffer();
                        break;
                    }
                case SCAN_FINISHED:
                    scanOperation.returnValueBuffer();
                    executeIfRecordsLocked();
                    scanOperation.close();
                    return false;
                case CACHE_EMPTY:
                    scanOperation.returnValueBuffer();
                    executeIfRecordsLocked();
                    fetch = true;
                    break;
                default:
                    Utility.throwError(result, ndbScanOperation.getNdbError());
            }
        }
        return true; // this statement is needed to make the compiler happy but it's never executed
    }

}
