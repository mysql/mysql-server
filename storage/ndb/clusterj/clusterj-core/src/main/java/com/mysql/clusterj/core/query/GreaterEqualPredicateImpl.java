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

package com.mysql.clusterj.core.query;

import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;

public class GreaterEqualPredicateImpl extends ComparativePredicateImpl {

    public GreaterEqualPredicateImpl(QueryDomainTypeImpl<?> dobj,
            PropertyImpl property, ParameterImpl param) {
        super(dobj);
        this.param = param;
        this.property = property;
    }

    @Override
    public void markBoundsForCandidateIndices(CandidateIndexImpl[] candidateIndices) {
        property.markLowerBound(candidateIndices, this, false);
    }

    @Override
    public void operationSetBounds(QueryExecutionContextImpl context,
            IndexScanOperation op) {
        property.operationSetBounds(param.getParameterValue(context),
                IndexScanOperation.BoundType.BoundLE, op);
    }

    /** Set the condition into the filter.
     * @param context the query execution context with the parameter values
     * @param op the operation
     * @param filter the filter
     */
    @Override
    public void filterCmpValue(QueryExecutionContextImpl context,
            ScanOperation op, ScanFilter filter) {
        property.filterCmpValue(param.getParameterValue(context),
                ScanFilter.BinaryCondition.COND_GE, filter);
    }

}
