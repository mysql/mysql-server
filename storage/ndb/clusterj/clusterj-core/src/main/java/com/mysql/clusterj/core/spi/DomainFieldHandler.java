/*
   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

    Class<?> getType();

    String getName();

    int getFieldNumber();

    void partitionKeySetPart(PartitionKey result, ValueHandler handler);

    boolean includedInIndex(String index);

    void operationSetModifiedValue(ValueHandler handler, Operation op);

    void operationGetValue(Operation op);

    void objectSetValueExceptIndex(ResultData rs, ValueHandler handler,
            String indexName);

    boolean isPrimaryKey();

    Object getValue(QueryExecutionContext context, String parameterName);

}
