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

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;

import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ScanFilter;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;

public class PropertyImpl implements PredicateOperand {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(PropertyImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(PropertyImpl.class);

    /** My domain object. */
    protected QueryDomainTypeImpl<?> dobj;

    /** My property */
    protected DomainFieldHandler fmd;

    public PropertyImpl(QueryDomainTypeImpl<?> dobj, DomainFieldHandler fmd) {
        this.dobj = dobj;
        this.fmd = fmd;
    }

    public void operationSetBounds(Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
        fmd.operationSetBounds(value, type, op);
    }

    public void operationSetLikeBounds(Object value, IndexScanOperation op, boolean lastColumn) {
        // not currently implemented
//        fmd.operationSetBounds(value, type, op);
        throw new ClusterJFatalInternalException(local.message("ERR_NotImplemented"));
    }

    public void operationEqual(Object value, Operation op) {
        fmd.operationEqual(value, op);
    }

    void objectSetValuesFor(Object value, Object row, String indexName) {
        fmd.objectSetValueFor(value, row, indexName);
    }

    void operationEqualFor(Object parameterValue, Operation op, String indexName) {
        fmd.operationEqualForIndex(parameterValue, op, indexName);
    }

    public void filterCmpValue(Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
        fmd.filterCompareValue(value, condition, filter);
    }
 
    public Predicate equal(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "equal"));
        }
        return (Predicate) new EqualPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate between(PredicateOperand lower, PredicateOperand upper) {
        if (!((lower instanceof ParameterImpl) && (upper instanceof ParameterImpl))) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "between"));
        }
        return (Predicate) new BetweenPredicateImpl(dobj, this, (ParameterImpl)lower, (ParameterImpl)upper);
    }

    public Predicate greaterThan(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "greaterThan"));
        }
        return (Predicate) new GreaterThanPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate greaterEqual(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "greaterEqual"));
        }
        return (Predicate) new GreaterEqualPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate lessThan(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "lessThan"));
        }
        return (Predicate) new LessThanPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate lessEqual(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "lessEqual"));
        }
        return (Predicate) new LessEqualPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate in(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "in"));
        }
        return (Predicate) new InPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate like(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "like"));
        }
        return (Predicate) new LikePredicateImpl(dobj, this, (ParameterImpl)other);
    }

    void markLowerBound(CandidateIndexImpl[] candidateIndices, PredicateImpl predicate, boolean strict) {
        fmd.markLowerBounds(candidateIndices, predicate, strict);
    }

    void markUpperBound(CandidateIndexImpl[] candidateIndices, PredicateImpl predicate, boolean strict) {
        fmd.markUpperBounds(candidateIndices, predicate, strict);
    }

    void markEqualBound(CandidateIndexImpl[] candidateIndices, PredicateImpl predicate) {
        fmd.markEqualBounds(candidateIndices, predicate);
    }

    public void markInBound(CandidateIndexImpl[] candidateIndices, InPredicateImpl predicate) {
        fmd.markInBounds(candidateIndices, predicate);
    }

    public void markLikeBound(CandidateIndexImpl[] candidateIndices, Object value, LikePredicateImpl predicate) {
        // not currently implemented
//        if (isSuitableForLikeIndex(value)) {
//            fmd.markLikeBounds(candidateIndices, predicate);
//        }
    }

    /** Determine whether the value is suitable for use as bounds for an index scan.
     * Used to create bounds for "like" queries. 
     * This method should just delegate to the DomainFieldHandler...
     * @param value the value
     * @return true if the value can be used to create lower and upper bounds
     */
    @SuppressWarnings("unused")
    private boolean isSuitableForLikeIndex(Object value) {
        boolean result = false;
        if (value instanceof String) {
            String stringValue = (String)value;
            int firstPercent = stringValue.indexOf('%');
            int firstUnderbar = stringValue.indexOf('_');
            if (firstPercent != 0 && firstUnderbar != 0) {
                 // neither % or _ is the first character so index can be used
                result = true;
            }
        }
        if (logger.isDebugEnabled()) logger.debug(
                "" + value + (result?" is suitable for index.":" is not suitable for index."));
        return result;
    }

    /** Create a lower bound from a "like" parameter.
     * This method should just delegate to the DomainFieldHandler...
     * @param parameterValue the value whence to create a lower bound
     * @return the lower bound
     */
    @SuppressWarnings("unused")
    private String getLowerBound(String parameterValue) {
        String result = parameterValue;
        int firstPercent = parameterValue.indexOf('%');
        int firstUnderbar = parameterValue.indexOf('_');
        int firstWildCard = 0;
        if (firstPercent == -1) {
            firstWildCard = firstUnderbar;
        } else if (firstUnderbar == -1) {
            firstWildCard = firstPercent;
        } else {
            firstWildCard = Math.min(firstPercent, firstUnderbar);
        }
        if (firstWildCard != -1) {
            // either % or _ appears in the string
            result = parameterValue.substring(0, firstWildCard);
        }
        if (logger.isDebugEnabled()) logger.debug("lower bound for " + parameterValue + " is " + result);
        return result;
    }

    /** Create an upper bound from a "like" parameter.
     * This method should just delegate to the DomainFieldHandler...
     * @param parameterValue the "like" parameter
     * @return the upper bound
     */
    @SuppressWarnings("unused")
    private String getUpperBound(String parameterValue) {
        int firstPercent = parameterValue.indexOf('%');
        int firstUnderbar = parameterValue.indexOf('_');
        int firstWildCard = 0;
        if (firstPercent == -1) {
            firstWildCard = firstUnderbar;
        } else if (firstUnderbar == -1) {
            firstWildCard = firstPercent;
        } else {
            firstWildCard = Math.min(firstPercent, firstUnderbar);
        }
        if (firstWildCard == -1) {
            // neither % nor _ appears in the string
            firstWildCard = parameterValue.length();
        }
        StringBuilder builder = new StringBuilder(parameterValue.subSequence(0, firstWildCard - 1));
        builder.append((char)(parameterValue.charAt(firstWildCard - 1) + 1));
        String result = builder.toString();
        if (logger.isDebugEnabled()) logger.debug("upper bound for " + parameterValue + " is " + result);
        return result;
    }

}
