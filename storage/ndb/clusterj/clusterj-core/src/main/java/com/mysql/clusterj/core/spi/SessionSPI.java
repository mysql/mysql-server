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

package com.mysql.clusterj.core.spi;

import com.mysql.clusterj.Session;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.IndexOperation;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.Table;
import com.mysql.clusterj.query.QueryDomainType;

import java.util.BitSet;

/**
 *
 */
public interface SessionSPI extends Session {

    <T> T initializeFromDatabase(
            DomainTypeHandler<T> domainTypeHandler, T object,
            ValueHandler valueHandler, ValueHandler createKeyValueHandler);

    public ResultData selectUnique(DomainTypeHandler<?> domainTypeHandler,
            ValueHandler keyHandler, BitSet fields);

    Operation insert(DomainTypeHandler<?> domainTypeHandler, ValueHandler valueHandler);

    Operation update(DomainTypeHandler<?> domainTypeHandler, ValueHandler valueHandler);

    Operation delete(DomainTypeHandler<?> domainTypeHandler, ValueHandler valueHandler);

    int deletePersistentAll(DomainTypeHandler<?> domainTypeHandler);

    int deletePersistentAll(ScanOperation op, boolean abort);

    void begin();

    void commit();

    void rollback();

    void setRollbackOnly();

    boolean getRollbackOnly();

    void startAutoTransaction();

    void endAutoTransaction();

    void failAutoTransaction();

    void executeNoCommit();

    void executeNoCommit(boolean abort, boolean force);

    Operation getSelectOperation(Table storeTable);

    Operation getDeleteOperation(Table storeTable);

    IndexOperation getUniqueIndexOperation(Index storeIndex, Table storeTable);

    IndexOperation getUniqueIndexDeleteOperation(Index storeIndex, Table storeTable);

    IndexScanOperation getIndexScanOperation(Index storeIndex, Table storeTable);

    IndexScanOperation getIndexScanDeleteOperation(Index storeIndex, Table storeTable);

    IndexScanOperation getIndexScanOperationMultiRange(Index storeIndex, Table storeTable);

    ScanOperation getTableScanOperation(Table storeTable);

    ScanOperation getTableScanDeleteOperation(Table storeTable);

    Dictionary getDictionary();

    <T> QueryDomainType<T> createQueryDomainType(DomainTypeHandler<T> handler);

    String getCoordinatedTransactionId();

    void setCoordinatedTransactionId(String coordinatedTransactionId);

    boolean isEnlisted();

}
