/*
 *  Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
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
import java.sql.SQLException;
import java.sql.Time;
import java.sql.Timestamp;

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.query.QueryExecutionContextImpl;
import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.jdbc.ParameterBindings;

/** This class handles retrieving parameter values from the parameterBindings
 * associated with a PreparedStatement.
 */
public class QueryExecutionContextJDBCImpl extends QueryExecutionContextImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(QueryExecutionContextJDBCImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(QueryExecutionContextJDBCImpl.class);

    /** The wrapped ParameterBindings */
    ParameterBindings parameterBindings;

    /** The current offset */
    int offset = 0;

    /** The number of parameters */
    int numberOfParameters;

    /** Create a new execution context with parameter bindings.
     * @param parameterBindings the jdbc parameter bindings for the statement
     * @param session the session for this context
     * @param numberOfParameters the number of parameters per statement
     */
    public QueryExecutionContextJDBCImpl(SessionSPI session,
            ParameterBindings parameterBindings, int numberOfParameters) {
        super(session);
        this.parameterBindings = parameterBindings;
        this.numberOfParameters = numberOfParameters;
    }

    /** Advance to the next statement (and next number of affected rows).
     */
    public void nextStatement() {
        offset += numberOfParameters;
    }

    public Byte getByte(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            Byte result = parameterBindings.getByte(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public BigDecimal getBigDecimal(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            BigDecimal result = parameterBindings.getBigDecimal(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public BigInteger getBigInteger(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            BigInteger result = parameterBindings.getBigDecimal(parameterIndex).toBigInteger();
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public Boolean getBoolean(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            Boolean result = parameterBindings.getBoolean(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public byte[] getBytes(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            byte[] result = parameterBindings.getBytes(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public Double getDouble(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            Double result = parameterBindings.getDouble(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public Float getFloat(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            Float result = parameterBindings.getFloat(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public Integer getInt(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            Integer result = parameterBindings.getInt(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public Date getJavaSqlDate(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            java.sql.Date result = parameterBindings.getDate(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public Time getJavaSqlTime(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            Time result = parameterBindings.getTime(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public Timestamp getJavaSqlTimestamp(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            java.sql.Timestamp result = parameterBindings.getTimestamp(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public java.util.Date getJavaUtilDate(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            java.util.Date result = parameterBindings.getDate(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public Long getLong(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            Long result = parameterBindings.getLong(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public Short getShort(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            Short result = parameterBindings.getShort(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public String getString(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            String result = parameterBindings.getString(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

    public Object getObject(String index) {
        try {
            int parameterIndex = Integer.valueOf(index) + offset;
            Object result = parameterBindings.getObject(parameterIndex);
            return result;
        } catch (SQLException ex) {
                throw new ClusterJUserException(local.message("ERR_Getting_Parameter_Value", offset, index), ex);
        }
    }

}
