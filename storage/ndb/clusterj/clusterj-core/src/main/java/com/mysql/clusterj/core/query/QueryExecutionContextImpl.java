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

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.clusterj.query.QueryDomainType;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** This is the execution context for a query. It contains the
 * parameter bindings so as to make query execution thread-safe.
 *
 */
public class QueryExecutionContextImpl implements QueryExecutionContext {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(BetweenPredicateImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(BetweenPredicateImpl.class);

    protected Map<String, Object> boundParameters = 
            new HashMap<String, Object>();

    /** The session for this query */
    protected SessionSPI session;

    /** The filters used in the query */
    private List<ScanFilter> filters = new ArrayList<ScanFilter>();

    /** The explain for this query; will be null until executed or explained */
    protected Map<String, Object> explain = null;

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
        this.explain = context.getExplain();
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
        // if any parameters changed, the explain is no longer valid
        this.explain = null;
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
    public boolean isBound(String parameterName) {
        return boundParameters.containsKey(parameterName);
    }

    public SessionSPI getSession() {
        return session;
    }

    public ResultData getResultData(QueryDomainType<?> queryDomainType) {
        // TODO handle skip and limit
        return ((QueryDomainTypeImpl<?>)queryDomainType).getResultData(this, 0, Long.MAX_VALUE, null, null);
    }

    /** Add a filter to the list of filters created for this query.
     * @param scanFilter the filter
     */
    public void addFilter(ScanFilter scanFilter) {
        filters.add(scanFilter);
    }

    /** Delete all the filters created for this query.
     */
    public void deleteFilters() {
        for (ScanFilter filter: filters) {
            filter.delete();
        }
        filters.clear();
    }

    public void setExplain(Map<String, Object> explain) {
        this.explain = explain;
    }

    public Map<String, Object> getExplain() {
        return explain;
    }

    public Byte getByte(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof Byte) {
            return (Byte)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "Byte"));
        }
    }

    public BigDecimal getBigDecimal(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof BigDecimal) {
            return (BigDecimal)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "BigDecimal"));
        }
    }

    public BigInteger getBigInteger(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof BigInteger) {
            return (BigInteger)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "BigInteger"));
        }
    }

    public Boolean getBoolean(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof Boolean) {
            return (Boolean)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "Boolean"));
        }
    }

    public byte[] getBytes(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof byte[]) {
            return (byte[])result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "byte[]"));
        }
    }

    public Double getDouble(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof Double) {
            return (Double)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "Double"));
        }
    }

    public Float getFloat(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof Float) {
            return (Float)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "Float"));
        }
    }

    public Integer getInt(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof Integer) {
            return (Integer)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "Integer"));
        }
    }

    public Date getJavaSqlDate(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof java.sql.Date) {
            return (java.sql.Date)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "java.sql.Date"));
        }
    }

    public Time getJavaSqlTime(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof java.sql.Time) {
            return (java.sql.Time)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "java.sql.Time"));
        }
    }

    public Timestamp getJavaSqlTimestamp(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof java.sql.Timestamp) {
            return (java.sql.Timestamp)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "java.sql.Timestamp"));
        }
    }

    public java.util.Date getJavaUtilDate(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof java.util.Date) {
            return (java.util.Date)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "java.util.Date"));
        }
    }

    public Long getLong(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof Long) {
            return (Long)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "Long"));
        }
    }

    public Short getShort(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof Short) {
            return (Short)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "Short"));
        }
    }

    public String getString(String index) {
        Object result = boundParameters.get(index);
        if (result == null) {
            return null;
        }
        if (result instanceof String) {
            return (String)result;
        } else {
            throw new ClusterJUserException(local.message("ERR_Parameter_Wrong_Type", index, result.getClass(), "String"));
        }
    }

    public Object getObject(String index) {
        return boundParameters.get(index);
    }

    public boolean hasNoNullParameters() {
        for (Object value: boundParameters.values()) {
            if (value == null) {
                return false;
            }
        }
        return true;
    }

}
