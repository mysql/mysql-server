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

import com.mysql.clusterj.core.metadata.AbstractDomainFieldHandlerImpl;

import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 * This class is responsible for deciding whether an index can be used
 * for a specific query. An instance of this class is created to evaluate
 * a query and to decide whether the index can be used in executing the
 * query. An inner class represents each candidateColumn in the index.
 */
public class CandidateIndexImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(CandidateIndexImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(CandidateIndexImpl.class);

    private String className = "No class for null where clause";
    private Index storeIndex;
    private String indexName = "No index for null where clause";
    private boolean unique;
    private boolean hash;
    private CandidateColumnImpl[] candidateColumns;
    private int scanType = PredicateImpl.TABLE_SCAN;
    private int fieldScore = 1;

    public CandidateIndexImpl(
            String className, Index storeIndex, boolean unique, boolean hash,
            AbstractDomainFieldHandlerImpl[] fields) {
        this.className = className;
        this.storeIndex = storeIndex;
        this.indexName = storeIndex.getName();
        this.unique = unique;
        this.hash = hash;
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

    /** The CandidateIndexImpl used in cases of no where clause. */
    static CandidateIndexImpl indexForNullWhereClause = new CandidateIndexImpl();

    /** The accessor for the no where clause candidate index. */
    static CandidateIndexImpl getIndexForNullWhereClause() {
        return indexForNullWhereClause;
    }

    /** The CandidateIndexImpl used in cases of no where clause. */
    protected CandidateIndexImpl() {
    }

    @Override
    public String toString() {
        StringBuffer buffer = new StringBuffer();
        buffer.append("CandidateIndexImpl for class: ");
        buffer.append(className);
        buffer.append(" index: ");
        buffer.append(indexName);
        buffer.append(" unique: ");
        buffer.append(unique);
        buffer.append(" hash: ");
        buffer.append(hash);
        for (CandidateColumnImpl column:candidateColumns) {
            buffer.append(" field: ");
            buffer.append(column.domainFieldHandler.getName());
        }
        return buffer.toString();
    }

    public void markLowerBound(int fieldNumber, PredicateImpl predicate, boolean strict) {
        candidateColumns[fieldNumber].markLowerBound(predicate, strict);
    }

    public void markUpperBound(int fieldNumber, PredicateImpl predicate, boolean strict) {
        candidateColumns[fieldNumber].markUpperBound(predicate, strict);
    }

    public void markEqualBound(int fieldNumber, PredicateImpl predicate) {
        candidateColumns[fieldNumber].markEqualBound(predicate);
    }

    String getIndexName() {
        return indexName;
    }

    CandidateColumnImpl lastLowerBoundColumn = null;
    CandidateColumnImpl lastUpperBoundColumn = null;

    int getScore() {
        int result = 0;
        boolean lowerBoundDone = false;
        boolean upperBoundDone = false;
        if (hash) {
            // all columns need to have equal bound
            for (CandidateColumnImpl column: candidateColumns) {
                if (!(column.equalBound)) {
                    // not equal bound; can't use unique index
                    return result;
                }
            }
            if ("PRIMARY".equals(indexName)) {
                scanType = PredicateImpl.PRIMARY_KEY;
            } else {
                scanType = PredicateImpl.UNIQUE_SCAN;
            }
            return 100;
        } else {
            // range index
            // leading columns need any kind of bound
            // extra credit for equals
            for (CandidateColumnImpl candidateColumn: candidateColumns) {
                if ((candidateColumn.equalBound)) {
                    scanType = PredicateImpl.INDEX_SCAN;
                    if (!lowerBoundDone) {
                        result += fieldScore;
                        lastLowerBoundColumn = candidateColumn;
                    }
                    if (!upperBoundDone) {
                        result += fieldScore;
                        lastUpperBoundColumn = candidateColumn;
                    }
                } else if (!(lowerBoundDone && upperBoundDone)) {
                    // lower bound and upper bound are independent
                    // boolean hasLowerBound = candidateColumn.lowerBoundStrict != null && candidateColumn.lowerBoundStrict;
                    // boolean hasUpperBound = candidateColumn.upperBoundStrict != null && candidateColumn.upperBoundStrict;
                    boolean hasLowerBound = candidateColumn.hasLowerBound();
                    boolean hasUpperBound = candidateColumn.hasUpperBound();
                    // keep going until both upper and lower are done
                    if (hasLowerBound || hasUpperBound) {
                        scanType = PredicateImpl.INDEX_SCAN;
                    }
                    if (!lowerBoundDone) {
                        if (hasLowerBound) {
                            result += fieldScore;
                            lastLowerBoundColumn = candidateColumn;
                        } else {
                            lowerBoundDone = true;
                        }
                    }
                    if (!upperBoundDone) {
                        if (hasUpperBound) {
                            result += fieldScore;
                            lastUpperBoundColumn = candidateColumn;
                        } else {
                            upperBoundDone = true;
                        }
                    } else {
                        // no more bounds; return what we have
                        if (lastLowerBoundColumn != null) {
                            lastLowerBoundColumn.markLastColumn();
                        }
                        if (lastUpperBoundColumn != null) {
                            lastUpperBoundColumn.markLastColumn();
                        }
                        return result;
                    }
                }
            }
        }
        return result;
    }

    public int getScanType() {
        return scanType;
    }

    void operationSetBounds(QueryExecutionContextImpl context,
            IndexScanOperation op) {
        for (CandidateColumnImpl candidateColumn:candidateColumns) {
            // execute the bounds operation
            candidateColumn.operationSetBounds(context, op);
        }
    }

    void operationSetKeys(QueryExecutionContextImpl context,
            Operation op) {
        for (CandidateColumnImpl candidateColumn:candidateColumns) {
            // execute the equal operation
            candidateColumn.operationSetKeys(context, op);
        }
    }

    class CandidateColumnImpl {

        protected AbstractDomainFieldHandlerImpl domainFieldHandler;
        protected PredicateImpl lowerBoundPredicate;
        protected PredicateImpl upperBoundPredicate;
        protected PredicateImpl equalPredicate;
        protected Boolean lowerBoundStrict = null;
        protected Boolean upperBoundStrict = null;
        protected boolean equalBound = false;
        protected boolean lastColumn = false;

        protected boolean hasLowerBound() {
            return lowerBoundPredicate != null;
        }

        protected boolean hasUpperBound() {
            return upperBoundPredicate != null;
        }

        private CandidateColumnImpl(AbstractDomainFieldHandlerImpl domainFieldHandler) {
            this.domainFieldHandler = domainFieldHandler;
        }

        private void markLastColumn() {
            lastColumn = true;
        }

        private void markLowerBound(PredicateImpl predicate, boolean strict) {
            lowerBoundStrict = strict;
            this.lowerBoundPredicate = predicate;
        }

        private void markUpperBound(PredicateImpl predicate, boolean strict) {
            upperBoundStrict = strict;
            this.upperBoundPredicate = predicate;
        }

        private void markEqualBound(PredicateImpl predicate) {
            equalBound = true;
            this.equalPredicate = predicate;
        }

        /** Set bounds into each predicate that has been defined.
         *
         * @param op
         */
        private void operationSetBounds(QueryExecutionContextImpl context,
                IndexScanOperation op) {
            if (lowerBoundPredicate != null) {
                lowerBoundPredicate.operationSetLowerBound(context, op);
            }
            if (upperBoundPredicate != null) {
                upperBoundPredicate.operationSetUpperBound(context, op);
            }
            if (equalPredicate != null) {
                equalPredicate.operationSetBounds(context, op);
            }
        }

        private void operationSetKeys(QueryExecutionContextImpl context,
                Operation op) {
            equalPredicate.operationEqual(context, op);
        }

        private void setBound(QueryExecutionContextImpl context,
                PredicateImpl predicate, IndexScanOperation op) {
            if (predicate != null) {
                predicate.operationSetBounds(context, op);
            }
            // set satisfied if bound is completely satisfied
        }

        private void setSatisfied() {
            throw new UnsupportedOperationException("Not yet implemented");
        }
    }

    public Index getStoreIndex() {
        return storeIndex;
    }

}
