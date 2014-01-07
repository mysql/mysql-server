/*
   Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.query.Predicate;
import java.util.ArrayList;
import java.util.List;

public class AndPredicateImpl extends PredicateImpl {

    List<PredicateImpl> predicates = new ArrayList<PredicateImpl>();

    public AndPredicateImpl(QueryDomainTypeImpl<?> dobj,
            PredicateImpl left, PredicateImpl right) {
        super(dobj);
        predicates.add(left);
        predicates.add(right);
    }

    @Override
    public Predicate and(Predicate predicate) {
        if (predicate instanceof ComparativePredicateImpl) {
            predicates.add((PredicateImpl)predicate);
            return this;
        } else if (predicate instanceof AndPredicateImpl) {
            predicates.addAll(((AndPredicateImpl)predicate).predicates);
            return this;
        } else if (predicate instanceof OrPredicateImpl) {
            predicates.add((PredicateImpl)predicate);
            return this;
        } else if (predicate instanceof InPredicateImpl) {
            predicates.add((PredicateImpl)predicate);
            return this;
        } else if (predicate instanceof NotPredicateImpl) {
            predicates.add((PredicateImpl)predicate);
            return this;
        } else if (predicate instanceof BetweenPredicateImpl) {
            predicates.add((PredicateImpl)predicate);
            return this;
        } else {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
        }
    }

    @Override
    public Predicate or(Predicate predicate) {
        throw new UnsupportedOperationException(
                local.message("ERR_NotImplemented"));
    }

    @Override
    public Predicate not() {
        throw new UnsupportedOperationException(
                local.message("ERR_NotImplemented"));
    }

    @Override
    public void markParameters() {
        for (PredicateImpl predicateImpl: predicates) {
            predicateImpl.markParameters();
        }
    }

    @Override
    public void unmarkParameters() {
        for (PredicateImpl predicateImpl: predicates) {
            predicateImpl.unmarkParameters();
        }
    }

    /** Create a filter for the operation. Set the conditions into the
     * new filter, one for each predicate.
     * @param context the query execution context with the parameter values
     * @param op the operation
     */
    @Override
    public void filterCmpValue(QueryExecutionContext context,
            ScanOperation op) {
        try {
            ScanFilter filter = op.getScanFilter(context);
            filter.begin(ScanFilter.Group.GROUP_AND);
            for (PredicateImpl predicate: predicates) {
                predicate.filterCmpValue(context, op, filter);
            }
            filter.end();
        } catch (Exception ex) {
            throw new ClusterJException(
                    local.message("ERR_Get_NdbFilter"), ex);
        }
    }

    /** Set the keys into the operation for each predicate.
     * Each predicate must be an equal predicate for a primary or unique key.
     */
    @Override
    public void operationEqual(QueryExecutionContext context,
            Operation op) {
        for (PredicateImpl predicate: predicates) {
            if (!(predicate instanceof EqualPredicateImpl)) {
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Implementation_Should_Not_Occur"));
            }
            predicate.operationEqual(context, op);
        }
    }

    /** Get the number of conditions in the top level predicate.
     * This is used to determine whether a hash index can be used. If there
     * are exactly the number of conditions as index columns, then the
     * hash index might be used.
     * For AND predicates, there is one condition for each predicate included
     * in the AND.
     * AndPredicateImpl overrides this method.
     * @return the number of conditions
     */
    @Override
    protected int getNumberOfConditionsInPredicate() {
        return predicates.size();
    }

    /** Return an array of top level predicates that might be used with indices.
     * 
     * @return an array of top level predicates (defaults to {this}).
     */
    @Override
    protected PredicateImpl[] getTopLevelPredicates() {
        return predicates.toArray(new PredicateImpl[predicates.size()]);
    }

}
