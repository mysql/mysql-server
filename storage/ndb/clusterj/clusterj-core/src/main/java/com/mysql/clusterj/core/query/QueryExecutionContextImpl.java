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

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.clusterj.query.QueryDomainType;

import java.util.HashMap;
import java.util.Map;

/** This is the execution context for a query. It contains the
 * parameter bindings so as to make query execution thread-safe.
 *
 */
public class QueryExecutionContextImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(BetweenPredicateImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(BetweenPredicateImpl.class);

    protected Map<String, Object> boundParameters = 
            new HashMap<String, Object>();

    /** The session for this query */
    protected SessionSPI session;

    /** Create a new execution context with an empty map of parameters.
     * @param session the session for this context
     */
    public QueryExecutionContextImpl(SessionSPI session) {
        if (session == null) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Session_Must_Not_Be_Null"));
        }
        this.session = session;
    }

    /** Create a new execution context copying the bound parameter values.
     * This allows a new execution of a query only modifying some parameters.
     * @param context an existing execution context
     */
    protected QueryExecutionContextImpl(QueryExecutionContextImpl context) {
        this.session = context.getSession();
        boundParameters = new HashMap<String, Object>(context.boundParameters);
    }

    /** Create a new execution context with specific map of parameters.
     * @param session the session for this context
     * @param parameterMap the parameter map for this context
     */
    public QueryExecutionContextImpl(SessionSPI session, Map<String, Object> parameterMap) {
	this.session = session;
	this.boundParameters = parameterMap;
    }

    /** Bind the value of a parameter for this query execution.
     *
     * @param parameterName the name of the parameter
     * @param value the value for the parameter
     */
    public void bindParameterValue(String parameterName, Object value) {
        if (parameterName == null) {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Null"));
        }
        boundParameters.put(parameterName, value);
    }
    /** Get the value of a parameter by name.
     */
    public Object getParameterValue(String parameterName) {
        if (!isBound(parameterName)) {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Not_Bound", parameterName));
        }
        return boundParameters.get(parameterName);
    }

    /** Return whether the parameter has a value for this execution context.
     *
     * @param parameterName the name of the parameter
     * @return whether the parameter has a value
     */
    boolean isBound(String parameterName) {
        return boundParameters.containsKey(parameterName);
    }

    public SessionSPI getSession() {
	return session;
    }

    public ResultData getResultData(QueryDomainType<?> queryDomainType) {
	return ((QueryDomainTypeImpl<?>)queryDomainType).getResultData(this);
    }
}
