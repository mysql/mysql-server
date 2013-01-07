/*
   Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.LockMode;

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

    public Operation getSelectOperation(Table storeTable);

    public Operation getInsertOperation(Table storeTable);

    public Operation getUpdateOperation(Table storeTable);

    public Operation getWriteOperation(Table storeTable);

    public Operation getDeleteOperation(Table storeTable);

    public IndexOperation getUniqueIndexOperation(Index storeIndex, Table storeTable);

    public IndexOperation getUniqueIndexDeleteOperation(Index storeIndex, Table storeTable);

    public IndexOperation getUniqueIndexUpdateOperation(Index storeIndex, Table storeTable);

    public IndexScanOperation getIndexScanOperation(Index storeIndex, Table storeTable);

    public IndexScanOperation getIndexScanOperationLockModeExclusiveScanFlagKeyInfo(Index storeIndex, Table storeTable);

    public IndexScanOperation getIndexScanOperationMultiRange(Index storeIndex, Table storeTable);

    public ScanOperation getTableScanOperation(Table storeTable);

    public ScanOperation getTableScanOperationLockModeExclusiveScanFlagKeyInfo(Table storeTable);

    public boolean isEnlisted();

    public void setPartitionKey(PartitionKey partitionKey);

    public String getCoordinatedTransactionId();

    public void setCoordinatedTransactionId(String coordinatedTransactionId);

    public void setLockMode(LockMode lockmode);

    public void setAutocommit(boolean autocommit);

    public void postExecuteCallback(Runnable postExecuteCallbackHandler);

}
