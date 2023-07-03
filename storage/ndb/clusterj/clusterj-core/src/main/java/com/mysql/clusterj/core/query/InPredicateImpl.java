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

package com.mysql.clusterj.core.query;

import java.util.List;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.IndexScanOperation.BoundType;
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
        parameter.setProperty(property);
        // mark this property as having complex values
        property.setComplexParameter();
    }

    @Override
    public void markParameters() {
        parameter.mark();
    }

    @Override
    public void unmarkParameters() {
        parameter.unmark();
    }

    @Override
    public void markBoundsForCandidateIndices(QueryExecutionContext context,
            CandidateIndexImpl[] candidateIndices) {
        if (parameter.getParameterValue(context) == null) {
            // null parameters cannot be used with index scans
            return;
        }
        property.markInBound(candidateIndices, this);
    }

    @Override
    public void markBoundsForCandidateIndices(CandidateIndexImpl[] candidateIndices) {
        property.markInBound(candidateIndices, this);
    }

    /** Set bound for the multi-valued parameter identified by the index.
     * 
     * @param context the query execution context
     * @param op the operation to set bounds on
     * @param index the index into the parameter list
     * @param lastColumn if true, can set strict bound
     */
    public int operationSetBound(
            QueryExecutionContext context, IndexScanOperation op, int index, boolean lastColumn) {
        if (lastColumn) {
            // last column can be strict
            return operationSetBound(context, op, index, BoundType.BoundEQ);
        } else {
            // not last column cannot be strict
            return operationSetBound(context, op, index, BoundType.BoundLE) +
                    operationSetBound(context, op, index, BoundType.BoundGE);
        }
    }

    public int operationSetUpperBound(QueryExecutionContext context, IndexScanOperation op, int index) {
        return operationSetBound(context, op, index, BoundType.BoundGE);
    }

    public int operationSetLowerBound(QueryExecutionContext context, IndexScanOperation op, int index) {
        return operationSetBound(context, op, index, BoundType.BoundLE);
    }

    private int operationSetBound(
            QueryExecutionContext context, IndexScanOperation op, int index, BoundType boundType) {
    Object parameterValue = parameter.getParameterValue(context);
        if (parameterValue == null) {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Cannot_Be_Null", "operator in", parameter.parameterName));
        } else if (parameterValue instanceof List<?>) {
            List<?> parameterList = (List<?>)parameterValue;
            Object value = parameterList.get(index);
            if (logger.isDetailEnabled()) logger.detail("InPredicateImpl.operationSetBound for " + property.fmd.getName() + " List index: " + index + " value: " + value + " boundType: " + boundType);
            property.operationSetBounds(value, boundType, op);
        } else if (parameterValue.getClass().isArray()) {
            Object[] parameterArray = (Object[])parameterValue;
            Object value = parameterArray[index];
            property.operationSetBounds(value, boundType, op);
            if (logger.isDetailEnabled()) logger.detail("InPredicateImpl.operationSetBound for " + property.fmd.getName() + "  array index: " + index + " value: " + value + " boundType: " + boundType);
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Wrong_Type", parameter.parameterName,
                            parameterValue.getClass().getName(), "List<?> or Object[]"));
        }
        return BOTH_BOUNDS_SET;
    }

    /** Set bounds for the multi-valued parameter identified by the index.
     * There is only one column in the bound, so set each bound and then end the bound.
     * 
     * @param context the query execution context
     * @param op the operation to set bounds on
     * @param index the index into the parameter list
     */
    public void operationSetAllBounds(QueryExecutionContext context,
            IndexScanOperation op) {
        Object parameterValue = parameter.getParameterValue(context);
        int index = 0;
        if (parameterValue == null) {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Cannot_Be_Null", "operator in", parameter.parameterName));
        } else if (parameterValue instanceof List<?>) {
            List<?> parameterList = (List<?>)parameterValue;
            for (Object value: parameterList) {
                property.operationSetBounds(value, BoundType.BoundEQ, op);
                if (logger.isDetailEnabled()) logger.detail("InPredicateImpl.operationSetAllBounds for List index: " + index + " value: " + value);
                op.endBound(index++);
            }
        } else if (parameterValue.getClass().isArray()) {
            Object[] parameterArray = (Object[])parameterValue;
            for (Object value: parameterArray) {
                property.operationSetBounds(value, BoundType.BoundEQ, op);
                if (logger.isDetailEnabled()) logger.detail("InPredicateImpl.operationSetAllBounds for array index: " + index + " value: " + value);
                op.endBound(index++);
            }
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Wrong_Type", parameter.parameterName,
                            parameterValue.getClass().getName(), "List<?> or Object[]"));
        }
    }

    /** Create a filter for the operation. Call the property to set the filter
     * from the parameter values.
     * @param context the query execution context with the parameter values
     * @param op the operation
     */
    @Override
    public void filterCmpValue(QueryExecutionContext context,
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
    @Override
    public void filterCmpValue(QueryExecutionContext context, ScanOperation op, ScanFilter filter) {
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
            } else if (Object[].class.isAssignableFrom(parameterValue.getClass())) {
                Object[] parameterArray = (Object[])parameterValue;
                for (Object value: parameterArray) {
                    property.filterCmpValue(value, BinaryCondition.COND_EQ, filter);
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

    public int getParameterSize(QueryExecutionContext context) {
        int result = 1;
        Object parameterValue = parameter.getParameterValue(context);
        if (parameterValue instanceof List<?>) {
            result = ((List<?>)parameterValue).size();
        }
        Class<?> cls = parameterValue.getClass();
        if (cls.isArray()) {
            if (!Object.class.isAssignableFrom(cls.getComponentType())) {
                throw new ClusterJUserException(local.message("ERR_Wrong_Parameter_Type_For_In",
                        property.fmd.getName()));
            }
            Object[] parameterArray = (Object[])parameterValue;
            result = parameterArray.length;
        }
        if (result > 4096) {
            throw new ClusterJUserException(local.message("ERR_Parameter_Too_Big_For_In",
                        property.fmd.getName(), result));
        }
        // single value parameter
        return result;
    }

    @Override 
    public boolean isUsable(QueryExecutionContext context) {
        return parameter.getParameterValue(context) != null;
    }

}
