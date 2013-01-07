/*
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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

import java.util.List;

import com.mysql.clusterj.core.store.Column;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.clusterj.tie.DbImpl.BufferManager;

import com.mysql.ndbjtie.ndbapi.NdbScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode;

/**
 *
 */
class ScanResultDataImpl extends ResultDataImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(ScanResultDataImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(ScanResultDataImpl.class);

    private NdbScanOperation ndbScanOperation = null;

    /** The number to skip */
    protected long skip = 0;

    /** The limit */
    protected long limit = Long.MAX_VALUE;

    /** The record counter during the scan */
    protected long recordCounter = 0;

    /** Flags for iterating a scan */
    protected final int RESULT_READY = 0;
    protected final int SCAN_FINISHED = 1;
    protected final int CACHE_EMPTY = 2;

    public ScanResultDataImpl(NdbScanOperation ndbScanOperation, List<Column> storeColumns,
            int maximumColumnId, int bufferSize, int[] offsets, int[] lengths, int maximumColumnLength,
            BufferManager bufferManager, long skip, long limit) {
        super(ndbScanOperation, storeColumns, maximumColumnId, bufferSize, offsets, lengths,
                bufferManager, false);
        this.ndbScanOperation = ndbScanOperation;
        this.skip = skip;
        this.limit = limit;
    }

    @Override
    public boolean next() {
        if (recordCounter >= limit) {
            // the next record is past the limit; we have delivered all the rows
            ndbScanOperation.close(true, true);
            return false;
        }
        // NdbScanOperation may have many results.
        boolean done = false;
        boolean fetch = false;
        boolean force = true; // always true for scans
        while (!done) {
            int result = ndbScanOperation.nextResult(fetch, force);
            switch (result) {
                case RESULT_READY:
                    if (++recordCounter > skip) {
                        // this record is past the skip
                        // if scanning with locks, grab the lock for the current transaction
                        if (ndbScanOperation.getLockMode() != LockMode.LM_CommittedRead) { 
                            ndbScanOperation.lockCurrentTuple();
                        }
                        return true;
                    } else {
                        // skip this record
                        break;
                    }
                case SCAN_FINISHED:
                    ndbScanOperation.close(true, true);
                    return false;
                case CACHE_EMPTY:
                    fetch = true;
                    break;
                default:
                    Utility.throwError(result, ndbScanOperation.getNdbError());
            }
        }
        return true; // this statement is needed to make the compiler happy but it's never executed
    }

}
