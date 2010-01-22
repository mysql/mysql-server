/*
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

import com.mysql.ndbjtie.ndbapi.NdbDictionary.DictionaryConst;
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

    private DictionaryConst ndbDictionary;

    public DictionaryImpl(DictionaryConst ndbDictionary) {
        this.ndbDictionary = ndbDictionary;
    }

    public Table getTable(String tableName) {
        TableConst ndbTable = ndbDictionary.getTable(tableName);
        if (ndbTable == null) {
            // try the lower case name in case it's a simple user misunderstanding
            ndbTable = ndbDictionary.getTable(tableName.toLowerCase());
        }
        handleError(ndbTable, ndbDictionary, tableName);
        IndexConst primaryKeyIndex = ndbDictionary.getIndex("PRIMARY", tableName);
        // no need to handle error if the PRIMARY index doesn't exist
        return new TableImpl(ndbTable, primaryKeyIndex);
    }

    public Index getIndex(String indexName, String tableName, String indexAlias) {
        IndexConst ndbIndex = ndbDictionary.getIndex(indexName, tableName);
        handleError(ndbIndex, ndbDictionary, indexAlias);
        return new IndexImpl(ndbIndex, indexAlias);
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

}
