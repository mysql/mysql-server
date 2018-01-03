/*
   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.ClusterJUserException;

import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
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

    /** Is this property used with a complex parameter? */
    private boolean complexParameter = false;

    public PropertyImpl(QueryDomainTypeImpl<?> dobj, DomainFieldHandler fmd) {
        this.dobj = dobj;
        this.fmd = fmd;
    }

    @Override
    public String toString() {
        return fmd.getName();
    }

    public void setComplexParameter() {
        complexParameter = true;
    }

    public void operationSetBounds(Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
        fmd.operationSetBounds(value, type, op);
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
        return new EqualPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate between(PredicateOperand lower, PredicateOperand upper) {
        if (!((lower instanceof ParameterImpl) && (upper instanceof ParameterImpl))) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "between"));
        }
        return new BetweenPredicateImpl(dobj, this, (ParameterImpl)lower, (ParameterImpl)upper);
    }

    public Predicate greaterThan(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "greaterThan"));
        }
        return new GreaterThanPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate greaterEqual(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "greaterEqual"));
        }
        return new GreaterEqualPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate lessThan(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "lessThan"));
        }
        return new LessThanPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate lessEqual(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "lessEqual"));
        }
        return new LessEqualPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate in(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "in"));
        }
        return new InPredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate like(PredicateOperand other) {
        if (!(other instanceof ParameterImpl)) {
            throw new ClusterJUserException(
                    local.message("ERR_Only_Parameters", "like"));
        }
        return new LikePredicateImpl(dobj, this, (ParameterImpl)other);
    }

    public Predicate isNull() {
        return new IsNullPredicateImpl(dobj, this);
    }

    public Predicate isNotNull() {
        return new IsNotNullPredicateImpl(dobj, this);
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

    public Object getParameterValue(QueryExecutionContext context, String parameterName) {
        if (complexParameter) {
            // the parameter is just an object at this point -- to be checked elsewhere
            return context.getParameterValue(parameterName);
        } else {
            return fmd.getValue(context, parameterName);
        }
    }

    public void filterIsNull(ScanFilter filter) {
        fmd.filterIsNull(filter);
    }

    public void filterIsNotNull(ScanFilter filter) {
        fmd.filterIsNotNull(filter);
    }

}
