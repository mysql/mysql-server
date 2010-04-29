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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.ndbjtie.ndbapi.NdbDictionary.ColumnConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;

/**
 *
 */
class TableImpl implements Table {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(TableImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(TableImpl.class);

    /** The table name */
    private String tableName;

    /** The primary key column names */
    private String[] primaryKeyColumnNames;

    /** The partition key column names */
    private String[] partitionKeyColumnNames;

    /** The columns in this table */
    private Map<String, ColumnImpl> columns = new HashMap<String, ColumnImpl>();

    /** The index names for this table */
    private String[] indexNames;

    public TableImpl(TableConst ndbTable, String[] indexNames) {
        this.tableName = ndbTable.getName();
        // process columns and partition key columns
        List<String> partitionKeyColumnNameList = new ArrayList<String>();
        List<String> primaryKeyColumnNameList = new ArrayList<String>();

        for (int i = 0; i < ndbTable.getNoOfColumns(); ++i) {
            ColumnConst column = ndbTable.getColumn(i);
            // primary key and partition key columns are listed in the order declared in the schema
            if (column.getPartitionKey()) {
                partitionKeyColumnNameList.add(column.getName());
            }
            if (column.getPrimaryKey()) {
                primaryKeyColumnNameList.add(column.getName());
            }
            ColumnConst ndbColumn = ndbTable.getColumn(i);
            columns.put(ndbColumn.getName(), new ColumnImpl(tableName, ndbColumn));
        }
        this.primaryKeyColumnNames = 
            primaryKeyColumnNameList.toArray(new String[primaryKeyColumnNameList.size()]);
        this.partitionKeyColumnNames = 
            partitionKeyColumnNameList.toArray(new String[partitionKeyColumnNameList.size()]);
        this.indexNames = indexNames;
    }

    public Column getColumn(String columnName) {
        return columns.get(columnName);
    }

    public String getName() {
        return tableName;
    }

    public String[] getPrimaryKeyColumnNames() {
        return primaryKeyColumnNames;
    }

    public String[] getPartitionKeyColumnNames() {
        return partitionKeyColumnNames;
    }

    public PartitionKey createPartitionKey() {
        PartitionKeyImpl result = new PartitionKeyImpl();
        result.setTable(tableName);
        return result;
    }

    public String[] getIndexNames() {
        return indexNames;
    }

}
