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

    public void markParameters() {
        param.mark();
    }

    public void unmarkParameters() {
        param.unmark();
    }

    @Override
    public void objectSetValuesFor(QueryExecutionContextImpl context,
            Object row, String indexName) {
        property.objectSetValuesFor(param.getParameterValue(context), row, indexName);
    }

    @Override
    public void operationSetLowerBound(QueryExecutionContextImpl context,
            IndexScanOperation op) {
        // delegate to setBounds for most operations
        operationSetBounds(context, op);
    }

    @Override
    public void operationSetUpperBound(QueryExecutionContextImpl context,
            IndexScanOperation op) {
        // delegate to setBounds for most operations
        operationSetBounds(context, op);
    }

}
