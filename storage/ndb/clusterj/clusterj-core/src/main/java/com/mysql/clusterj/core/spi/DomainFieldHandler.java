/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.core.spi;

import com.mysql.clusterj.core.query.CandidateIndexImpl;
import com.mysql.clusterj.core.query.InPredicateImpl;
import com.mysql.clusterj.core.query.PredicateImpl;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.IndexScanOperation.BoundType;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanFilter.BinaryCondition;

/**
 *
 */
public interface DomainFieldHandler {

    void filterCompareValue(Object value, BinaryCondition condition, ScanFilter filter);

    void markEqualBounds(CandidateIndexImpl[] candidateIndices, PredicateImpl predicate);

    void markInBounds(CandidateIndexImpl[] candidateIndices, InPredicateImpl predicate);

    void markLowerBounds(CandidateIndexImpl[] candidateIndices, PredicateImpl predicate, boolean strict);

    void markUpperBounds(CandidateIndexImpl[] candidateIndices, PredicateImpl predicate, boolean strict);

    void objectSetValueFor(Object value, Object row, String indexName);

    void operationEqual(Object value, Operation op);

    void operationEqualForIndex(Object parameterValue, Operation op, String indexName);

    void operationSetBounds(Object value, BoundType type, IndexScanOperation op);

    void operationSetValue(ValueHandler handler, Operation op);

    void objectSetKeyValue(Object keys, ValueHandler handler);

    void objectSetValue(ResultData rs, ValueHandler handler);

    void objectSetValue(Object value, ValueHandler handler);

    Object objectGetValue(ValueHandler handler);

    Class<?> getType();

    String getName();

    String getColumnName();

    int getFieldNumber();

    void partitionKeySetPart(PartitionKey result, ValueHandler handler);

    boolean includedInIndex(String index);

    void operationSetModifiedValue(ValueHandler handler, Operation op);

    void operationGetValue(Operation op);

    void objectSetValueExceptIndex(ResultData rs, ValueHandler handler,
            String indexName);

    boolean isPrimaryKey();

    boolean isPartitionKey();

    boolean isPersistent();

    boolean isLob();

    Object getValue(QueryExecutionContext context, String parameterName);

    void filterIsNull(ScanFilter filter);

    void filterIsNotNull(ScanFilter filter);

}
