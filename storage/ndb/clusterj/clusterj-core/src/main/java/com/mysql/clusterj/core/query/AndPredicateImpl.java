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

import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.ClusterJException;
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
        } else {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
        }
    }

    public Predicate or(Predicate predicate) {
        throw new UnsupportedOperationException(
                local.message("ERR_NotImplemented"));
    }

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
    public void filterCmpValue(QueryExecutionContextImpl context,
            ScanOperation op) {
        try {
            ScanFilter filter = op.getScanFilter();
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

    /** Get the best index for the operation. Delegate to the method
     * in the superclass, passing the array of predicates.
     *
     * @return the best index
     */
    @Override
    public CandidateIndexImpl getBestCandidateIndex() {
        return getBestCandidateIndexFor(predicates.toArray(
                new PredicateImpl[predicates.size()]));
    }

}
