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
import java.sql.SQLException;
import java.sql.Time;
import java.sql.Timestamp;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.jdbc.ParameterBindings;

/** This class handles retrieving parameter values from the parameterBindings
 * associated with a PreparedStatement.
 */
public class ValueHandlerImpl implements ValueHandler {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ValueHandlerImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(ValueHandlerImpl.class);

    private ParameterBindings parameterBindings;
    private int[] fieldNumberMap;

    /** The offset into the parameter bindings, used for batch processing */
    private int offset;

    public ValueHandlerImpl(ParameterBindings parameterBindings, int[] fieldNumberMap, int offset) {
        this.parameterBindings = parameterBindings;
        this.fieldNumberMap = fieldNumberMap;
        this.offset = offset;
    }

    public ValueHandlerImpl(ParameterBindings parameterBindings, int[] fieldNumberMap) {
        this(parameterBindings, fieldNumberMap, 0);
    }

    public BigDecimal getBigDecimal(int fieldNumber) {
        try {
            return parameterBindings.getBigDecimal(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public BigInteger getBigInteger(int fieldNumber) {
        try {
            return parameterBindings.getBigDecimal(offset + fieldNumberMap[fieldNumber]).toBigInteger();
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public boolean getBoolean(int fieldNumber) {
        try {
            return parameterBindings.getBoolean(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public boolean[] getBooleans(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public byte getByte(int fieldNumber) {
        try {
            return parameterBindings.getByte(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public byte[] getBytes(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public double getDouble(int fieldNumber) {
        try {
            return parameterBindings.getDouble(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public float getFloat(int fieldNumber) {
        try {
            return parameterBindings.getFloat(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public int getInt(int fieldNumber) {
        try {
            return parameterBindings.getInt(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Date getJavaSqlDate(int fieldNumber) {
        try {
            return parameterBindings.getDate(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Time getJavaSqlTime(int fieldNumber) {
        try {
            return parameterBindings.getTime(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Timestamp getJavaSqlTimestamp(int fieldNumber) {
        try {
            return parameterBindings.getTimestamp(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public java.util.Date getJavaUtilDate(int fieldNumber) {
        try {
            return parameterBindings.getDate(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public long getLong(int fieldNumber) {
        try {
            return parameterBindings.getLong(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Boolean getObjectBoolean(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public Byte getObjectByte(int fieldNumber) {
        try {
            return parameterBindings.getByte(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Double getObjectDouble(int fieldNumber) {
        try {
            return parameterBindings.getDouble(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Float getObjectFloat(int fieldNumber) {
        try {
            return parameterBindings.getFloat(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Integer getObjectInt(int fieldNumber) {
        try {
            return parameterBindings.getInt(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Long getObjectLong(int fieldNumber) {
        try {
            return parameterBindings.getLong(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public Short getObjectShort(int fieldNumber) {
        try {
            return parameterBindings.getShort(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public short getShort(int fieldNumber) {
        try {
            return parameterBindings.getShort(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public String getString(int fieldNumber) {
        try {
            return parameterBindings.getString(offset + fieldNumberMap[fieldNumber]);
        } catch (SQLException e) {
            throw new ClusterJDatastoreException(e);
        }
    }

    public boolean isModified(int fieldNumber) {
        return fieldNumberMap[fieldNumber] != -1;
    }

    public boolean isNull(int fieldNumber) {
        try {
            return parameterBindings.isNull(offset + fieldNumberMap[fieldNumber]);
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

}
