/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.ScanFilter.Group;
import com.mysql.clusterj.query.Predicate;

public class OrPredicateImpl extends PredicateImpl {

    /** The predicates being ORed */
    List<PredicateImpl> predicates = new ArrayList<PredicateImpl>();

    /** Create an OrPredicateImpl from two predicates.
     * 
     * @param dobj the QueryDomainObject
     * @param left one predicate
     * @param right the other predicate
     */
    public OrPredicateImpl(QueryDomainTypeImpl<?> dobj,
            PredicateImpl left, PredicateImpl right) {
        super(dobj);
        predicates.add(left);
        predicates.add(right);
    }

    @Override
    public Predicate or(Predicate predicate) {
        if (predicate instanceof ComparativePredicateImpl) {
            predicates.add((PredicateImpl)predicate);
            return this;
        } else if (predicate instanceof AndPredicateImpl) {
            predicates.add((PredicateImpl)predicate);
            return this;
        } else if (predicate instanceof OrPredicateImpl) {
            predicates.addAll(((OrPredicateImpl)predicate).predicates);
            return this;
        } else if (predicate instanceof InPredicateImpl) {
            predicates.add((PredicateImpl)predicate);
            return this;
        } else {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
        }
    }

    @Override
    public void markParameters() {
        // Nothing to do because "or" can't use indexes
    }

    @Override
    public void unmarkParameters() {
        // Nothing to do because "or" can't use indexes
    }

    void markBoundsForCandidateIndices(QueryExecutionContext context,
            CandidateIndexImpl[] candidateIndices) {
        // Nothing to do because "or" can't use indexes
    }

    /** Create a filter for the operation. Call the ORed predicates to set
     * the filter values.
     * @param context the query execution context with the parameter values
     * @param op the operation
     */
    public void filterCmpValue(QueryExecutionContext context,
            ScanOperation op) {
        try {
            ScanFilter filter = op.getScanFilter(context);
            filter.begin(Group.GROUP_OR);
            for (PredicateImpl predicate: predicates) {
                predicate.filterCmpValue(context, op, filter);
            }
            filter.end();
        } catch (ClusterJException ex) {
            throw ex;
        } catch (Exception ex) {
            throw new ClusterJException(
                    local.message("ERR_Get_NdbFilter"), ex);
        }
    }

    /** Use an existing filter for the operation. Call the ORed predicates to set
     * the filter values.
     * @param context the query execution context with the parameter values
     * @param op the operation
     * @param filter the existing filter
     */
    public void filterCmpValue(QueryExecutionContext context,
            ScanOperation op, ScanFilter filter) {
        try {
            filter.begin(Group.GROUP_OR);
            for (PredicateImpl predicate: predicates) {
                predicate.filterCmpValue(context, op, filter);
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
