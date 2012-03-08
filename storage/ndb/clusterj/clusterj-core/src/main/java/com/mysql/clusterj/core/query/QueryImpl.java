/*
   Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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

    public QueryImpl(SessionImpl session, QueryDomainTypeImpl<E> dobj) {
        this.session = session;
        context = new QueryExecutionContextImpl(session);
        this.dobj = dobj;
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
        List<E> results = dobj.getResultList(context);
        // create new context, copying the parameters, for another execution
        context = new QueryExecutionContextImpl(context);
        return results;
    }

    /** Delete the instances that satisfy the query criteria.
     * @return the number of instances deleted
     */
    public int deletePersistentAll() {
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
