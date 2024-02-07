/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.core.query;

import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;

public class EqualPredicateImpl extends ComparativePredicateImpl {

    public EqualPredicateImpl(QueryDomainTypeImpl<?> dobj,
            PropertyImpl property, ParameterImpl param) {
        super(dobj, property, param);
    }

    @Override
    public void markBoundsForCandidateIndices(QueryExecutionContext context, CandidateIndexImpl[] candidateIndices) {
        if (param.getParameterValue(context) == null) {
            // null parameters cannot be used with indexes
            return;
        }
        property.markEqualBound(candidateIndices, this);
    }

    @Override
    public void markBoundsForCandidateIndices(CandidateIndexImpl[] candidateIndices) {
        property.markEqualBound(candidateIndices, this);
    }

    @Override
    public int operationSetBounds(QueryExecutionContext context, IndexScanOperation op, boolean lastColumn) {
        // can always set boundEQ
        Object value = param.getParameterValue(context);
        if (value != null) {
            property.operationSetBounds(value, IndexScanOperation.BoundType.BoundEQ, op);
            return BOTH_BOUNDS_SET;
        } else {
            return NO_BOUND_SET;
        }
    }

    @Override
    public int operationSetLowerBound(QueryExecutionContext context, IndexScanOperation op, boolean lastColumn) {
        // only set lower bound
        Object value = param.getParameterValue(context);
        if (value != null) {
            property.operationSetBounds(value, IndexScanOperation.BoundType.BoundLE, op);
            return LOWER_BOUND_SET;
        } else {
            return NO_BOUND_SET;
        }
    }

    @Override
    public int operationSetUpperBound(QueryExecutionContext context, IndexScanOperation op, boolean lastColumn) {
        // only set upper bound
        Object value = param.getParameterValue(context);
        if (value != null) {
            property.operationSetBounds(param.getParameterValue(context), IndexScanOperation.BoundType.BoundGE, op);
            return UPPER_BOUND_SET;
        } else {
            return NO_BOUND_SET;
        }
    }

    @Override
    public void operationEqual(QueryExecutionContext context,
            Operation op) {
        property.operationEqual(param.getParameterValue(context), op);
    }

    @Override
    public void operationEqualFor(QueryExecutionContext context,
            Operation op, String indexName) {
        property.operationEqualFor(param.getParameterValue(context), op, indexName);
    }

    /** Set the condition into the filter.
     * @param context the query execution context with the parameter values
     * @param op the operation
     * @param filter the filter
     */
    @Override
    public void filterCmpValue(QueryExecutionContext context, ScanOperation op, ScanFilter filter) {
        property.filterCmpValue(param.getParameterValue(context),
                ScanFilter.BinaryCondition.COND_EQ, filter);
    }

}
