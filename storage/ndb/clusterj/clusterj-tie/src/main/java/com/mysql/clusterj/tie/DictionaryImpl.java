/*
 *  Copyright (c) 2010, 2026, Oracle and/or its affiliates.
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

import com.mysql.clusterj.ClusterJTableException;

import com.mysql.ndbjtie.ndbapi.NdbDictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.DictionaryConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.DictionaryConst.ListConst.Element;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.DictionaryConst.ListConst.ElementArray;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.IndexConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;
import com.mysql.ndbjtie.ndbapi.NdbErrorConst;

import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 *
 */
class DictionaryImpl implements com.mysql.clusterj.core.store.Dictionary {

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(DictionaryImpl.class);

    private final NdbDictionary.Dictionary ndbDictionary;

    private final DbFactoryImpl dbFactory;

    /* If true, prefer getTable() over getTableGlobal() */
    private final boolean preferThreadLocal;

    /* waitMsec in nanos; max allowed execution time */
    private final long maxNanos;

    /* Set of wait times per iteration in the getTable() wait loop */
    private final int[] configWaitTimes;

    public DictionaryImpl(NdbDictionary.Dictionary ndbDictionary,
                          DbFactoryImpl dbFactory,
                          boolean preferThreadLocal) {
        this.ndbDictionary = ndbDictionary;
        this.dbFactory = dbFactory;
        this.preferThreadLocal = preferThreadLocal;

        /* Configure the wait loop in getTable().
        */
        int waitMsec = dbFactory.getTableWaitTime();
        maxNanos = waitMsec * 1000000;
        if(waitMsec == 0) {
            configWaitTimes = new int[0];
        } else if(waitMsec <= 5) {
            configWaitTimes = new int[1];
            configWaitTimes[0] = waitMsec;
        } else {
            configWaitTimes = new int[4];
            configWaitTimes[0] = waitMsec / 10;         // 1 tenth
            configWaitTimes[1] = (waitMsec * 2) / 10;   // 2 tenths
            configWaitTimes[2] = (waitMsec * 3) / 10;   // 3 tenths
            configWaitTimes[3] = (waitMsec * 4) / 10;   // 4 tenths
        }
    }

    private TableConst getNdbTable(String tableName) {
        return preferThreadLocal ?
            ndbDictionary.getTable(tableName) :
            ndbDictionary.getTableGlobal(tableName);
    }

    private IndexConst getNdbIndex(String indexName, String tableName) {
        return preferThreadLocal ?
            ndbDictionary.getIndex(indexName, tableName) :
            ndbDictionary.getIndexGlobal(indexName, tableName);
    }

    public Table getTable(String tableName) {
        final long startNanos = System.nanoTime();
        final long breakTime = startNanos + maxNanos;
        final int retries = configWaitTimes.length;
        final String lowerCaseName = tableName.toLowerCase();
        TableConst ndbTable = null;

        for(int iter = 0; iter <= retries ; iter++) {
            ndbTable = getNdbTable(tableName);
            if (ndbTable == null && ! lowerCaseName.equals(tableName)) {
                // try the lower case table name
                ndbTable = getNdbTable(tableName.toLowerCase());
            }

            if (ndbTable == null && iter < retries) {
                if(System.nanoTime() > breakTime) break;
                try {
                    Thread.sleep(configWaitTimes[iter]);
                } catch (InterruptedException e) {
                    break;
                }
            }
        }

        if (ndbTable == null) {
            NdbErrorConst error = ndbDictionary.getNdbError();
            throw new ClusterJTableException(
                tableName, startNanos, System.nanoTime(),
                error.message(), error.code(), error.mysql_code(),
                error.status(), error.classification());
        }

        if (ndbTable.getObjectStatus() != NdbDictionary.ObjectConst.Status.Retrieved)
            throw new ClusterJTableException(
                tableName, startNanos, System.nanoTime(),
                "Invalid table in local dictionary", -1, 159, 2, 5);

        return new TableImpl(ndbTable, getIndexNames(ndbTable.getName()));
    }

    public Index getIndex(String indexName, String tableName, String indexAlias) {
        if ("PRIMARY$KEY".equals(indexName)) {
            // create a pseudo index for the primary key hash
            TableConst ndbTable = getNdbTable(tableName);
            if (ndbTable == null) {
                // try the lower case table name
                ndbTable = getNdbTable(tableName.toLowerCase());
            }
            handleError(ndbTable, ndbDictionary, "");
            return new IndexImpl(ndbTable);
        }
        IndexConst ndbIndex = getNdbIndex(indexName, tableName);
        if (ndbIndex == null) {
            // try the lower case table name
            ndbIndex = getNdbIndex(indexName, tableName.toLowerCase());
        }
        handleError(ndbIndex, ndbDictionary, indexAlias);
        return new IndexImpl(ndbIndex, indexAlias);
    }

    public String[] getIndexNames(String tableName) {
        // get all indexes for this table including ordered PRIMARY
        DictionaryConst.List indexList = DictionaryConst.List.create();
        handleError(indexList, ndbDictionary, tableName);
        final String[] result;
        try {
            int returnCode = ndbDictionary.listIndexes(indexList, tableName);
            handleError(returnCode, ndbDictionary, tableName);
            int count = indexList.count();
            result = new String[count];
            if (logger.isDetailEnabled()) logger.detail("Found " + count + " indexes for " + tableName);
            ElementArray elementArray = indexList.elements();
            for (int i = 0; i < count; ++i) {
                Element element = elementArray.at(i);
                handleError(element, ndbDictionary, String.valueOf(i));
                String indexName = element.name();
                result[i] = indexName;
            }
        } finally {
            // free the list memory even if error
            com.mysql.ndbjtie.ndbapi.NdbDictionary.DictionaryConst.List.delete(indexList);
        }
        return result;
    }

    protected static void handleError(int returnCode, DictionaryConst ndbDictionary, String extra) {
        if (returnCode == 0) {
            return;
        } else {
            Utility.throwError(returnCode, ndbDictionary.getNdbError(), extra);
        }
    }

    protected static void handleError(Object object, DictionaryConst ndbDictionary, String extra) {
        if (object != null) {
            return;
        } else {
            Utility.throwError(null, ndbDictionary.getNdbError(), extra);
        }
    }

    /** Remove cached table from this ndb dictionary. This allows schema change to work.
     * @param tableName the name of the table
     */
    public void invalidateTable(String tableName) {
        ndbDictionary.invalidateTable(tableName);
    }

    public NdbDictionary.Dictionary getNdbDictionary() {
        return ndbDictionary;
    }
}
