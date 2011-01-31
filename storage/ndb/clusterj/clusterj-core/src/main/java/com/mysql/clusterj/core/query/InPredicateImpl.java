/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.ScanFilter.BinaryCondition;
import com.mysql.clusterj.core.store.ScanFilter.Group;

public class InPredicateImpl extends PredicateImpl {

    /** The property */
    protected PropertyImpl property;

    /** The parameter containing the values */
    protected ParameterImpl parameter;

    public InPredicateImpl(QueryDomainTypeImpl<?> dobj,
            PropertyImpl property, ParameterImpl parameter) {
        super(dobj);
        this.property = property;
        this.parameter = parameter;
    }

    @Override
    public void markParameters() {
        // Nothing to do because "in" can't use indexes
    }

    @Override
    public void unmarkParameters() {
        // Nothing to do because "in" can't use indexes
    }

    void markBoundsForCandidateIndices(QueryExecutionContextImpl context,
            CandidateIndexImpl[] candidateIndices) {
        // Nothing to do because "in" can't use indexes
    }

    /** Create a filter for the operation. Call the property to set the filter
     * from the parameter values.
     * @param context the query execution context with the parameter values
     * @param op the operation
     */
    public void filterCmpValue(QueryExecutionContextImpl context,
            ScanOperation op) {
        try {
            ScanFilter filter = op.getScanFilter(context);
            filterCmpValue(context, op, filter);
        } catch (ClusterJException ex) {
            throw ex;
        } catch (Exception ex) {
            throw new ClusterJException(
                    local.message("ERR_Get_NdbFilter"), ex);
        }
    }

    /** Use an existing filter for the operation. Call the property to set the filter
     * from the parameter values.
     * @param context the query execution context with the parameter values
     * @param op the operation
     * @param filter the existing filter
     */
    public void filterCmpValue(QueryExecutionContextImpl context,
            ScanOperation op, ScanFilter filter) {
        try {
            filter.begin(Group.GROUP_OR);
            Object parameterValue = parameter.getParameterValue(context);
            if (parameterValue == null) {
                throw new ClusterJUserException(
                        local.message("ERR_Parameter_Cannot_Be_Null", "operator in", parameter.parameterName));
            } else if (parameterValue instanceof Iterable<?>) {
                Iterable<?> iterable = (Iterable<?>)parameterValue;
                for (Object value: iterable) {
                    property.filterCmpValue(value, BinaryCondition.COND_EQ, filter);
                }
            } else if (parameterValue.getClass().isArray()) {
                Object[] parameterArray = (Object[])parameterValue;
                for (Object parameter: parameterArray) {
                    property.filterCmpValue(parameter, BinaryCondition.COND_EQ, filter);
                }
            } else {
                throw new ClusterJUserException(
                        local.message("ERR_Parameter_Wrong_Type", parameter.parameterName,
                                parameterValue.getClass().getName(), "Iterable<?> or Object[]"));
            }
            filter.end();
        } catch (ClusterJException ex) {
            throw ex;
        } catch (Exception ex) {
            throw new ClusterJException(
                    local.message("ERR_Get_NdbFilter"), ex);
        }
    }

}
