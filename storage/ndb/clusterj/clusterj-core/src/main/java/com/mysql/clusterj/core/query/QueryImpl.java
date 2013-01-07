/*
   Copyright (c) 2009, 2012, Oracle and/or its affiliates. All rights reserved.

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
import com.mysql.clusterj.Results;
import com.mysql.clusterj.core.*;
import com.mysql.clusterj.Query;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.util.List;
import java.util.Map;

public class QueryImpl<E> implements Query<E> {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(BetweenPredicateImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(BetweenPredicateImpl.class);

    /** My session. */
    protected SessionImpl session;

    /** My DomainObject */
    protected QueryDomainTypeImpl<E> dobj;

    /** My query execution context. */
    protected QueryExecutionContextImpl context = null;

    /** The number to skip */
    protected long skip = 0;

    /** The limit */
    protected long limit = Long.MAX_VALUE;

    /** The order */
    protected Query.Ordering ordering = null;

    /** The ordering fields */
    protected String[] orderingFields = null;

    public QueryImpl(SessionImpl session, QueryDomainTypeImpl<E> dobj) {
        this.session = session;
        context = new QueryExecutionContextImpl(session);
        this.dobj = dobj;
    }

    /**
     * Set limits on results to return. The execution of the query is
     * modified to return only a subset of results. If the filter would
     * normally return 100 instances, skip is set to 50, and
     * limit is set to 40, then the first 50 results that would have 
     * been returned are skipped, the next 40 results are returned and the
     * remaining 10 results are ignored.
     * <p>
     * Skip must be greater than or equal to 0. Limit must be greater than or equal to 0.
     * Limits may not be used with deletePersistentAll.
     * <p>
     * The limits as specified by the user are converted here into an internal form
     * where the limit is the last record to deliver instead of the number of records
     * to deliver. So if the user specifies limits of (10, 20) we convert this 
     * to limits of (10, 30) for the lower layers of the implementation.
     * @param skip the number of results to skip
     * @param limit the number of results to return after skipping;
     * use Long.MAX_VALUE for no limit.
     */
    public void setLimits(long skip, long limit) {
        if (skip < 0 || limit < 0) {
            throw new ClusterJUserException(local.message("ERR_Invalid_Limits", skip, limit));
        }
        this.skip = skip;
        if (Long.MAX_VALUE - skip < limit) {
            limit = Long.MAX_VALUE;
        } else {
            this.limit = limit + skip;
        }
    }

    /** Set ordering for this query. Verify that the ordering fields exist in the domain type.
     * @param ordering the ordering for the query
     * @param orderingFields the list of fields to order by
     */
    public void setOrdering(com.mysql.clusterj.Query.Ordering ordering,
            String... orderingFields) {
        this.ordering = ordering;
        this.orderingFields = orderingFields;
        // verify that all ordering fields actually are fields
        StringBuilder builder = new StringBuilder();
        String separator = "";
        for (String orderingField : orderingFields) {
            try {
                dobj.get(orderingField);
            } catch (ClusterJUserException ex) {
                builder.append(separator);
                builder.append(orderingField);
                separator = ", ";
            }
        }
        String errors = builder.toString();
        if (errors.length() > 0) {
            throw new ClusterJUserException(local.message("ERR_Ordering_Field_Does_Not_Exist", errors));
        }
    }

    public Results<E> execute(Object arg0) {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
    }

    public Results<E> execute(Object... arg0) {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
    }

    public Results<E> execute(Map<String, ?> arg0) {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
    }

    public void setParameter(String parameterName, Object parameterValue) {
        context.bindParameterValue(parameterName, parameterValue);
    }

    public List<E> getResultList() {
        List<E> results = dobj.getResultList(context, skip, limit, ordering, orderingFields);
        // create new context, copying the parameters, for another execution
        context = new QueryExecutionContextImpl(context);
        return results;
    }

    /** Delete the instances that satisfy the query criteria.
     * @return the number of instances deleted
     */
    public int deletePersistentAll() {
        if (skip != 0 || limit != Long.MAX_VALUE) {
            throw new ClusterJUserException(local.message("ERR_Invalid_Limits", skip, limit));
        }
        int result = dobj.deletePersistentAll(context);
        return result;
    }

    /**
     * Explain this query.
     * @return the data about the execution of this query
     */
    public Map<String, Object> explain() {
        Map<String, Object> result = context.getExplain();
        if (result == null) {
            dobj.explain(context);
            return context.getExplain();
        }
        return result;
    }

}
