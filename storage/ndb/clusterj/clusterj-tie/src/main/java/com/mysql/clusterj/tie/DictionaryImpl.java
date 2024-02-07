/*
 *  Copyright (c) 2010, 2024, Oracle and/or its affiliates.
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

import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.DictionaryConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.DictionaryConst.ListConst.Element;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.DictionaryConst.ListConst.ElementArray;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.IndexConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;

import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 *
 */
class DictionaryImpl implements com.mysql.clusterj.core.store.Dictionary {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(DictionaryImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(DictionaryImpl.class);

    private Dictionary ndbDictionary;

    private ClusterConnectionImpl clusterConnection;

    public DictionaryImpl(Dictionary ndbDictionary, ClusterConnectionImpl clusterConnection) {
        this.ndbDictionary = ndbDictionary;
        this.clusterConnection = clusterConnection;
    }

    public Table getTable(String tableName) {
        TableConst ndbTable = ndbDictionary.getTable(tableName);
        if (ndbTable == null) {
            // try the lower case table name
            ndbTable = ndbDictionary.getTable(tableName.toLowerCase());
        }
        if (ndbTable == null) {
            return null;
        }
        return new TableImpl(ndbTable, getIndexNames(ndbTable.getName()));
    }

    public Index getIndex(String indexName, String tableName, String indexAlias) {
        if ("PRIMARY$KEY".equals(indexName)) {
            // create a pseudo index for the primary key hash
            TableConst ndbTable = ndbDictionary.getTable(tableName);
            if (ndbTable == null) {
                // try the lower case table name
                ndbTable = ndbDictionary.getTable(tableName.toLowerCase());
            }
            handleError(ndbTable, ndbDictionary, "");
            return new IndexImpl(ndbTable);
        }
        IndexConst ndbIndex = ndbDictionary.getIndex(indexName, tableName);
        if (ndbIndex == null) {
            // try the lower case table name
            ndbIndex = ndbDictionary.getIndex(indexName, tableName.toLowerCase());
        }
        handleError(ndbIndex, ndbDictionary, indexAlias);
        return new IndexImpl(ndbIndex, indexAlias);
    }

    public String[] getIndexNames(String tableName) {
        // get all indexes for this table including ordered PRIMARY
        com.mysql.ndbjtie.ndbapi.NdbDictionary.DictionaryConst.List indexList = 
            com.mysql.ndbjtie.ndbapi.NdbDictionary.DictionaryConst.List.create();
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
    public void removeCachedTable(String tableName) {
        // remove the cached table from this dictionary
        ndbDictionary.removeCachedTable(tableName);
        // also remove the cached NdbRecord associated with this table
        clusterConnection.unloadSchema(tableName);
    }

    public Dictionary getNdbDictionary() {
        return ndbDictionary;
    }

}
