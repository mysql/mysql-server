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
import java.util.Arrays;
import java.util.List;

import com.mysql.ndbjtie.ndbapi.NdbDictionary.ColumnConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.IndexConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;

import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

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

    private TableConst ndbTable;

    /** The table name */
    private String name;

    /** The primary key column names */
    private String[] primaryKeyColumnNames;

    public TableImpl(TableConst ndbTable, IndexConst primaryKeyIndex) {
        this.ndbTable = ndbTable;
        this.name = ndbTable.getName();
        if (primaryKeyIndex == null) {
            primaryKeyColumnNames = new String[0];
        } else {
            int numberOfPrimaryKeyColumns = primaryKeyIndex.getNoOfColumns();
            primaryKeyColumnNames = new String[numberOfPrimaryKeyColumns];
            int columnsInPrimaryKeyIndex = primaryKeyIndex.getNoOfColumns();
            if (columnsInPrimaryKeyIndex != numberOfPrimaryKeyColumns) {
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Mismatch_No_Of_Primary_Key_Columns",
                                name, numberOfPrimaryKeyColumns, columnsInPrimaryKeyIndex));
            }
            for (int i = 0; i < numberOfPrimaryKeyColumns; ++i) {
                primaryKeyColumnNames[i] = primaryKeyIndex.getColumn(i).getName();
            }
        }
    }

    public ColumnImpl getColumn(String columnName) {
        ColumnConst ndbColumn = ndbTable.getColumn(columnName);
        if (ndbColumn == null) {
            return null;
        }
        return new ColumnImpl(ndbTable, ndbColumn);
    }

    public String getName() {
        return name;
    }

    public TableConst getNdbTable() {
        return ndbTable;
    }

    public String[] getPrimaryKeyColumnNames() {
        return primaryKeyColumnNames;
    }

    public String[] getPartitionKeyColumnNames() {
        int numberOfColumns = ndbTable.getNoOfColumns();
        List<String> partitionKeyColumnNames = new ArrayList<String>();
        for (int i = 0; i < numberOfColumns; ++i) {
            ColumnConst candidatePartitionKey = ndbTable.getColumn(i);
            if (candidatePartitionKey.getPartitionKey()) {
                partitionKeyColumnNames.add(candidatePartitionKey.getName());
            }
        }
        String[] result = partitionKeyColumnNames.toArray(new String[partitionKeyColumnNames.size()]);
        return result;
    }

    public PartitionKey createPartitionKey() {
        PartitionKey result = new PartitionKeyImpl();
        result.setTable(this);
        return result;
    }


}
