/*
 *  Copyright (C) 2009-2010 Sun Microsystems, Inc.
 *  All rights reserved. Use is subject to license terms.
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
import java.util.List;

import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.Ndb.Key_part_ptr;
import com.mysql.ndbjtie.ndbapi.Ndb.Key_part_ptrArray;

import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbTransaction;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.core.store.ClusterTransaction;

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
            .getInstance(com.mysql.clusterj.core.store.ClusterConnection.class);

    private Ndb ndb;

    // TODO change the allocation from 300 to a constant in ndbjtie
    /** The ndb error detail buffer */
    private ByteBuffer errorBuffer = ByteBuffer.allocateDirect(300);

    // TODO change the allocation to something reasonable
    /** The partition key scratch buffer */
    private ByteBuffer partitionKeyScratchBuffer = ByteBuffer.allocateDirect(10000);

    /** The NdbDictionary for this Ndb */
    private Dictionary ndbDictionary;

    /** The Dictionary for this DbImpl */
    private DictionaryImpl dictionary;

    public DbImpl(Ndb ndb, int maxTransactions) {
        this.ndb = ndb;
        int returnCode = ndb.init(maxTransactions);
        handleError(returnCode, ndb);
        ndbDictionary = ndb.getDictionary();
        handleError(ndbDictionary, ndb);
        this.dictionary = new DictionaryImpl(ndbDictionary);
    }

    public void close() {
        Ndb.delete(ndb);
    }

    public com.mysql.clusterj.core.store.Dictionary getDictionary() {
        return dictionary;
    }

    public ClusterTransaction startTransaction() {
        return new ClusterTransactionImpl(this, ndbDictionary);
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
     * @param table the table
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
            ndbTransaction = ndb.startTransaction(
                    table, keyParts.get(0).buffer, keyParts.get(0).length);
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
     * @param table the table
     * @param keyParts the list of partition key parts
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

}
