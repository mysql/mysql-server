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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

package com.mysql.clusterj.jdbc;

import java.lang.reflect.Method;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.core.CacheManager;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.ValueHandlerBatching;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 * ValueHandlerBatchingJDBCSetImpl implements the ValueHandler interface
 * to provide values to the update method from parameter values
 * in the JDBC SQL UPDATE... SET statement. It is a wrapper around
 * ValueHandlerBatchingJDBCImpl.
 * 
 * Values are requested by field number. The field number is converted
 * to an offset into the parameter list and then delegated to
 * the ValueHandlerBatchingJDBCImpl which takes as an argument
 * the relative parameter number within the batch.
 * 
 * The mapping from field number to parameter number is done by the
 * fieldNumberToParameterNumber map provided as a constructor parameter.
 * The map is an int[] of size numberOfFields in the domain type. Each
 * element in the int[] is either -1 which means that the field has no parameter
 * value, or the offset into the parameter list for the value.
 * 
 * This interface also must implement the isModified method used to determine
 * whether the field value is provided in the parameter list. If the field
 * is not in the parameter list, then the field is not modified.
 */
public class ValueHandlerBatchingJDBCSetImpl implements ValueHandlerBatching {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ValueHandlerBatchingJDBCSetImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(ValueHandlerBatchingJDBCSetImpl.class);

    /** The delegate for calls */
    private ValueHandlerBatching delegate;

    /** The map from field number to parameter number */
    private int[] fieldNumberToParameterNumber;

    public ValueHandlerBatchingJDBCSetImpl(int[] fieldNumberToParameterNumber,
            ValueHandlerBatching valueHandlerBatching) {
        this.delegate = valueHandlerBatching;
        this.fieldNumberToParameterNumber = fieldNumberToParameterNumber;
    }
    @Override
    public BigDecimal getBigDecimal(int arg0) {
        return delegate.getBigDecimal(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public BigInteger getBigInteger(int arg0) {
        return delegate.getBigInteger(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public boolean getBoolean(int arg0) {
        return delegate.getBoolean(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public boolean[] getBooleans(int arg0) {
        return delegate.getBooleans(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public byte getByte(int arg0) {
        return delegate.getByte(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public byte[] getBytes(int arg0) {
        return delegate.getBytes(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public byte[] getLobBytes(int arg0) {
        return delegate.getBytes(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public double getDouble(int arg0) {
        return delegate.getDouble(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public float getFloat(int arg0) {
        return delegate.getFloat(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public int getInt(int arg0) {
        return delegate.getInt(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public Date getJavaSqlDate(int arg0) {
        return delegate.getJavaSqlDate(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public Time getJavaSqlTime(int arg0) {
        return delegate.getJavaSqlTime(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public Timestamp getJavaSqlTimestamp(int arg0) {
        return delegate.getJavaSqlTimestamp(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public java.util.Date getJavaUtilDate(int arg0) {
        return delegate.getJavaUtilDate(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public long getLong(int arg0) {
        return delegate.getLong(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public Boolean getObjectBoolean(int arg0) {
        return delegate.getObjectBoolean(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public Byte getObjectByte(int arg0) {
        return delegate.getObjectByte(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public Double getObjectDouble(int arg0) {
        return delegate.getObjectDouble(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public Float getObjectFloat(int arg0) {
        return delegate.getObjectFloat(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public Integer getObjectInt(int arg0) {
        return delegate.getObjectInt(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public Long getObjectLong(int arg0) {
        return delegate.getObjectLong(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public Short getObjectShort(int arg0) {
        return delegate.getObjectShort(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public short getShort(int arg0) {
        return delegate.getShort(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public String getString(int arg0) {
        return delegate.getString(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public String getLobString(int arg0) {
        return delegate.getString(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public boolean isModified(int arg0) {
        return fieldNumberToParameterNumber[arg0] != -1;
    }

    @Override
    public boolean isNull(int arg0) {
        return delegate.isNull(fieldNumberToParameterNumber[arg0]);
    }

    @Override
    public void markModified(int arg0) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public String pkToString(DomainTypeHandler<?> arg0) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void resetModified() {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setBigDecimal(int arg0, BigDecimal arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setBigInteger(int arg0, BigInteger arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setBoolean(int arg0, boolean arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setBooleans(int arg0, boolean[] arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setByte(int arg0, byte arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setBytes(int arg0, byte[] arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setLobBytes(int arg0, byte[] arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setDouble(int arg0, double arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setFloat(int arg0, float arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setInt(int arg0, int arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setJavaSqlDate(int arg0, Date arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setJavaSqlTime(int arg0, Time arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setJavaSqlTimestamp(int arg0, Timestamp arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setJavaUtilDate(int arg0, java.util.Date arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setLong(int arg0, long arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObject(int arg0, Object arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectBoolean(int arg0, Boolean arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectByte(int arg0, Byte arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectDouble(int arg0, Double arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectFloat(int arg0, Float arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectInt(int arg0, Integer arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectLong(int arg0, Long arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setObjectShort(int arg0, Short arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setShort(int arg0, short arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setString(int arg0, String arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void setLobString(int arg0, String arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public ColumnMetadata[] columnMetadata() {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public Boolean found() {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void found(Boolean arg0) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public Object get(int arg0) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public void set(int arg0, Object arg1) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Should_Not_Occur"));
    }
    @Override
    public int getNumberOfStatements() {
        return delegate.getNumberOfStatements();
    }

    @Override
    public boolean next() {
        return delegate.next();
    }

    @Override
    public void setCacheManager(CacheManager cm) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Unsupported_Method",
                "setCacheManager(CacheManager)", "ValueHandlerBatching"));
    }

    @Override
    public void setProxy(Object proxy) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Unsupported_Method",
                "setProxy(Object)", "ValueHandlerBatching"));
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
                "invoke(Object, Method, Object[])", "ValueHandlerBatching"));
    }

    @Override
    public void release() {
        if (logger.isDetailEnabled()) logger.detail("ValueHandlerBatchingJDBCSetImpl.release");
        this.delegate.release();
        this.fieldNumberToParameterNumber = null;
    }

}
