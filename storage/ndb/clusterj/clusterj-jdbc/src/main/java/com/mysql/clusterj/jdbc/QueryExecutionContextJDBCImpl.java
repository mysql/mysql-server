/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.jdbc;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.core.query.QueryExecutionContextImpl;
import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/** This class handles retrieving parameter values from the parameterBindings
 * associated with a PreparedStatement.
 */
public class QueryExecutionContextJDBCImpl extends QueryExecutionContextImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(QueryExecutionContextJDBCImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(QueryExecutionContextJDBCImpl.class);

    /** The wrapped ParameterBindings */
    ValueHandler parameterBindings;

    /** The number of parameters */
    int numberOfParameters;

    /** Create a new execution context with parameter bindings.
     * @param parameterBindings the jdbc parameter bindings for the statement
     * @param session the session for this context
     * @param numberOfParameters the number of parameters per statement
     */
    public QueryExecutionContextJDBCImpl(SessionSPI session,
            ValueHandler parameterBindings, int numberOfParameters) {
        super(session);
        this.parameterBindings = parameterBindings;
        this.numberOfParameters = numberOfParameters;
    }

    public Byte getByte(String index) {
        int parameterIndex = Integer.valueOf(index);
        Byte result = parameterBindings.getByte(parameterIndex);
        return result;
    }

    public BigDecimal getBigDecimal(String index) {
        int parameterIndex = Integer.valueOf(index);
        BigDecimal result = parameterBindings.getBigDecimal(parameterIndex);
        return result;
    }

    public BigInteger getBigInteger(String index) {
        int parameterIndex = Integer.valueOf(index);
        BigInteger result = parameterBindings.getBigDecimal(parameterIndex).toBigInteger();
        return result;
    }

    public Boolean getBoolean(String index) {
        int parameterIndex = Integer.valueOf(index);
        Boolean result = parameterBindings.getBoolean(parameterIndex);
        return result;
    }

    public byte[] getBytes(String index) {
        int parameterIndex = Integer.valueOf(index);
        byte[] result = parameterBindings.getBytes(parameterIndex);
        return result;
    }

    public Double getDouble(String index) {
        int parameterIndex = Integer.valueOf(index);
        Double result = parameterBindings.getDouble(parameterIndex);
        return result;
    }

    public Float getFloat(String index) {
        int parameterIndex = Integer.valueOf(index);
        Float result = parameterBindings.getFloat(parameterIndex);
        return result;
    }

    public Integer getInt(String index) {
        int parameterIndex = Integer.valueOf(index);
        Integer result = parameterBindings.getInt(parameterIndex);
        return result;
    }

    public Date getJavaSqlDate(String index) {
        int parameterIndex = Integer.valueOf(index);
        java.sql.Date result = parameterBindings.getJavaSqlDate(parameterIndex);
        return result;
    }

    public Time getJavaSqlTime(String index) {
        int parameterIndex = Integer.valueOf(index);
        Time result = parameterBindings.getJavaSqlTime(parameterIndex);
        return result;
    }

    public Timestamp getJavaSqlTimestamp(String index) {
        int parameterIndex = Integer.valueOf(index);
        java.sql.Timestamp result = parameterBindings.getJavaSqlTimestamp(parameterIndex);
        return result;
    }

    public java.util.Date getJavaUtilDate(String index) {
        int parameterIndex = Integer.valueOf(index);
        java.util.Date result = parameterBindings.getJavaUtilDate(parameterIndex);
        return result;
    }

    public Long getLong(String index) {
        int parameterIndex = Integer.valueOf(index);
        Long result = parameterBindings.getLong(parameterIndex);
        return result;
    }

    public Short getShort(String index) {
        int parameterIndex = Integer.valueOf(index);
        Short result = parameterBindings.getShort(parameterIndex);
        return result;
    }

    public String getString(String index) {
        int parameterIndex = Integer.valueOf(index);
        String result = parameterBindings.getString(parameterIndex);
        return result;
    }

    public Object getObject(String index) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public Object getValueHandler() {
        return parameterBindings;
    }

}
