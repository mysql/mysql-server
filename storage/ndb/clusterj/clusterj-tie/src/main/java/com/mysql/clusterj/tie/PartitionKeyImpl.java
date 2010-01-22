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
import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.Table;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.ndbjtie.ndbapi.NdbOperation;
import com.mysql.ndbjtie.ndbapi.NdbTransaction;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;

/**
 * This class manages the startTransaction operation based on partition keys.
 */
class PartitionKeyImpl implements PartitionKey {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(PartitionKeyImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(PartitionKeyImpl.class);

    private static PartitionKeyImpl theInstance = new PartitionKeyImpl();

    /** The partition key parts */
    private List<KeyPart> keyParts = new ArrayList<KeyPart>();

    /** The table */
    private TableConst table = null;

    /** The partition id */
    private int partitionId;
    
    public void addIntKey(Column storeColumn, int key) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, key);
        KeyPart keyPart = new KeyPart(buffer, buffer.limit());
        keyParts.add(keyPart);
    }

    public void addLongKey(Column storeColumn, long key) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, key);
        KeyPart keyPart = new KeyPart(buffer, buffer.limit());
        keyParts.add(keyPart);
    }

    public void addStringKey(Column storeColumn, String string) {
        ByteBuffer buffer = Utility.convertValue(storeColumn, string);
        KeyPart keyPart = new KeyPart(buffer, buffer.limit());
        keyParts.add(keyPart);
    }

    public void setTable(Table table) {
        this.table = ((TableImpl)table).getNdbTable();
    }

    public void setPartitionId(int partitionId) {
        this.partitionId = partitionId;
    }

    protected void handleError(int returnCode, NdbOperation ndbOperation) {
        if (returnCode == 0) {
            return;
        } else {
            Utility.throwError(returnCode, ndbOperation.getNdbError());
        }
    }

    protected static void handleError(Object object, NdbOperation ndbOperation) {
        if (object != null) {
            return;
        } else {
            Utility.throwError(null, ndbOperation.getNdbError());
        }
    }

    public NdbTransaction enlist(DbImpl db) {
        NdbTransaction result = null;
        if (keyParts == null || keyParts.size() == 0) {
            if (logger.isDebugEnabled()) logger.debug(
                    "PartitionKeyImpl.enlist via partitionId with keyparts "
                    + (keyParts==null?"null.":("size " + keyParts.size()))
                    + " table: " + (table==null?"null":table.getName())
                    + " partition id: " + partitionId);
            result = db.enlist(table, partitionId);
        } else {
            if (logger.isDebugEnabled()) logger.debug(
                    "PartitionKeyImpl.enlist via keyParts with keyparts "
                    + (keyParts==null?"null.":("size " + keyParts.size()))
                    + " table: " + (table==null?"null":table.getName()));
            result = db.enlist(table, keyParts);
        }
      if (logger.isDebugEnabled()) logger.debug(
              "PartitionKeyImpl.enlist transaction id: "
               + Long.toHexString(result.getTransactionId()));
        return result;
    }

    /** Get a singleton instance of PartitionKeyImpl that doesn't name a specific table.
     * 
     * @return the instance
     */
    public static PartitionKeyImpl getInstance() {
        return theInstance;
    }

}
