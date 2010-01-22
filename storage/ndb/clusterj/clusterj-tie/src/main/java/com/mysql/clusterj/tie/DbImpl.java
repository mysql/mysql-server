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
import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbTransaction;
import com.mysql.ndbjtie.ndbapi.Ndb.Key_part_ptr;
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

    public DbImpl(Ndb ndb, int maxTransactions) {
        this.ndb = ndb;
        int returnCode = ndb.init(maxTransactions);
        handleError(returnCode, ndb);
    }

    public void close() {
    }

    public com.mysql.clusterj.core.store.Dictionary getDictionary() {
        Dictionary ndbDictionary = ndb.getDictionary();
        handleError(ndbDictionary, ndb);
        return new DictionaryImpl(ndbDictionary);
    }

    public ClusterTransaction startTransaction() {
        return new ClusterTransactionImpl(this);
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
    public NdbTransaction enlist(TableConst table, List<KeyPart> keyParts) {
        if (keyParts == null || keyParts.size() == 0) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Key_Parts_Must_Not_Be_Null_Or_Zero_Length",
                            table.getName()));
        } else if (keyParts.size() > 1) {
            // TODO enable this code once startTransaction with multiple key parts supported
            // allocate one extra for null-terminated array (initialized to null)
//            Key_part_ptr[] parts = new Key_part_ptr[keyParts.size() + 1];
//            for (int i = 0; i < keyParts.size(); ++i) {
//                parts[i] = Key_part_ptr.create(); // delete these below
//                keyParts.get(i).get(parts[i]);
//            }
//            NdbTransaction ndbTransaction = ndb.startTransaction(
//                table, parts, partitionKeyScratchBuffer, partitionKeyScratchBuffer.capacity());
//            // even if error, delete all the key parts to avoid memory leaks
//            for (int i = 0; i < keyParts.size(); ++i) {
//                Key_part_ptr.delete(parts[i]);
//            }
//            handleError (ndbTransaction, ndb);
//            return ndbTransaction;
            System.out.println("DbImpl.enlist(): Multiple key parts are not supported... yet.");
            NdbTransaction result = ndb.startTransaction(null, null, 0);
            handleError (result, ndb);
            return result;
        } else {
            // extract the ByteBuffer from the keyPart
            NdbTransaction ndbTransaction = ndb.startTransaction(
                    table, keyParts.get(0).getBuffer(), keyParts.get(0).getLength());
            handleError (ndbTransaction, ndb);
            return ndbTransaction;
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
    public NdbTransaction enlist(TableConst table, int partitionId) {
        NdbTransaction result = null;
        if (table == null) {
            result = ndb.startTransaction(null, null, 0);
        } else {
            result = ndb.startTransaction(table, partitionId);
        }
        handleError (result, ndb);
        return result;
    }

}
