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

import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.IndexScanOperation;

/** This is an abstract superclass for all of the comparison predicates:
 * GreaterEqualPredicate, GreaterThanPredicate, LessEqualPredicate, and
 * LessThanPredicate.
 */
public abstract class ComparativePredicateImpl extends PredicateImpl {
    /**
     * My parameter
     */
    protected ParameterImpl param;
    /**
     * My property
     */
    protected PropertyImpl property;

    public ComparativePredicateImpl(QueryDomainTypeImpl<?> dobj) {
        super(dobj);
    }

    public ComparativePredicateImpl(QueryDomainTypeImpl<?> dobj,
            PropertyImpl property, ParameterImpl param) {
        super(dobj);
        this.property = property;
        this.param = param;
        param.setProperty(property);
    }

    @Override
    public void markParameters() {
        param.mark();
    }

    @Override
    public void unmarkParameters() {
        param.unmark();
    }

    @Override
    public void objectSetValuesFor(QueryExecutionContext context,
            Object row, String indexName) {
        property.objectSetValuesFor(param.getParameterValue(context), row, indexName);
    }

    @Override
    public int operationSetLowerBound(QueryExecutionContext context,
            IndexScanOperation op, boolean lastColumn) {
        // delegate to setBounds for most operations
        return operationSetBounds(context, op, lastColumn);
    }

    @Override
    public int operationSetUpperBound(QueryExecutionContext context,
            IndexScanOperation op, boolean lastColumn) {
        // delegate to setBounds for most operations
        return operationSetBounds(context, op, lastColumn);
    }

    @Override
    public ParameterImpl getParameter() {
        return param;
    }

    @Override
    protected PropertyImpl getProperty() {
        return property;
    }

    @Override 
    public boolean isUsable(QueryExecutionContext context) {
        return param.getParameterValue(context) != null;
    }

}
