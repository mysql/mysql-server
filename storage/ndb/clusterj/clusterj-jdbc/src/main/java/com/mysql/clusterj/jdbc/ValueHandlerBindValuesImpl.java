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
import java.sql.Time;
import java.sql.Timestamp;
import java.util.Iterator;
import java.util.List;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.core.CacheManager;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.ValueHandlerBatching;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.jdbc.ServerPreparedStatement.BatchedBindValues;
import com.mysql.jdbc.ServerPreparedStatement.BindValue;

/** This class handles retrieving parameter values from the parameterBindings
 * associated with a PreparedStatement.
 */
public class ValueHandlerBindValuesImpl implements ValueHandlerBatching {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ValueHandlerBindValuesImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(ValueHandlerBindValuesImpl.class);

    private Iterator<?> parameterSetListIterator;
    private BindValue[] bindValues;
    private int[] fieldNumberToColumnNumberMap;
    private int numberOfStatements;
    private int currentStatement = 0;
    private int offset;
    private int numberOfParameters;

    private static final int FIELD_TYPE_BLOB = 252;
    private static final int FIELD_TYPE_DOUBLE = 5;
    private static final int FIELD_TYPE_FLOAT = 4;
    private static final int FIELD_TYPE_LONG = 3;
    private static final int FIELD_TYPE_LONGLONG = 8;
    private static final int FIELD_TYPE_NEW_DECIMAL = 246;
    private static final int FIELD_TYPE_SHORT = 2;
    private static final int FIELD_TYPE_TINY = 1;

    public ValueHandlerBindValuesImpl(List<?> parameterSetList, int[] fieldNumberToColumnNumberMap) {
        this.parameterSetListIterator = parameterSetList.iterator();
        this.numberOfStatements = parameterSetList.size();
        this.fieldNumberToColumnNumberMap = fieldNumberToColumnNumberMap;
    }

    public ValueHandlerBindValuesImpl(BindValue[] bindValues, int[] fieldNumberToColumnNumberMap,
            int numberOfStatements, int numberOfParameters) {
        this.bindValues = bindValues;
        this.fieldNumberToColumnNumberMap = fieldNumberToColumnNumberMap;
        this.numberOfStatements = numberOfStatements;
        this.numberOfParameters = numberOfParameters;
        this.offset = -numberOfParameters;
    }

    /** Position to the next parameter set. If no more parameter sets, return false.
     * @result true if positioned on a valid parameter set
     */
    @Override
    public boolean next() {
        if (parameterSetListIterator == null) {
            offset += numberOfParameters;
            return currentStatement++ < numberOfStatements;
        }
        if (parameterSetListIterator.hasNext()) {
            Object parameterSet = parameterSetListIterator.next();
            if (parameterSet instanceof BatchedBindValues) {
                bindValues = ((BatchedBindValues)parameterSet).batchedParameterValues;
            } else {
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Mixed_Server_Prepared_Statement_Values", parameterSet.getClass().getName()));
            }
            return true;
        } else {
            return false;
        }
    }

    @Override
    public int getNumberOfStatements() {
        return numberOfStatements;
    }

    /** Get the index into the BindValue array. The number needs to be increased by 1
     * because SQL is 1-origin while the java array is 0-origin.
     * @param fieldNumber the field number for the requested field
     * @return the 0-origin index into the BindValue array
     */
    private int getIndex(int fieldNumber) {
        return fieldNumberToColumnNumberMap==null? fieldNumber + offset - 1:
            fieldNumberToColumnNumberMap[fieldNumber] + offset - 1;
    }

    @Override
    public BigDecimal getBigDecimal(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        Object value = bindValue.value;
        if (value instanceof BigDecimal) {
            return (BigDecimal) value;
        } else if (value instanceof String) {
            return new BigDecimal((String)value);
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Wrong_Type", "BigDecimal", value.getClass().getName()));
        }
    }

    @Override
    public BigInteger getBigInteger(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        Object value = bindValue.value;
        if (value instanceof BigDecimal) {
            return ((BigDecimal)value).toBigInteger();
        } else if (value instanceof String) {
                return new BigDecimal((String)value).toBigInteger();
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Wrong_Type", "BigDecimal", value.getClass().getName()));
        }
    }

