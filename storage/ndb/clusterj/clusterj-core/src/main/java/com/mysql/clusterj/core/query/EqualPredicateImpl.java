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

package com.mysql.clusterj.core.query;

import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;

public class EqualPredicateImpl extends ComparativePredicateImpl {

    public EqualPredicateImpl(QueryDomainTypeImpl<?> dobj,
            PropertyImpl property, ParameterImpl param) {
        super(dobj);
        this.param = param;
        this.property = property;
    }

    @Override
    public void markBoundsForCandidateIndices(QueryExecutionContextImpl context, CandidateIndexImpl[] candidateIndices) {
        if (param.getParameterValue(context) == null) {
            // null parameters cannot be used with indexes
            return;
        }
        property.markEqualBound(candidateIndices, this);
    }

    @Override
    public void operationSetBounds(QueryExecutionContextImpl context, IndexScanOperation op, boolean lastColumn) {
        // can always set boundEQ
        property.operationSetBounds(param.getParameterValue(context), IndexScanOperation.BoundType.BoundEQ, op);
    }

    @Override
    public void operationSetLowerBound(QueryExecutionContextImpl context, IndexScanOperation op, boolean lastColumn) {
        // only set lower bound
        property.operationSetBounds(param.getParameterValue(context), IndexScanOperation.BoundType.BoundLE, op);
    }

    @Override
    public void operationSetUpperBound(QueryExecutionContextImpl context, IndexScanOperation op, boolean lastColumn) {
        // only set upper bound
        property.operationSetBounds(param.getParameterValue(context), IndexScanOperation.BoundType.BoundGE, op);
    }

    @Override
    public void operationEqual(QueryExecutionContextImpl context,
            Operation op) {
        property.operationEqual(param.getParameterValue(context), op);
    }

    @Override
    public void operationEqualFor(QueryExecutionContextImpl context,
            Operation op, String indexName) {
        property.operationEqualFor(param.getParameterValue(context), op, indexName);
    }

    /** Set the condition into the filter.
     * @param context the query execution context with the parameter values
     * @param op the operation
     * @param filter the filter
     */
    @Override
    public void filterCmpValue(QueryExecutionContextImpl context, ScanOperation op, ScanFilter filter) {
        property.filterCmpValue(param.getParameterValue(context),
                ScanFilter.BinaryCondition.COND_EQ, filter);
    }

}
