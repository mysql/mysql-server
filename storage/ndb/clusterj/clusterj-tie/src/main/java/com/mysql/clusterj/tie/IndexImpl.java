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

import com.mysql.ndbjtie.ndbapi.NdbDictionary.ColumnConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.IndexConst;
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

    private IndexConst ndbIndex;
    private String name;
    private int noOfColumns;
    private ColumnConst[] columns;
    private int type;

    public IndexImpl(IndexConst index, String indexAlias) {
        this.ndbIndex = index;
        this.name = indexAlias;
        // do some analysis of the index
        this.noOfColumns = ndbIndex.getNoOfColumns();
        this.columns = new ColumnConst[noOfColumns];
        this.type = ndbIndex.getType();
        if (logger.isDetailEnabled()) {
            StringBuffer buffer = new StringBuffer("Index columns for " + name + ":");
            for (int i = 0; i < noOfColumns; ++i) {
                columns[i] = index.getColumn(i);
                buffer.append(columns[i].getName());
                buffer.append(" ");
            }
            logger.detail(buffer.toString());
        }
    }

    public boolean isHash() {
        return IndexConst.Type.UniqueHashIndex == type;
    }

    public boolean isUnique() {
        return IndexConst.Type.UniqueHashIndex == type;
    }

    public IndexConst getNdbIndex() {
    return ndbIndex;
    }

    public String getName() {
        return name;
    }

}
