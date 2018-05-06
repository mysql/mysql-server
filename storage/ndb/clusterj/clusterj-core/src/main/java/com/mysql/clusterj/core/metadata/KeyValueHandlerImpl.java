/*
   Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.core.metadata;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.core.CacheManager;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.lang.reflect.Method;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;

/** KeyValueHandlerImpl implements the ValueHandler interface for an array
 * of objects that represent key values. 
 *
 */
public class KeyValueHandlerImpl implements ValueHandler {
    static final I18NHelper local = I18NHelper.getInstance(KeyValueHandlerImpl.class);
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(KeyValueHandlerImpl.class);

    private Object[] values;

    /** The number of fields */
    private int numberOfFields;
    
    public KeyValueHandlerImpl(Object[] keyValues) {
        this.values = keyValues;
        this.numberOfFields = keyValues.length;
        if (logger.isDetailEnabled()) {
            StringBuffer buffer = new StringBuffer();
            for (int i = 0; i < values.length; ++i) {
                if (values[i] != null) {
                    buffer.append(" values[" + i +"]: \"" + values[i] + "\"");
                }
            }
            logger.detail("KeyValueHandler<init> values.length: " + values.length + buffer.toString());
        }
    }

    public void release() {
        this.values = null;
    }

    public boolean wasReleased() {
        return this.values == null;
    }

    public boolean isNull(int fieldNumber) {
        return values[fieldNumber] == null;
    }

    public int getInt(int fieldNumber) {
        if (logger.isDetailEnabled()) logger.detail("KeyValueHandler.getInt(" + fieldNumber + ") returns: " + values[fieldNumber]);
        return (Integer) values[fieldNumber];
    }

    public long getLong(int fieldNumber) {
        if (logger.isDetailEnabled()) logger.detail("KeyValueHandler.getLong(" + fieldNumber + ") returns: " + values[fieldNumber]);
        return (Long) values[fieldNumber];
    }

    public Integer getObjectInt(int fieldNumber) {
        if (logger.isDetailEnabled()) logger.detail("KeyValueHandler.getObjectInt(" + fieldNumber + ") returns: " + values[fieldNumber]);
        return (Integer) values[fieldNumber];
    }

    public Long getObjectLong(int fieldNumber) {
        if (logger.isDetailEnabled()) logger.detail("KeyValueHandler.getObjectLong(" + fieldNumber + ") returns: " + values[fieldNumber]);
        return (Long) values[fieldNumber];
    }

    public String getString(int fieldNumber) {
        if (logger.isDetailEnabled()) logger.detail("KeyValueHandler.getString(" + fieldNumber + ") returns: " + values[fieldNumber]);
        return (String) values[fieldNumber];
    }

    public boolean isModified(int fieldNumber) {
        throw new UnsupportedOperationException(
                local.message("ERR_Operation_Not_Supported",
                "isModified", "KeyValueHandlerImpl"));
    }

    public BigInteger getBigInteger(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getBigInteger", "KeyValueHandlerImpl"));
    }

    public boolean getBoolean(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getBoolean", "KeyValueHandlerImpl"));
    }

    public boolean[] getBooleans(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getBooleans", "KeyValueHandlerImpl"));
    }

    public byte getByte(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getByte", "KeyValueHandlerImpl"));
    }

    public byte[] getBytes(int fieldNumber) {
        if (logger.isDetailEnabled()) logger.detail("KeyValueHandler.getBytes(" + fieldNumber + ") returns: " + values[fieldNumber]);
        return (byte[]) values[fieldNumber];
    }

    public short getShort(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getShort", "KeyValueHandlerImpl"));
    }

    public float getFloat(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getFloat", "KeyValueHandlerImpl"));
    }

    public double getDouble(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getDouble", "KeyValueHandlerImpl"));
    }

    public Boolean getObjectBoolean(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getObjectBoolean", "KeyValueHandlerImpl"));
    }

    public Byte getObjectByte(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getObjectByte", "KeyValueHandlerImpl"));
    }

    public Short getObjectShort(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getObjectShort", "KeyValueHandlerImpl"));
    }

    public Float getObjectFloat(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getObjectFloat", "KeyValueHandlerImpl"));
    }

    public Double getObjectDouble(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getObjectDouble", "KeyValueHandlerImpl"));
    }

    public BigDecimal getBigDecimal(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getBigDecimal", "KeyValueHandlerImpl"));
    }

    public Date getJavaSqlDate(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getJavaSqlDate", "KeyValueHandlerImpl"));
    }

    public java.util.Date getJavaUtilDate(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getJavaUtilDate", "KeyValueHandlerImpl"));
    }

    public Time getJavaSqlTime(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getJavaSqlTime", "KeyValueHandlerImpl"));
    }

    public Timestamp getJavaSqlTimestamp(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getJavaSqlTimestamp", "KeyValueHandlerImpl"));
    }

