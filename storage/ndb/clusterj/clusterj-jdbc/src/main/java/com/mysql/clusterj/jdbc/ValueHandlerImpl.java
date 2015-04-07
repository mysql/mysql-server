/*
 *  Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.
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

import java.lang.reflect.Method;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.sql.Date;
import java.sql.SQLException;
import java.sql.Time;
import java.sql.Timestamp;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.core.CacheManager;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.ValueHandlerBatching;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.jdbc.ParameterBindings;

/** This class handles retrieving parameter values from the parameterBindings
 * associated with a PreparedStatement.
 */
public class ValueHandlerImpl implements ValueHandlerBatching {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ValueHandlerImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(ValueHandlerImpl.class);

    private ParameterBindings parameterBindings;
    private final int[] fieldNumberMap;
    private final int numberOfBoundParameters;
    private final int numberOfStatements;
    private final int numberOfParameters;
    /** The offset into the parameter bindings, used for batch processing */
    private int offset;

    public ValueHandlerImpl(ParameterBindings parameterBindings, int[] fieldNumberMap, 
            int numberOfStatements, int numberOfParameters) {
        this.parameterBindings = parameterBindings;
        this.fieldNumberMap = fieldNumberMap;
        this.numberOfParameters = numberOfParameters;
        this.offset = -numberOfParameters;
        this.numberOfBoundParameters = numberOfStatements * numberOfParameters;
        this.numberOfStatements = numberOfStatements;
    }

    @Override
    public int getNumberOfStatements() {
        return numberOfStatements;
    }

    @Override
    public boolean next() {
        offset += numberOfParameters;
        return offset < numberOfBoundParameters;
    }

    /** Return the index into the parameterBindings for this offset and field number.
     * Offset moves the "window" to the next set of parameters for multi-statement
     * (batched) statements.
     * @param fieldNumber the origin-0 number
     * @return the index into the parameterBindings
     */
    private int getIndex(int fieldNumber) {
        int result = fieldNumberMap == null ? offset + fieldNumber : offset + fieldNumberMap[fieldNumber];
        return result;
    }

    public BigDecimal getBigDecimal(int fieldNumber) {
        try {
            return parameterBindings.getBigDecimal(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public BigInteger getBigInteger(int fieldNumber) {
        try {
            return parameterBindings.getBigDecimal(getIndex(fieldNumber)).toBigInteger();
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public boolean getBoolean(int fieldNumber) {
        try {
            return parameterBindings.getBoolean(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public boolean[] getBooleans(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public byte getByte(int fieldNumber) {
        try {
            return parameterBindings.getByte(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public byte[] getBytes(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public byte[] getLobBytes(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public double getDouble(int fieldNumber) {
        try {
            return parameterBindings.getDouble(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public float getFloat(int fieldNumber) {
        try {
            return parameterBindings.getFloat(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public int getInt(int fieldNumber) {
        try {
            return parameterBindings.getInt(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Date getJavaSqlDate(int fieldNumber) {
        try {
            return parameterBindings.getDate(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Time getJavaSqlTime(int fieldNumber) {
        try {
            return parameterBindings.getTime(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Timestamp getJavaSqlTimestamp(int fieldNumber) {
        try {
            return parameterBindings.getTimestamp(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public java.util.Date getJavaUtilDate(int fieldNumber) {
        try {
            return parameterBindings.getDate(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public long getLong(int fieldNumber) {
        try {
            return parameterBindings.getLong(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Boolean getObjectBoolean(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public Byte getObjectByte(int fieldNumber) {
        try {
            return parameterBindings.getByte(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Double getObjectDouble(int fieldNumber) {
        try {
            return parameterBindings.getDouble(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Float getObjectFloat(int fieldNumber) {
        try {
            return parameterBindings.getFloat(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Integer getObjectInt(int fieldNumber) {
        try {
            return parameterBindings.getInt(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Long getObjectLong(int fieldNumber) {
        try {
            return parameterBindings.getLong(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Short getObjectShort(int fieldNumber) {
        try {
            return parameterBindings.getShort(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public short getShort(int fieldNumber) {
        try {
            return parameterBindings.getShort(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public String getString(int fieldNumber) {
        try {
            return parameterBindings.getString(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public String getLobString(int fieldNumber) {
        try {
            return parameterBindings.getString(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public boolean isModified(int fieldNumber) {
        return fieldNumberMap[fieldNumber] != -1;
    }

    public boolean isNull(int fieldNumber) {
        try {
            return parameterBindings.isNull(getIndex(fieldNumber));
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public void markModified(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public String pkToString(DomainTypeHandler<?> domainTypeHandler) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void resetModified() {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setBigDecimal(int fieldNumber, BigDecimal value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setBigInteger(int fieldNumber, BigInteger bigIntegerExact) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setBoolean(int fieldNumber, boolean b) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setBooleans(int fieldNumber, boolean[] b) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setByte(int fieldNumber, byte value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setBytes(int fieldNumber, byte[] value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setLobBytes(int fieldNumber, byte[] value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setDouble(int fieldNumber, double value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setFloat(int fieldNumber, float value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setInt(int fieldNumber, int value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setJavaSqlDate(int fieldNumber, Date value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setJavaSqlTime(int fieldNumber, Time value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setJavaSqlTimestamp(int fieldNumber, Timestamp value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setJavaUtilDate(int fieldNumber, java.util.Date value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setLong(int fieldNumber, long value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setObject(int fieldNumber, Object value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setObjectBoolean(int fieldNumber, Boolean value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setObjectByte(int fieldNumber, Byte value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setObjectDouble(int fieldNumber, Double value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setObjectFloat(int fieldNumber, Float value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setObjectInt(int fieldNumber, Integer value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setObjectLong(int fieldNumber, Long value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setObjectShort(int fieldNumber, Short value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setShort(int fieldNumber, short value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setString(int fieldNumber, String value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setLobString(int fieldNumber, String value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public ColumnMetadata[] columnMetadata() {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public Boolean found() {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void found(Boolean found) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public Object get(int columnNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void set(int columnNumber, Object value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setCacheManager(CacheManager cm) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Unsupported_Method",
                "setCacheManager(CacheManager)", "ValueHandlerImpl"));
    }

    @Override
    public void setProxy(Object proxy) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Unsupported_Method",
                "setProxy(Object)", "ValueHandlerImpl"));
    }

    @Override
    public Object getProxy() {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Unsupported_Method",
                "getProxy()", "ValueHandlerImpl"));
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Unsupported_Method",
                "invoke(Object, Method, Object[])", "ValueHandlerImpl"));
    }

    @Override
    public void release() {
        if (logger.isDetailEnabled()) logger.detail("ValueHandlerImpl.release");
        this.parameterBindings = null;
    }

}
