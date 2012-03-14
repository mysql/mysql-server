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
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;

import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.clusterj.query.Predicate;

public abstract class PredicateImpl implements Predicate {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(PredicateImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(PredicateImpl.class);

    /** My domain object. */
    protected QueryDomainTypeImpl<?> dobj;

    /** Scan types. */
    protected enum ScanType {
        INDEX_SCAN,
        TABLE_SCAN,
        UNIQUE_KEY,
        PRIMARY_KEY
    }

    public PredicateImpl(QueryDomainTypeImpl<?> dobj) {
        this.dobj = dobj;
    }

    public Predicate or(Predicate other) {
        assertPredicateImpl(other);
        PredicateImpl otherPredicateImpl = (PredicateImpl)other;
        assertIdenticalDomainObject(otherPredicateImpl, "or");
        return new OrPredicateImpl(this.dobj, this, otherPredicateImpl);
    }

    public Predicate and(Predicate other) {
        assertPredicateImpl(other);
        PredicateImpl predicateImpl = (PredicateImpl)other;
        assertIdenticalDomainObject(predicateImpl, "and");
        if (other instanceof AndPredicateImpl) {
            AndPredicateImpl andPredicateImpl = (AndPredicateImpl)other;
            return andPredicateImpl.and(this);
        } else {
            return new AndPredicateImpl(dobj, this, predicateImpl);
        }
    }

    public Predicate not() {
        return new NotPredicateImpl(this);
    }

    void markBoundsForCandidateIndices(QueryExecutionContext context, CandidateIndexImpl[] candidateIndices) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void operationSetBounds(QueryExecutionContext context,
            IndexScanOperation op, boolean lastColumn) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void operationSetLowerBound(QueryExecutionContext context,
            IndexScanOperation op, boolean lastColumn) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void operationSetUpperBound(QueryExecutionContext context,
            IndexScanOperation op, boolean lastColumn){
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void operationEqual(QueryExecutionContext context,
            Operation op) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void operationEqualFor(QueryExecutionContext context,
            Operation op, String indexName) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void objectSetValuesFor(QueryExecutionContext context,
            Object row, String indexName) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    /** Create a filter for the operation. Set the condition into the
     * new filter.
     * @param context the query execution context with the parameter values
     * @param op the operation
     */
    public void filterCmpValue(QueryExecutionContext context,
            ScanOperation op) {
        try {
            ScanFilter filter = op.getScanFilter(context);
            filter.begin();
            filterCmpValue(context, op, filter);
            filter.end();
        } catch (ClusterJException ex) {
            throw ex;
        } catch (Exception ex) {
            throw new ClusterJException(
                    local.message("ERR_Get_NdbFilter"), ex);
        }
    }

    public void filterCmpValue(QueryExecutionContext context,
            ScanOperation op, ScanFilter filter) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void assertIdenticalDomainObject(PredicateImpl other, String venue) {
        QueryDomainTypeImpl<?> otherDomainObject = other.getDomainObject();
        if (dobj != otherDomainObject) {
            throw new ClusterJUserException(
                    local.message("ERR_Wrong_Domain_Object", venue));
        }
    }

    /** Mark this predicate as being satisfied. */
    void setSatisfied() {
        throw new UnsupportedOperationException("Not yet implemented");
    }

    /** Mark all parameters as being required. */
    public abstract void markParameters();

    /** Unmark all parameters as being required. */
    public abstract void unmarkParameters();

    private void assertPredicateImpl(Predicate other) {
        if (!(other instanceof PredicateImpl)) {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
        }
    }

    private QueryDomainTypeImpl<?> getDomainObject() {
        return dobj;
    }

    public CandidateIndexImpl getBestCandidateIndex(QueryExecutionContext context) {
        return getBestCandidateIndexFor(context, this);
    }

    /** Get the best candidate index for the query, considering all indices
     * defined and all predicates in the query.
     * @param predicates the predicates
     * @return the best index for the query
     */
    protected CandidateIndexImpl getBestCandidateIndexFor(QueryExecutionContext context,
            PredicateImpl... predicates) {
        // Create CandidateIndexImpl to decide how to scan.
        CandidateIndexImpl[] candidateIndices = dobj.createCandidateIndexes();
        // Iterate over predicates and have each one register with
        // candidate indexes.
        for (PredicateImpl predicateImpl : predicates) {
            predicateImpl.markBoundsForCandidateIndices(context, candidateIndices);
        }
        // Iterate over candidate indices to find one that is usable.
        int highScore = 0;
        // Holder for the best index; default to the index for null where clause
        CandidateIndexImpl bestCandidateIndexImpl = 
                CandidateIndexImpl.getIndexForNullWhereClause();
        // Hash index operations require the predicates to have no extra conditions
        // beyond the index columns.
        int numberOfConditions = getNumberOfConditionsInPredicate();
        for (CandidateIndexImpl candidateIndex : candidateIndices) {
            if (candidateIndex.supportsConditionsOfLength(numberOfConditions)) {
                // opportunity for a user-defined plugin to evaluate indices
                int score = candidateIndex.getScore();
                if (logger.isDetailEnabled()) {
                    logger.detail("Score: " + score + " from " + candidateIndex);
                }
                if (score > highScore) {
                    bestCandidateIndexImpl = candidateIndex;
                    highScore = score;
                }
            }
        }
        if (logger.isDetailEnabled()) logger.detail("High score: " + highScore
                + " from " + bestCandidateIndexImpl.getIndexName());
        return bestCandidateIndexImpl;
    }

    /** Get the number of conditions in the top level predicate.
     * This is used to determine whether a hash index can be used. If there
     * are exactly the number of conditions as index columns, then the
     * hash index might be used.
     * By default (for equal, greaterThan, lessThan, greaterEqual, lessEqual)
     * there is one condition.
     * AndPredicateImpl overrides this method.
     * @return the number of conditions
     */
    protected int getNumberOfConditionsInPredicate() {
        return 1;
    }

}
