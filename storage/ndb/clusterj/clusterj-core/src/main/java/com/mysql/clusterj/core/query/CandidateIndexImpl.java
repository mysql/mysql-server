/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.metadata.AbstractDomainFieldHandlerImpl;

import com.mysql.clusterj.core.query.PredicateImpl.ScanType;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is responsible for deciding whether an index can be used
 * for a specific query. An instance of this class is created to evaluate
 * a query and to decide whether the index can be used in executing the
 * query. An inner class represents each candidateColumn in the index.
 * 
 * An instance of this class is created for each index for each query, and an instance of
 * the candidate column for each column of the index. During execution of the query,
 * the query terms are used to mark the candidate columns and associate candidate columns
 * with each query term. Each query term might be associated with multiple candidate columns,
 * one for each index containing the column referenced by the query term.
 * 
 */
public final class CandidateIndexImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(CandidateIndexImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(CandidateIndexImpl.class);

    private String className = "none";
    private Index storeIndex;
    private String indexName = "none";
    private boolean unique;
    private boolean multiRange = false;
    private CandidateColumnImpl[] candidateColumns = null;
    private ScanType scanType = PredicateImpl.ScanType.TABLE_SCAN;
    private int fieldScore = 1;
    protected int score = 0;
    private boolean canBound = true;

    public CandidateIndexImpl(
            String className, Index storeIndex, boolean unique, AbstractDomainFieldHandlerImpl[] fields) {
        if (logger.isDebugEnabled()) logger.debug("className: " + className
                + " storeIndex: " + storeIndex.getName()
                + " unique: " + Boolean.toString(unique)
                + " fields: " + toString(fields));
        this.className = className;
        this.storeIndex = storeIndex;
        this.indexName = storeIndex.getName();
        this.unique = unique;
        this.candidateColumns = new CandidateColumnImpl[fields.length];
        if (fields.length == 1) {
            // for a single field with multiple columns, score the number of columns
            this.fieldScore = fields[0].getColumnNames().length;
        }
        int i = 0;
        for (AbstractDomainFieldHandlerImpl domainFieldHandler: fields) {
            CandidateColumnImpl candidateColumn = new CandidateColumnImpl(domainFieldHandler);
            candidateColumns[i++] = candidateColumn;
        }
        if (logger.isDebugEnabled()) logger.debug(toString());
    }

    private String toString(AbstractDomainFieldHandlerImpl[] fields) {
        StringBuilder builder = new StringBuilder();
        char separator = '[';
        for (AbstractDomainFieldHandlerImpl field: fields) {
            builder.append(separator);
            builder.append(field.getName());
            separator = ' ';
        }
        builder.append(']');
        return builder.toString();
    }

    /** The CandidateIndexImpl used in cases of no where clause. */
    static CandidateIndexImpl indexForNullWhereClause = new CandidateIndexImpl();

    /** The accessor for the no where clause candidate index. */
    public static CandidateIndexImpl getIndexForNullWhereClause() {
        return indexForNullWhereClause;
    }

    /** The CandidateIndexImpl used in cases of no where clause. */
    protected CandidateIndexImpl() {
        // candidateColumns will be null if no usable columns in the index
    }

    @Override
    public String toString() {
        StringBuilder buffer = new StringBuilder();
        buffer.append("CandidateIndexImpl for class: ");
        buffer.append(className);
        buffer.append(" index: ");
        buffer.append(indexName);
        buffer.append(" unique: ");
        buffer.append(unique);
        if (candidateColumns != null) {
            for (CandidateColumnImpl column:candidateColumns) {
                buffer.append(" field: ");
                buffer.append(column.domainFieldHandler.getName());
            }
        } else {
            buffer.append(" no fields.");
        }
        return buffer.toString();
    }

    public void markLowerBound(int fieldNumber, PredicateImpl predicate, boolean strict) {
        if (candidateColumns != null) {
            candidateColumns[fieldNumber].markLowerBound(predicate, strict);
        }
    }

    public void markUpperBound(int fieldNumber, PredicateImpl predicate, boolean strict) {
        if (candidateColumns != null) {
            candidateColumns[fieldNumber].markUpperBound(predicate, strict);
        }
    }

    public void markEqualBound(int fieldNumber, PredicateImpl predicate) {
        if (candidateColumns != null) {
            candidateColumns[fieldNumber].markEqualBound(predicate);
        }
    }

    public void markInBound(int fieldNumber, InPredicateImpl predicate) {
        if (candidateColumns != null) {
            candidateColumns[fieldNumber].markInBound(predicate);
        }
    }

    String getIndexName() {
        return indexName;
    }

    CandidateColumnImpl lastLowerBoundColumn = null;
    CandidateColumnImpl lastUpperBoundColumn = null;

    /** Evaluate the suitability of this index for the query. An instance represents each
     * index, with primary key and non-hash-key indexes having two instances, one for the
     * unique index and one for the btree index for the same column(s).
     * Unique indexes where all of the columns are compared equal return a score of 100.
     * Btree indexes get one point for each query term that can be used (maximum of two
     * points for each comparison). Greater than and less than get one point each.
     * Equals and In get two points each. Once a gap is found in query terms for either
     * upper or lower bound, processing for that bound stops.
     * The last query term (candidate column) for each of the lower and upper bound is noted.
     * The method is synchronized because the method modifies the state of the instance,
     * which might be shared by multiple threads.
     */
    synchronized void score() {
        score = 0;
        if (candidateColumns == null) {
            return;
        }
        boolean lowerBoundDone = false;
        boolean upperBoundDone = false;
        if (unique) {
            // all columns need to have equal bound
            for (CandidateColumnImpl column: candidateColumns) {
                if (!(column.equalBound)) {
                    // not equal bound; can't use unique index
                    return;
                }
            }
            if ("PRIMARY".equals(indexName)) {
                scanType = PredicateImpl.ScanType.PRIMARY_KEY;
            } else {
                scanType = PredicateImpl.ScanType.UNIQUE_KEY;
            }
            score = 100;
            return;
        } else {
            // range index
            // leading columns need any kind of bound
            // extra credit for equals
            boolean firstColumn = true;
            for (CandidateColumnImpl candidateColumn: candidateColumns) {
                if ((candidateColumn.equalBound)) {
                    scanType = PredicateImpl.ScanType.INDEX_SCAN;
                    if (!lowerBoundDone) {
                        score += fieldScore;
                        lastLowerBoundColumn = candidateColumn;
                    }
                    if (!upperBoundDone) {
                        score += fieldScore;
                        lastUpperBoundColumn = candidateColumn;
                    }
                } else if ((candidateColumn.inBound)) {
                    scanType = PredicateImpl.ScanType.INDEX_SCAN;
                    if (firstColumn) {
                        multiRange = true;
                    }
                    if (!lowerBoundDone) {
                        score += fieldScore;
                        lastLowerBoundColumn = candidateColumn;
                    }
                    if (!upperBoundDone) {
                        score += fieldScore;
                        lastUpperBoundColumn = candidateColumn;
                    }
                } else if (!(lowerBoundDone && upperBoundDone)) {
                    // lower bound and upper bound are independent
                    boolean hasLowerBound = candidateColumn.hasLowerBound();
                    boolean hasUpperBound = candidateColumn.hasUpperBound();
                    // keep going until both upper and lower are done
                    if (hasLowerBound || hasUpperBound) {
                        scanType = PredicateImpl.ScanType.INDEX_SCAN;
                    }
                    if (!lowerBoundDone) {
                        if (hasLowerBound) {
                            score += fieldScore;
                            lastLowerBoundColumn = candidateColumn;
                        } else {
                            lowerBoundDone = true;
                        }
                    }
                    if (!upperBoundDone) {
                        if (hasUpperBound) {
                            score += fieldScore;
                            lastUpperBoundColumn = candidateColumn;
                        } else {
                            upperBoundDone = true;
                        }
                    } 
                    if (lowerBoundDone && upperBoundDone) {
                        continue;
                    }
                }
                firstColumn = false;
            }
            if (lastLowerBoundColumn != null) {
                lastLowerBoundColumn.markLastLowerBoundColumn();
            }
            if (lastUpperBoundColumn != null) {
                lastUpperBoundColumn.markLastUpperBoundColumn();
            }
        }
        return;
    }

    public ScanType getScanType() {
        return scanType;
    }

    /* No bound is complete yet */
    private final int BOUND_STATUS_NO_BOUND_DONE = 0;
    /* The lower bound is complete */
    private final int BOUND_STATUS_LOWER_BOUND_DONE = 1;
    /* The upper bound is complete */
    private final int BOUND_STATUS_UPPER_BOUND_DONE = 2;
    /* Both bounds are complete */
    private final int BOUND_STATUS_BOTH_BOUNDS_DONE = 3;

    /** Set bounds for the operation defined for this index. This index was chosen as
     * the best index to use for the query.
     * Each query term (candidate column) is used to set a bound. The bound depends on
     * the type of query term, whether the term is the last term, and whether the
     * bound type (upper or lower) has already been completely specified.
     * Equal and In query terms can be used for an equal bound, a lower bound, or an upper
     * bound. Strict bounds that are not the last bound are converted to non-strict bounds.
     * In query terms are decomposed into multiple range bounds, one range for each
     * value in the query term.
     * @param context the query execution context, containing the parameter values
     * @param op the index scan operation
     */
    void operationSetBounds(QueryExecutionContext context, IndexScanOperation op) {
        if (multiRange) {
            // find how many query terms are inPredicates
            List<Integer> parameterSizes = new ArrayList<Integer>();
            for (CandidateColumnImpl candidateColumn:candidateColumns) {
                if (candidateColumn.hasInBound()) {
                    parameterSizes.add(candidateColumn.getParameterSize(context));
                }
            }
            if (parameterSizes.size() > 1) {
                throw new ClusterJUserException(local.message("ERR_Too_Many_In_For_Index", indexName));
            }
            // if only one column in the index, optimize
            if (candidateColumns.length == 1) {
                candidateColumns[0].operationSetAllBounds(context, op);
            } else {
                // set multiple bounds; one for each item in the parameter (context)
                for (int parameterIndex = 0; parameterIndex < parameterSizes.get(0); ++parameterIndex) {
                    int boundStatus = BOUND_STATUS_NO_BOUND_DONE;
                    for (CandidateColumnImpl candidateColumn:candidateColumns) {
                        if (logger.isDetailEnabled()) logger.detail(
                                "parameterIndex: " + parameterIndex 
                                + " boundStatus: " + boundStatus
                                + " candidateColumn: " + candidateColumn.domainFieldHandler.getName());
                        // execute the bounds operation if anything left to do
                        if (boundStatus != BOUND_STATUS_BOTH_BOUNDS_DONE) {
                            boundStatus = candidateColumn.operationSetBounds(context, op, parameterIndex, boundStatus);
                        }
                    }
                    // after all columns are done, mark the end of bounds
                    op.endBound(parameterIndex);
                }
            }
        } else {
            // not multi-range
            int boundStatus = BOUND_STATUS_NO_BOUND_DONE;
            for (CandidateColumnImpl candidateColumn:candidateColumns) {
                if (logger.isDetailEnabled()) logger.detail("boundStatus: " + boundStatus
                        + " candidateColumn: " + candidateColumn.domainFieldHandler.getName());
                // execute the bounds operation for each query term
                if (boundStatus != BOUND_STATUS_BOTH_BOUNDS_DONE) {
                    boundStatus = candidateColumn.operationSetBounds(context, op, -1, boundStatus);
                }
            }
        }
    }

    void operationSetKeys(QueryExecutionContext context,
            Operation op) {
        for (CandidateColumnImpl candidateColumn:candidateColumns) {
            // execute the equal operation
            candidateColumn.operationSetKeys(context, op);
        }
    }

    /** 
     * This class represents one column in an index, and its corresponding query term(s).
     * The column might be associated with a lower bound term, an upper bound term,
     * an equal term, or an in term. 
     */
    class CandidateColumnImpl {

        protected AbstractDomainFieldHandlerImpl domainFieldHandler;
        protected PredicateImpl predicate;
        protected PredicateImpl lowerBoundPredicate;
        protected PredicateImpl upperBoundPredicate;
        protected PredicateImpl equalPredicate;
        protected InPredicateImpl inPredicate;
        protected Boolean lowerBoundStrict = null;
        protected Boolean upperBoundStrict = null;
        protected boolean equalBound = false;
        protected boolean inBound = false;
        protected boolean lastLowerBoundColumn = false;
        protected boolean lastUpperBoundColumn = false;

        protected boolean hasLowerBound() {
            return lowerBoundPredicate != null || equalPredicate != null || inPredicate != null;
        }

        /** Set all bounds in the operation, ending each bound with an end_of_bound.
         * 
         * @param context the query context
         * @param op the operation
         */
        public void operationSetAllBounds(QueryExecutionContext context, IndexScanOperation op) {
            inPredicate.operationSetAllBounds(context, op);
        }

        public int getParameterSize(QueryExecutionContext context) {
            return inPredicate.getParameterSize(context);
        }

        protected boolean hasUpperBound() {
            return upperBoundPredicate != null || equalPredicate != null || inPredicate != null;
        }

        protected boolean hasInBound() {
            return inBound;
        }

        private CandidateColumnImpl(AbstractDomainFieldHandlerImpl domainFieldHandler) {
            this.domainFieldHandler = domainFieldHandler;
        }

        private void markLastLowerBoundColumn() {
            lastLowerBoundColumn = true;
        }

        private void markLastUpperBoundColumn() {
            lastUpperBoundColumn = true;
        }

        private void markLowerBound(PredicateImpl predicate, boolean strict) {
            lowerBoundStrict = strict;
            this.lowerBoundPredicate = predicate;
            this.predicate = predicate;
        }

        private void markUpperBound(PredicateImpl predicate, boolean strict) {
            upperBoundStrict = strict;
            this.upperBoundPredicate = predicate;
            this.predicate = predicate;
        }

        private void markEqualBound(PredicateImpl predicate) {
            equalBound = true;
            this.equalPredicate = predicate;
            this.predicate = predicate;
        }

        public void markInBound(InPredicateImpl predicate) {
            inBound = true;
            this.inPredicate = predicate;
            this.predicate = predicate;
        }

        /** Set bounds into each predicate that has been defined.
         *
         * @param op the operation
         * @param index for inPredicates, the index into the parameter
         * @param boundStatus 
         */
        private int operationSetBounds(
                QueryExecutionContext context, IndexScanOperation op, int index, int boundStatus) {
            if (inPredicate != null && index == -1
                    || !canBound) {
                // "in" predicate cannot be used to set bounds unless it is the first column in the index
                // if index scan but no valid bounds to set skip bounds
                return BOUND_STATUS_BOTH_BOUNDS_DONE;
            }

            int boundSet = PredicateImpl.NO_BOUND_SET;

            if (logger.isDetailEnabled()) logger.detail("column: " + domainFieldHandler.getName() 
                    + " boundStatus: " + boundStatus
                    + " lastLowerBoundColumn: " + lastLowerBoundColumn
                    + " lastUpperBoundColumn: " + lastUpperBoundColumn);
            switch(boundStatus) {
                case BOUND_STATUS_BOTH_BOUNDS_DONE:
                    // cannot set either lower or upper bound
                    return BOUND_STATUS_BOTH_BOUNDS_DONE;
                case BOUND_STATUS_NO_BOUND_DONE:
                    // can set either/both lower or upper bound
                    if (equalPredicate != null) {
                        boundSet |= equalPredicate.operationSetBounds(context, op, true);
                    }
                    if (inPredicate != null) {
                        boundSet |= inPredicate.operationSetBound(context, op, index, true);
                    }
                    if (lowerBoundPredicate != null) {
                        boundSet |= lowerBoundPredicate.operationSetLowerBound(context, op, lastLowerBoundColumn);
                    }
                    if (upperBoundPredicate != null) {
                        boundSet |= upperBoundPredicate.operationSetUpperBound(context, op, lastUpperBoundColumn);
                    }
                    break;
                case BOUND_STATUS_LOWER_BOUND_DONE:
                    // cannot set lower, only upper bound
                    if (equalPredicate != null) {
                        boundSet |= equalPredicate.operationSetUpperBound(context, op, lastUpperBoundColumn);
                    }
                    if (inPredicate != null) {
                        boundSet |= inPredicate.operationSetUpperBound(context, op, index);
                    }
                    if (upperBoundPredicate != null) {
                        boundSet |= upperBoundPredicate.operationSetUpperBound(context, op, lastUpperBoundColumn);
                    }
                    break;
                case BOUND_STATUS_UPPER_BOUND_DONE:
                    // cannot set upper, only lower bound
                    if (equalPredicate != null) {
                        boundSet |= equalPredicate.operationSetLowerBound(context, op, lastLowerBoundColumn);
                    }
                    if (inPredicate != null) {
                        boundSet |= inPredicate.operationSetLowerBound(context, op, index);
                    }
                    if (lowerBoundPredicate != null) {
                        boundSet |= lowerBoundPredicate.operationSetLowerBound(context, op, lastLowerBoundColumn);
                    }
                    break;
            }
            if (0 == (boundSet & PredicateImpl.LOWER_BOUND_SET)) {
                // didn't set lower bound
                boundStatus |= BOUND_STATUS_LOWER_BOUND_DONE;
            }
                
            if (0 == (boundSet & PredicateImpl.UPPER_BOUND_SET)) {
                // didn't set upper bound
                boundStatus |= BOUND_STATUS_UPPER_BOUND_DONE;
            }
                
            return boundStatus;
        }

        private void operationSetKeys(QueryExecutionContext context, Operation op) {
            equalPredicate.operationEqual(context, op);
        }

    }

    /** Determine whether this index supports exactly the number of conditions.
     * For ordered indexes, any number of conditions are supported via filters.
     * For hash indexes, only the number of columns in the index are supported.
     * @param numberOfConditions the number of conditions in the query predicate
     * @return if this index supports exactly the number of conditions
     */
    public boolean supportsConditionsOfLength(int numberOfConditions) {
        if (unique) {
            return numberOfConditions == candidateColumns.length;
        } else {
            return true;
        }
    }

    public Index getStoreIndex() {
        return storeIndex;
    }

    public int getScore() {
        return score;
    }

    public boolean isMultiRange() {
        return multiRange;
    }

    public boolean isUnique() {
        return unique;
    }

    /** Is this index usable in the current context?
     * If a primary or unique index, all parameters must be non-null.
     * If a btree index, the parameter for the first comparison must be non-null.
     * If ordering is specified, the ordering fields must appear in the proper position in the index.
     * <ul><li>Returns -1 if this index is unusable.
     * </li><li>Returns 0 if this index is usable but has no filtering terms
     * </li><li>Returns 1 if this index is usable and has at least one usable filtering term
     * </li></ul>
     * @param context the query execution context
     * @param orderingFields the fields in the ordering
     * @return the usability of this index
     */
    public int isUsable(QueryExecutionContext context, String[] orderingFields) {
        boolean ordering = orderingFields != null;
        if (ordering && !containsAllOrderingFields(orderingFields)) {
            return -1;
        }
                
        // ordering is ok; unique indexes have to have no null parameters
        if (unique && score > 0) {
            return context.hasNoNullParameters()?1:-1;
        } else {
            // index scan; the first parameter must not be null
            if (candidateColumns == null) {
                // this is a dummy index for "no where clause"
                canBound = false;
            } else {
                CandidateColumnImpl candidateColumn = candidateColumns[0];
                PredicateImpl predicate = candidateColumn.predicate;
                canBound = predicate != null && predicate.isUsable(context);
            }
            // if first parameter is null, can scan but not bound
            if (canBound) {
                if (logger.isDebugEnabled()) logger.debug("for " + indexName + " canBound true -> returns 1");
                scanType = PredicateImpl.ScanType.INDEX_SCAN;
                return 1;
            } else {
                if (ordering) {
                    if (logger.isDebugEnabled()) logger.debug("for " + indexName + " canBound false -> returns 0");
                    scanType = PredicateImpl.ScanType.INDEX_SCAN;
                    return 0;
                } else {
                    if (logger.isDebugEnabled()) logger.debug("for " + indexName + " canBound false -> returns -1");
                    return -1;
                }
            }
        }
    }

    /** Does this index contain all ordering fields?
     * 
     * @param orderingFields the ordering fields
     * @return true if this ordered index contains all ordering fields in the proper position with no gaps
     */
    public boolean containsAllOrderingFields(String[] orderingFields) {
        if (isUnique()) {
            return false;
        }
        int candidateColumnIndex = 0;
        if (orderingFields != null) {
            for (String orderingField: orderingFields) {
                if (candidateColumnIndex >= candidateColumns.length) {
                    // too many columns in orderingFields for this index
                    if (logger.isDebugEnabled()) logger.debug("Index " + indexName + " cannot be used because "
                            + orderingField + " is not part of this index.");
                    return false;
                }
                // each ordering field must correspond in order to the index fields
                CandidateColumnImpl candidateColumn = candidateColumns[candidateColumnIndex++];
                if (!orderingField.equals(candidateColumn.domainFieldHandler.getName())) {
                    // the ordering field is not in the proper position in this candidate index
                    if (logger.isDebugEnabled()) {
                        logger.debug("Index " + indexName + " cannot be used because CandidateColumn "
                            + candidateColumn.domainFieldHandler.getName() + " does not match " + orderingField);
                    }
                    return false;
                }
            }
            if (logger.isDebugEnabled()) {
                logger.debug("CandidateIndexImpl.containsAllOrderingFields found possible index (unique: "
                        + unique + ") " + indexName);
            }
            scanType = PredicateImpl.ScanType.INDEX_SCAN;
            return true;
        }
        return false;
    }

}
