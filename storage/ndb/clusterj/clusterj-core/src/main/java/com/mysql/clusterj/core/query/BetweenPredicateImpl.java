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

import com.mysql.clusterj.ClusterJException;

import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;

/** Implement the between operator with a property and two parameters.
 *
 * 
 */
public class BetweenPredicateImpl extends PredicateImpl {

    /** The lower and upper bound parameters */
    protected ParameterImpl lower;
    protected ParameterImpl upper;
    /** The property to compare with */
    protected PropertyImpl property;

    public BetweenPredicateImpl(QueryDomainTypeImpl<?> dobj,
            PropertyImpl property, ParameterImpl lower, ParameterImpl upper) {
        super(dobj);
        this.lower = lower;
        this.upper = upper;
        this.property = property;
        lower.setProperty(property);
        upper.setProperty(property);
    }

    @Override
    public void markParameters() {
        lower.mark();
        upper.mark();
    }

    @Override
    public void unmarkParameters() {
        lower.unmark();
        upper.unmark();
    }

    @Override
    public void markBoundsForCandidateIndices(QueryExecutionContext context, CandidateIndexImpl[] candidateIndices) {
        if (lower.getParameterValue(context) == null || upper.getParameterValue(context) == null) {
            // null parameters cannot be used with index scans
            return;
        }
        property.markLowerBound(candidateIndices, this, false);
        property.markUpperBound(candidateIndices, this, false);
    }

    @Override
    public void markBoundsForCandidateIndices(CandidateIndexImpl[] candidateIndices) {
        property.markLowerBound(candidateIndices, this, false);
        property.markUpperBound(candidateIndices, this, false);
    }

    /** Set the upper and lower bounds for the operation.
     * Delegate to the property to actually call the setBounds for each
     * of upper and lower bound.
     * @param context the query context that contains the parameter values
     * @param op the index scan operation on which to set bounds
     * @return an indicator of which bound(s) were actually set
     */
    @Override
    public int operationSetBounds(QueryExecutionContext context,
            IndexScanOperation op, boolean lastColumn) {
        int result = NO_BOUND_SET;
        Object lowerValue = lower.getParameterValue(context);
        Object upperValue = upper.getParameterValue(context);
        if (lowerValue != null) {
            property.operationSetBounds(lowerValue,
                    IndexScanOperation.BoundType.BoundLE, op);
            result |= LOWER_BOUND_SET;
        }
        if (upperValue != null) {
            property.operationSetBounds(upperValue,
                    IndexScanOperation.BoundType.BoundGE, op);
            result |= UPPER_BOUND_SET;
        }
        return result;
    }

    /** Set the upper bound for the operation.
     * Delegate to the property to actually call the setBounds
     * for the upper bound.
     * @param context the query context that contains the parameter values
     * @param op the index scan operation on which to set bounds
     */
    @Override
    public int operationSetUpperBound(QueryExecutionContext context,
            IndexScanOperation op, boolean lastColumn) {
        Object upperValue = upper.getParameterValue(context);
        if (upperValue != null) {
            property.operationSetBounds(upperValue,
                    IndexScanOperation.BoundType.BoundGE, op);
            return UPPER_BOUND_SET;
        }
        return NO_BOUND_SET;
    }

    /** Set the lower bound for the operation.
     * Delegate to the property to actually call the setBounds
     * for the lower bound.
     * @param context the query context that contains the parameter values
     * @param op the index scan operation on which to set bounds
     */
    @Override
    public int operationSetLowerBound(QueryExecutionContext context,
            IndexScanOperation op, boolean lastColumn) {
        Object lowerValue = lower.getParameterValue(context);
        if (lowerValue != null) {
            property.operationSetBounds(lowerValue,
                    IndexScanOperation.BoundType.BoundLE, op);
            return LOWER_BOUND_SET;
        }
        return NO_BOUND_SET;
    }

    /** Create a filter for the operation. Set the condition into the
     * new filter.
     * @param context the query execution context with the parameter values
     * @param op the operation
     */
    @Override
    public void filterCmpValue(QueryExecutionContext context,
            ScanOperation op) {
        try {
            ScanFilter filter = op.getScanFilter(context);
            filter.begin();
            filterCmpValue(context, op, filter);
            filter.end();
        } catch (Exception ex) {
            throw new ClusterJException(
                    local.message("ERR_Get_NdbFilter"), ex);
        }
    }

    /** Set the condition into the filter.
     * @param context the query execution context with the parameter values
     * @param op the operation
     * @param filter the filter
     */
    @Override
    public void filterCmpValue(QueryExecutionContext context,
            ScanOperation op, ScanFilter filter) {
        property.filterCmpValue(lower.getParameterValue(context),
                ScanFilter.BinaryCondition.COND_GE, filter);
        property.filterCmpValue(upper.getParameterValue(context),
                ScanFilter.BinaryCondition.COND_LE, filter);
    }

    @Override 
    public boolean isUsable(QueryExecutionContext context) {
        return !(lower.getParameterValue(context) == null || upper.getParameterValue(context) == null);
    }

    @Override
    protected PropertyImpl getProperty() {
        return property;
    }

}
