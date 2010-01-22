/*
   Copyright (C) 2009 Sun Microsystems Inc.
   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.core.store;

/**
 *
 */
public interface ClusterTransaction {

    public void close();

    public void executeCommit();

    public void executeCommit(boolean abort, boolean force);

    public void executeNoCommit(boolean abort, boolean force);

    public void executeNoCommit();

    public void executeRollback();

    public Operation getDeleteOperation(Table storeTable);

    public Operation getInsertOperation(Table storeTable);

    public IndexScanOperation getSelectIndexScanOperation(Index storeIndex, Table storeTable);

    public Operation getSelectOperation(Table storeTable);

    public ScanOperation getSelectScanOperation(Table storeTable);

    public ScanOperation getSelectScanOperationLockModeExclusiveScanFlagKeyInfo(Table storeTable);

    public IndexOperation getSelectUniqueOperation(Index storeIndex, Table storeTable);

    public Operation getUpdateOperation(Table storeTable);

    public Operation getWriteOperation(Table storeTable);

    public boolean isEnlisted();

    public void setPartitionKey(PartitionKey partitionKey);

}
