/*
 *  Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
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

import java.util.Arrays;

import com.mysql.ndbjtie.ndbapi.NdbDictionary.IndexConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 *
 */
class IndexImpl implements Index {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(IndexImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(IndexImpl.class);

    private String tableName;

    /** The name of the index as known by the schem and user */
    private String name;

    /** The number of columns in the index */
    private int noOfColumns;

    /** The names of the columns, in the order declared in the KEY clause */
    private String[] columnNames;

    /** Is the index unique? */
    private boolean unique;

    /** The actual name of the index, e.g. idx_name$unique */
    private String internalName;

    /**
     * Create a new IndexImpl for the index.
     * @param ndbIndex the ndbIndex for this index
     * @param indexAlias the name as known by the schema
     */
    public IndexImpl(IndexConst ndbIndex, String indexAlias) {
        this.internalName = ndbIndex.getName();
        this.tableName = ndbIndex.getTable();
        this.name = indexAlias;
        this.unique = ndbIndex.getType() == IndexConst.Type.UniqueHashIndex;
        this.noOfColumns = ndbIndex.getNoOfColumns();
        this.columnNames = new String[noOfColumns];
        for (int i = 0; i < noOfColumns; ++i) {
          String columnName = ndbIndex.getColumn(i).getName();
          this.columnNames[i] = columnName;
        }
        if (logger.isDetailEnabled()) logger.detail(toString());
    }

    /** Create a pseudo IndexImpl for the primary key. The primary key pseudo index
     * is not an index but is treated as an index by query. This index is not used
     * with an index scan but is only used for primary key lookup.
     * 
     * @param ndbTable the ndb Table
     */
    public IndexImpl(TableConst ndbTable) {
        this.internalName = "PRIMARY";
        this.name = "PRIMARY";
        this.tableName = ndbTable.getName();
        this.unique = true;
        this.noOfColumns = ndbTable.getNoOfPrimaryKeys();
        this.columnNames = new String[noOfColumns];
        for (int i = 0; i < noOfColumns; ++i) {
            this.columnNames[i] = ndbTable.getPrimaryKey(i);
        }
        if (logger.isDetailEnabled()) logger.detail(toString());
    }

    public boolean isUnique() {
        return unique;
    }

    public String getName() {
        return name;
    }

    public String getInternalName() {
        return internalName;
    }

    public String[] getColumnNames() {
        return columnNames;
    }

    @Override
    public String toString() {
        return "IndexImpl name: " + name + " internal name: " + internalName + " table: " + tableName + " unique: " + unique + " columns: " + Arrays.toString(columnNames);
    }

}
