/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;

/** This represents a named parameter that is bound at execution time
 * to a value.
 * 
 */
public class ParameterImpl implements PredicateOperand {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ParameterImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(ParameterImpl.class);

    /** My domain object. */
    protected QueryDomainTypeImpl<?> dobj;

    /** My property (set when bound) */
    protected PropertyImpl property;

    /** My parameter name */
    protected String parameterName;

    /** Is a value bound to this parameter? */
    protected boolean bound = false;

    /** Is this parameter marked (used in the query)? */
    protected boolean marked = false;

    public ParameterImpl(QueryDomainTypeImpl<?> dobj, String parameterName) {
        this.dobj = dobj;
        this.parameterName = parameterName;
    }

    public void mark() {
        marked = true;
    }

    boolean isMarkedAndUnbound(QueryExecutionContext context) {
        return marked && !context.isBound(parameterName);
    }

    void unmark() {
        marked = false;
    }

    public String getName() {
        return parameterName;
    }

    public Object getParameterValue(QueryExecutionContext context) {
        return property.getParameterValue(context, parameterName);
    }

    public Predicate equal(PredicateOperand predicateOperand) {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
    }

    public Predicate between(PredicateOperand lower, PredicateOperand upper) {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
    }

    public Predicate greaterThan(PredicateOperand other) {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
    }

    public Predicate greaterEqual(PredicateOperand other) {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
    }

    public Predicate lessThan(PredicateOperand other) {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
    }

    public Predicate lessEqual(PredicateOperand other) {
        throw new UnsupportedOperationException(
                local.message("ERR_NotImplemented"));
    }

    public Predicate in(PredicateOperand other) {
        throw new UnsupportedOperationException(
                local.message("ERR_NotImplemented"));
    }

    public Predicate like(PredicateOperand other) {
        throw new UnsupportedOperationException(
                local.message("ERR_NotImplemented"));
    }

    public Predicate isNull() {
        throw new UnsupportedOperationException(
                local.message("ERR_NotImplemented"));
    }

    public Predicate isNotNull() {
        throw new UnsupportedOperationException(
                local.message("ERR_NotImplemented"));
    }

    public void setProperty(PropertyImpl property) {
        if (this.property != null && this.property.fmd.getType() != property.fmd.getType()) {
            throw new ClusterJUserException(local.message("ERR_Multiple_Parameter_Usage", parameterName,
                    this.property.fmd.getType().getName(), property.fmd.getType().getName()));
        } else {
            this.property = property;
        }
    }

}