    @Override
    public boolean getBoolean(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public boolean[] getBooleans(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public byte getByte(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        switch(bindValue.bufferType) {
            case FIELD_TYPE_DOUBLE:
                return (byte)bindValue.doubleBinding;
            case FIELD_TYPE_FLOAT:
                return (byte)bindValue.floatBinding;
            case FIELD_TYPE_LONG:
            case FIELD_TYPE_LONGLONG:
            case FIELD_TYPE_SHORT:
            case FIELD_TYPE_TINY:
                return (byte)bindValue.longBinding;
            default:
                throw new ClusterJUserException(
                        local.message("ERR_Parameter_Wrong_Type", "byte", bindValue.getClass().getName()));
        }
    }

    @Override
    public byte[] getBytes(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public byte[] getLobBytes(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public double getDouble(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        switch(bindValue.bufferType) {
            case FIELD_TYPE_DOUBLE:
                return bindValue.doubleBinding;
            case FIELD_TYPE_FLOAT:
                return bindValue.floatBinding;
            case FIELD_TYPE_LONG:
            case FIELD_TYPE_LONGLONG:
            case FIELD_TYPE_SHORT:
            case FIELD_TYPE_TINY:
                return bindValue.longBinding;
            default:
                throw new ClusterJUserException(
                        local.message("ERR_Parameter_Wrong_Type", "double", bindValue.getClass().getName()));
        }
    }

    @Override
    public float getFloat(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        switch(bindValue.bufferType) {
            case FIELD_TYPE_DOUBLE:
                return (float)bindValue.doubleBinding;
            case FIELD_TYPE_FLOAT:
                return bindValue.floatBinding;
            case FIELD_TYPE_LONG:
            case FIELD_TYPE_LONGLONG:
            case FIELD_TYPE_SHORT:
            case FIELD_TYPE_TINY:
                return bindValue.longBinding;
            default:
                throw new ClusterJUserException(
                        local.message("ERR_Parameter_Wrong_Type", "float", bindValue.getClass().getName()));
        }
    }

    @Override
    public int getInt(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        switch(bindValue.bufferType) {
            case FIELD_TYPE_DOUBLE:
                return (int)bindValue.doubleBinding;
            case FIELD_TYPE_FLOAT:
                return (int)bindValue.floatBinding;
            case FIELD_TYPE_LONG:
            case FIELD_TYPE_LONGLONG:
            case FIELD_TYPE_SHORT:
            case FIELD_TYPE_TINY:
                return (int)bindValue.longBinding;
            default:
                throw new ClusterJUserException(
                        local.message("ERR_Parameter_Wrong_Type", "int", bindValue.getClass().getName()));
        }
    }

    @Override
    public Date getJavaSqlDate(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        Object value = bindValue.value;
        if (value instanceof java.sql.Date) {
            return (java.sql.Date) value;
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Wrong_Type", "java.sql.Date", value.getClass().getName()));
        }
    }

    @Override
    public Time getJavaSqlTime(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        Object value = bindValue.value;
        if (value instanceof java.sql.Time) {
            return (java.sql.Time) value;
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Wrong_Type", "java.sql.Time", value.getClass().getName()));
        }
    }

    @Override
    public Timestamp getJavaSqlTimestamp(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        Object value = bindValue.value;
        if (value instanceof java.sql.Timestamp) {
            return (java.sql.Timestamp) value;
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Wrong_Type", "java.sql.Timestamp", value.getClass().getName()));
        }
    }

    @Override
    public java.util.Date getJavaUtilDate(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        Object value = bindValue.value;
        if (value instanceof java.util.Date) {
            return (java.util.Date) value;
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Wrong_Type", "java.util.Date", value.getClass().getName()));
        }
    }

    @Override
    public long getLong(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        switch(bindValue.bufferType) {
            case FIELD_TYPE_DOUBLE:
                return (long)bindValue.doubleBinding;
            case FIELD_TYPE_FLOAT:
                return (long)bindValue.floatBinding;
            case FIELD_TYPE_LONG:
            case FIELD_TYPE_LONGLONG:
            case FIELD_TYPE_SHORT:
            case FIELD_TYPE_TINY:
                return bindValue.longBinding;
            default:
                throw new ClusterJUserException(
                        local.message("ERR_Parameter_Wrong_Type", "long", bindValue.getClass().getName()));
        }
    }

    @Override
    public Boolean getObjectBoolean(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public Byte getObjectByte(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        if (bindValue.isNull) {
            return null;
        } else {
            return getByte(fieldNumber);
        }
    }

    @Override
    public Double getObjectDouble(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        if (bindValue.isNull) {
            return null;
        } else {
            return getDouble(fieldNumber);
        }
    }

    @Override
    public Float getObjectFloat(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        if (bindValue.isNull) {
            return null;
        } else {
            return getFloat(fieldNumber);
        }
    }

    @Override
    public Integer getObjectInt(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        if (bindValue.isNull) {
            return null;
        } else {
            return getInt(fieldNumber);
        }
    }

    @Override
    public Long getObjectLong(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        if (bindValue.isNull) {
            return null;
        } else {
            return getLong(fieldNumber);
        }
    }

    @Override
    public Short getObjectShort(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        if (bindValue.isNull) {
            return null;
        } else {
            return getShort(fieldNumber);
        }
    }

    @Override
    public short getShort(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        switch(bindValue.bufferType) {
            case FIELD_TYPE_DOUBLE:
                return (short)bindValue.doubleBinding;
            case FIELD_TYPE_FLOAT:
                return (short)bindValue.floatBinding;
            case FIELD_TYPE_LONG:
            case FIELD_TYPE_LONGLONG:
            case FIELD_TYPE_SHORT:
            case FIELD_TYPE_TINY:
                return (short)bindValue.longBinding;
            default:
                throw new ClusterJUserException(
                        local.message("ERR_Parameter_Wrong_Type", "short", bindValue.getClass().getName()));
        }
    }

    @Override
    public String getString(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        Object value = bindValue.value;
        if (value instanceof String) {
            return (String) value;
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Wrong_Type", "String", value.getClass().getName()));
        }
    }

    @Override
    public String getLobString(int fieldNumber) {
        BindValue bindValue = bindValues[getIndex(fieldNumber)];
        Object value = bindValue.value;
        if (value instanceof String) {
            return (String) value;
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Parameter_Wrong_Type", "String", value.getClass().getName()));
        }
    }

    @Override
    public boolean isNull(int fieldNumber) {
        return bindValues[getIndex(fieldNumber)].isNull;
    }

    @Override
    public boolean isModified(int fieldNumber) {
        return fieldNumberToColumnNumberMap[fieldNumber] != -1;
    }

    @Override
    public void markModified(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public String pkToString(DomainTypeHandler<?> domainTypeHandler) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void resetModified() {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setBigDecimal(int fieldNumber, BigDecimal value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setBigInteger(int fieldNumber, BigInteger bigIntegerExact) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setBoolean(int fieldNumber, boolean b) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setBooleans(int fieldNumber, boolean[] b) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setByte(int fieldNumber, byte value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setBytes(int fieldNumber, byte[] value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setLobBytes(int fieldNumber, byte[] value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setDouble(int fieldNumber, double value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setFloat(int fieldNumber, float value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    public void setInt(int fieldNumber, int value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setJavaSqlDate(int fieldNumber, Date value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setJavaSqlTime(int fieldNumber, Time value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setJavaSqlTimestamp(int fieldNumber, Timestamp value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setJavaUtilDate(int fieldNumber, java.util.Date value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setLong(int fieldNumber, long value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObject(int fieldNumber, Object value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectBoolean(int fieldNumber, Boolean value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectByte(int fieldNumber, Byte value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectDouble(int fieldNumber, Double value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectFloat(int fieldNumber, Float value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectInt(int fieldNumber, Integer value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectLong(int fieldNumber, Long value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectShort(int fieldNumber, Short value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setShort(int fieldNumber, short value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setString(int fieldNumber, String value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setLobString(int fieldNumber, String value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public ColumnMetadata[] columnMetadata() {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public Boolean found() {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void found(Boolean found) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public Object get(int columnNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void set(int columnNumber, Object value) {
        throw new ClusterJFatalInternalException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setCacheManager(CacheManager cm) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Unsupported_Method",
                "setCacheManager(CacheManager)", "ValueHandlerBindValuesImpl"));
    }

    @Override
    public void setProxy(Object proxy) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Unsupported_Method",
                "setProxy(Object)", "ValueHandlerBindValuesImpl"));
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
                "invoke(Object, Method, Object[])", "ValueHandlerBindValuesImpl"));
    }

    @Override
    public void release() {
        if (logger.isDetailEnabled()) logger.detail("ValueHandlerBindValuesImpl.release");
        if (this.bindValues != null) {
            for (int i = 0; i < this.bindValues.length; ++i) {
                if (this.bindValues[i] != null && !this.bindValues[i].isNull) {
                  this.bindValues[i].value = null;
                  this.bindValues[i] = null;
                }
            }
            this.bindValues = null;
        }
    }

}
