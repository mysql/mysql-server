/*
 *  Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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

import com.mysql.ndbjtie.ndbapi.NdbRecord;
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

    /** The column names */
    private String[] columnNames;

    /** The primary key column names */
    private String[] primaryKeyColumnNames;

    /** The partition key column names */
    private String[] partitionKeyColumnNames;

    /** The columns in this table */
    private Map<String, ColumnImpl> columns = new HashMap<String, ColumnImpl>();

    /** The index names for this table */
    private String[] indexNames;

    /** The array of lengths of column space indexed by column id */
    private int[] lengths;

    /** The array of offsets of column space indexed by column id */
    private int[] offsets;

    /** The total size of buffer needed for all columns */
    private int bufferSize;

    /** The maximum column id */
    private int maximumColumnId;

    /** The maximum column length */
    private int maximumColumnLength = 0;

    public TableImpl(TableConst ndbTable, String[] indexNames) {
        this.tableName = ndbTable.getName();
        // process columns and partition key columns
        List<String> partitionKeyColumnNameList = new ArrayList<String>();
        List<String> primaryKeyColumnNameList = new ArrayList<String>();

        int noOfColumns = ndbTable.getNoOfColumns();
        ColumnImpl[] columnImpls = new ColumnImpl[noOfColumns];
        columnNames = new String[noOfColumns];
        for (int i = 0; i < noOfColumns; ++i) {
            ColumnConst column = ndbTable.getColumn(i);
            // primary key and partition key columns are listed in the order declared in the schema
            if (column.getPartitionKey()) {
                partitionKeyColumnNameList.add(column.getName());
            }
            if (column.getPrimaryKey()) {
                primaryKeyColumnNameList.add(column.getName());
            }
            ColumnConst ndbColumn = ndbTable.getColumn(i);
            String columnName = ndbColumn.getName();
            ColumnImpl columnImpl = new ColumnImpl(tableName, ndbColumn);
            columns.put(columnName, columnImpl);
            columnImpls[i] = columnImpl;
            columnNames[i] = columnName;
            // find maximum column id
            int columnId = ndbColumn.getColumnNo();
            if (columnId > maximumColumnId) {
                maximumColumnId = columnId;
            }
        }
        // iterate columns again and construct layout of record in memory
        offsets = new int[maximumColumnId + 1];
        lengths = new int[maximumColumnId + 1];
        int offset = 0;
        for (int i = 0; i < noOfColumns; ++i) {
            ColumnImpl columnImpl = columnImpls[i];
            int columnId = columnImpl.getColumnId();
            int columnSpace = columnImpl.getColumnSpace();
            lengths[columnId] = columnSpace;
            offsets[columnId] = offset;
            offset += columnSpace;
            if (columnSpace > maximumColumnLength ) {
                maximumColumnLength = columnSpace;
            }
        }
        bufferSize = offset;
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

    public String[] getColumnNames() {
        return columnNames;
    }

    public int getMaximumColumnId() {
        return maximumColumnId;
    }

    public int getBufferSize() {
        return bufferSize;
    }

    public int[] getOffsets() {
        return offsets;
    }

    public int[] getLengths() {
        return lengths;
    }

    public int getMaximumColumnLength() {
        return maximumColumnLength;
    }

}
