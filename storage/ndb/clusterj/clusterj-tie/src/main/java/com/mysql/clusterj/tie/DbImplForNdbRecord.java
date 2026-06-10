/*
 *  Copyright (c) 2012, 2026, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
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

import java.util.List;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;

import com.mysql.clusterj.core.store.ClusterTransaction;
import com.mysql.clusterj.core.store.Db;

import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbInterpretedCode;
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation.IndexBound;
import com.mysql.ndbjtie.ndbapi.NdbScanFilter;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation.ScanOptions;
import com.mysql.ndbjtie.ndbapi.NdbTransaction;
import com.mysql.ndbjtie.ndbapi.NdbDictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;

/**
 * This class is used to hold the ndb Dictionary used for NdbRecord. It has the minimum
 * implementation needed to implement the life cycle of the Ndb. In particular, it omits the
 * buffer manager and partition key scratch buffer used in the standard DbImpl.
 */
class DbImplForNdbRecord extends DbImplCore implements Db {

    /** The ClusterConnection */
    private ClusterConnectionImpl clusterConnection;

    /** The DbFactory */
    final DbFactoryImpl parentFactory;

    public DbImplForNdbRecord(DbFactoryImpl factory, Ndb ndb) {
        super(ndb, 1);
        this.parentFactory = factory;
        this.clusterConnection = factory.connectionImpl;
        this.errorBuffer = this.clusterConnection.byteBufferPoolForDBImplError.borrowBuffer();
        handleInitError();
        handleError(ndbDictionary, ndb);
    }

    public void close() {
        clusterConnection.byteBufferPoolForDBImplError.returnBuffer(this.errorBuffer);
        Ndb.delete(ndb);
        parentFactory.closeDb(this);
    }

    public com.mysql.clusterj.core.store.Dictionary getDictionary() {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public ClusterTransaction startTransaction() {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public NdbTransaction enlist(String tableName, List<KeyPart> keyParts) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public NdbTransaction enlist(String tableName, int partitionId) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    /** Initialize ndbjtie infrastructure for query objects created via jtie wrapper create methods.
     * Creating them here avoids race conditions later with multiple threads trying to create
     * them simultaneously. The initialization only needs to be done once.
     */
    protected void initializeQueryObjects() {
        synchronized(ClusterConnectionImpl.class) {
            if (ClusterConnectionImpl.queryObjectsInitialized) {
                return;
            }
            IndexBound indexBound = IndexBound.create();
            if (indexBound != null) IndexBound.delete(indexBound);
            ScanOptions scanOptions = ScanOptions.create();
            if (scanOptions != null) ScanOptions.delete(scanOptions);
            NdbDictionary.Table table = NdbDictionary.Table.create("dummy");
            if (table != null) {
                NdbInterpretedCode ndbInterpretedCode = NdbInterpretedCode.create(table, null, 0);
                if (ndbInterpretedCode != null) {
                    NdbScanFilter ndbScanFilter = NdbScanFilter.create(ndbInterpretedCode);
                    if (ndbScanFilter != null) {
                        NdbScanFilter.delete(ndbScanFilter);
                    }
                    NdbInterpretedCode.delete(ndbInterpretedCode);
                }
                NdbDictionary.Table.delete(table);
            }
            ClusterConnectionImpl.queryObjectsInitialized = true;
        }
    }
}