    public byte[] getLobBytes(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getLobBytes", "KeyValueHandlerImpl"));
    }

    public String getLobString(int fieldNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getLobString", "KeyValueHandlerImpl"));
    }

    public void setBoolean(int fieldNumber, boolean b) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setBoolean", "KeyValueHandlerImpl"));
    }

    public void setBooleans(int fieldNumber, boolean[] b) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setBooleans", "KeyValueHandlerImpl"));
    }

    public void setBigInteger(int fieldNumber, BigInteger value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setBigInteger", "KeyValueHandlerImpl"));
    }

    public void setByte(int fieldNumber, byte value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setByte", "KeyValueHandlerImpl"));
    }

    public void setBytes(int fieldNumber, byte[] value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setBytes", "KeyValueHandlerImpl"));
    }

    public void setLobBytes(int fieldNumber, byte[] value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setLobBytes", "KeyValueHandlerImpl"));
    }

    public void setLobString(int fieldNumber, String value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setLobString", "KeyValueHandlerImpl"));
    }

    public void setShort(int fieldNumber, short value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setShort", "KeyValueHandlerImpl"));
    }

    public void setInt(int fieldNumber, int value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setInt", "KeyValueHandlerImpl"));
    }

    public void setLong(int fieldNumber, long value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setLong", "KeyValueHandlerImpl"));
    }

    public void setFloat(int fieldNumber, float value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setFloat", "KeyValueHandlerImpl"));
    }

    public void setDouble(int fieldNumber, double value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setDouble", "KeyValueHandlerImpl"));
    }

    public void setObjectBoolean(int fieldNumber, Boolean value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setObjectBoolean", "KeyValueHandlerImpl"));
    }

    public void setObjectByte(int fieldNumber, Byte value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setObjectByte", "KeyValueHandlerImpl"));
    }

    public void setObjectShort(int fieldNumber, Short value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setObjectShort", "KeyValueHandlerImpl"));
    }

    public void setObjectInt(int fieldNumber, Integer value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setObjectInt", "KeyValueHandlerImpl"));
    }

    public void setObjectLong(int fieldNumber, Long value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setObjectLong", "KeyValueHandlerImpl"));
    }

    public void setObjectFloat(int fieldNumber, Float value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setObjectFloat", "KeyValueHandlerImpl"));
    }

    public void setObjectDouble(int fieldNumber, Double value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setObjectDouble", "KeyValueHandlerImpl"));
    }

    public void setBigDecimal(int fieldNumber, BigDecimal value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setBigDecimal", "KeyValueHandlerImpl"));
    }

    public void setString(int fieldNumber, String value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setString", "KeyValueHandlerImpl"));
    }

    public void setObject(int fieldNumber, Object value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setObject", "KeyValueHandlerImpl"));
    }

    public void setJavaSqlDate(int fieldNumber, Date value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setJavaSqlDate", "KeyValueHandlerImpl"));
    }

    public void setJavaUtilDate(int fieldNumber, java.util.Date value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setJavaUtilDate", "KeyValueHandlerImpl"));
    }

    public void setJavaSqlTime(int fieldNumber, Time value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setJavaSqlTime", "KeyValueHandlerImpl"));
    }

    public void setJavaSqlTimestamp(int fieldNumber, Timestamp value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setJavaSqlTimestamp", "KeyValueHandlerImpl"));
    }

    public void markModified(int fieldNumber) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public void resetModified() {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public String pkToString(DomainTypeHandler<?> domainTypeHandler) {
        StringBuffer sb = new StringBuffer(" key: [");
        int[] keys = domainTypeHandler.getKeyFieldNumbers();
        String separator = "";
        for (int i: keys) {
            sb.append(separator);
            sb.append(values[i]);
            separator = ";";
        }
        sb.append("]");
        return sb.toString();
    }

    public void found(Boolean found) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "found(Boolean)", "KeyValueHandlerImpl"));
    }

    public ColumnMetadata[] columnMetadata() {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "columnMetadata", "KeyValueHandlerImpl"));
    }

    public Boolean found() {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "found", "KeyValueHandlerImpl"));
    }

    public Object get(int columnNumber) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "get(int)", "KeyValueHandlerImpl"));
    }

    public void set(int columnNumber, Object value) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "set(int, Object)", "KeyValueHandlerImpl"));
    }

    public void setProxy(Object proxy) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setProxy(Object)", "KeyValueHandlerImpl"));
    }

    public Object getProxy() {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "getProxy()", "KeyValueHandlerImpl"));
    }

    public Object invoke(Object proxy, Method method, Object[] args)
            throws Throwable {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "invoke(Object, Method, Object[])", "KeyValueHandlerImpl"));
    }

    public void setCacheManager(CacheManager cm) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Operation_Not_Supported",
                "setCacheManager(CacheManager)", "KeyValueHandlerImpl"));
    }

}
