/*
   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.core.metadata;

import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.DynamicObjectDelegate;

import com.mysql.clusterj.core.CacheManager;
import com.mysql.clusterj.core.StateManager;
import com.mysql.clusterj.core.StoreManager;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;

import java.math.BigDecimal;
import java.math.BigInteger;

import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.BitSet;
import java.util.HashMap;
import java.util.Map;

public class InvocationHandlerImpl<T> implements InvocationHandler,
        StateManager, ValueHandler, DynamicObjectDelegate {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(InvocationHandlerImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(InvocationHandlerImpl.class);

    /** The properties of the instance. */
    protected Object[] properties;

    /** The number of fields */
    protected int numberOfFields;

    /** The types of the properties. */
    protected Map<String, Class<?>> typemap = new HashMap<String, Class<?>>();

    /** The DomainTypeHandlerImpl for this class. */
    protected DomainTypeHandlerImpl<T> domainTypeHandler;

    /** The modifiedFields bit for each field. */
    BitSet modifiedFields;

    /** Has this object been modified? */
    boolean modified = false;

    /** The proxy object this handler handles. */
    private Object proxy;

    /** The cache manager for object modification notifications. */
    private CacheManager objectManager;

    /** Has this object been found in the database? */
    private Boolean found = null;

    public InvocationHandlerImpl(DomainTypeHandlerImpl<T> domainTypeHandler) {
        this.domainTypeHandler = domainTypeHandler;
        numberOfFields = domainTypeHandler.getNumberOfFields();
        properties = new Object[numberOfFields];
        modifiedFields = new BitSet(numberOfFields);
        domainTypeHandler.initializeNotPersistentFields(this);
    }

    public void setProxy(Object proxy) {
        this.proxy = proxy;
    }

    public Object getProxy() {
        return proxy;
    }

    public void setCacheManager(CacheManager manager) {
        objectManager = manager;
        resetModified();
    }

    public Object invoke(Object proxy, Method method, Object[] args)
            throws Throwable {
        String methodName = method.getName();
        if (logger.isDetailEnabled()) logger.detail("invoke with Method: " + method.getName());
        
        String propertyHead = methodName.substring(3,4);
        String propertyTail = methodName.substring(4);
        String propertyName = propertyHead.toLowerCase() + propertyTail;
        int fieldNumber;
        if (methodName.startsWith("get")) {
            if (logger.isDetailEnabled()) logger.detail("Property name: " + propertyName);
            // get the field number from the name
            fieldNumber = domainTypeHandler.getFieldNumber(propertyName);
            if (logger.isDetailEnabled()) logger.detail(methodName + ": Returning field number " + fieldNumber
                + " value: " + properties[fieldNumber]);
            return properties[fieldNumber];
        } else if (methodName.startsWith("set")) {
            if (logger.isDetailEnabled()) logger.detail("Property name: " + propertyName);
            // get the field number from the name
            fieldNumber = domainTypeHandler.getFieldNumber(propertyName);
            // mark the field as modified
            if (!modified && objectManager != null) {
                modified = true;
                objectManager.markModified(this);
                if (logger.isDetailEnabled()) logger.detail("modifying " + this);
            }
            modifiedFields.set(fieldNumber);
            properties[fieldNumber] = args[0];
        } else if ("toString".equals(methodName)) {
            return(domainTypeHandler.getDomainClass().getName()
                    + pkToString(domainTypeHandler));
        } else if ("hashCode".equals(methodName)) {
            return(this.hashCode());
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Method_Name", methodName));
        }
        return null;
    }

    public void markModified(int fieldNumber) {
        modifiedFields.set(fieldNumber);
    }

    public String pkToString(DomainTypeHandler<?> domainTypeHandler) {
        StringBuffer sb = new StringBuffer(" key: [");
        int[] keys = domainTypeHandler.getKeyFieldNumbers();
        String separator = "";
        for (int i: keys) {
            sb.append(separator);
            sb.append(properties[i]);
            separator = ";";
        }
        sb.append("]");
        return sb.toString();
    }

    /** Reset the modified flags
     *
     */
    public void resetModified() {
        modified = false;
        modifiedFields.clear();
    }

    public boolean isNull(int fieldNumber) {
        return properties[fieldNumber] == null;
    }

    public BigInteger bigIntegerValue(int fieldNumber) {
        return (BigInteger) properties[fieldNumber];
    }

    public boolean booleanValue(int fieldNumber) {
        return (Boolean) properties[fieldNumber];
    }

    public boolean[] booleansValue(int fieldNumber) {
        return (boolean[]) properties[fieldNumber];
    }

    public byte[] bytesValue(int fieldNumber) {
        return (byte[]) properties[fieldNumber];
    }

    public byte byteValue(int fieldNumber) {
        return (Byte) properties[fieldNumber];
    }

    public Byte objectByteValue(int fieldNumber) {
        return (Byte) properties[fieldNumber];
    }

    public java.util.Date javaUtilDateValue(int fieldNumber) {
        return (java.util.Date) properties[fieldNumber];
    }

    public java.sql.Date javaSqlDateValue(int fieldNumber) {
        return (java.sql.Date) properties[fieldNumber];
    }

    public java.sql.Time javaSqlTimeValue(int fieldNumber) {
        return (java.sql.Time) properties[fieldNumber];
    }

    public java.sql.Timestamp javaSqlTimestampValue(int fieldNumber) {
        return (java.sql.Timestamp) properties[fieldNumber];
    }

    public BigDecimal decimalValue(int fieldNumber) {
        return (BigDecimal) properties[fieldNumber];
    }

    public double doubleValue(int fieldNumber) {
        return (Double) properties[fieldNumber];
    }

    public float floatValue(int fieldNumber) {
        return (Float) properties[fieldNumber];
    }

    public int intValue(int fieldNumber) {
        if (logger.isDetailEnabled()) logger.detail("intValue: Returning field number " + fieldNumber
                + " value: " + properties[fieldNumber]);
        return (Integer) properties[fieldNumber];
    }

    public Integer objectIntValue(int fieldNumber) {
        return (Integer) properties[fieldNumber];
    }

    public long longValue(int fieldNumber) {
        return (Long) properties[fieldNumber];
    }

    public short shortValue(int fieldNumber) {
        return (Short) properties[fieldNumber];
    }

    public String stringValue(int fieldNumber) {
        if (logger.isDetailEnabled()) logger.detail("stringValue: Returning field number " + fieldNumber
                + " value: " + properties[fieldNumber]);
        return (String) properties[fieldNumber];
    }

    public void setValue(int fieldNumber, Object value) {
        if (logger.isDetailEnabled()) logger.detail("setValue: Setting field number " + fieldNumber
                + " to value " + value);
        properties[fieldNumber] = value;
    }

    public void flush(StoreManager stm) {
        // ignore flush until deferred updates are supported
    }

    public boolean isModified(int fieldNumber) {
        return modifiedFields.get(fieldNumber);
    }

    public BigInteger getBigInteger(int fieldNumber) {
        return bigIntegerValue(fieldNumber);
    }

    public boolean getBoolean(int fieldNumber) {
        return booleanValue(fieldNumber);
    }

    public boolean[] getBooleans(int fieldNumber) {
        return booleansValue(fieldNumber);
    }

    public Boolean getObjectBoolean(int fieldNumber) {
        return booleanValue(fieldNumber);
    }

    public byte getByte(int fieldNumber) {
        return byteValue(fieldNumber);
    }

    public byte[] getBytes(int fieldNumber) {
        return bytesValue(fieldNumber);
    }

    public short getShort(int fieldNumber) {
        return shortValue(fieldNumber);
    }

    public int getInt(int fieldNumber) {
        return intValue(fieldNumber);
    }

    public long getLong(int fieldNumber) {
        return longValue(fieldNumber);
    }

    public float getFloat(int fieldNumber) {
        return floatValue(fieldNumber);
    }

    public double getDouble(int fieldNumber) {
        return doubleValue(fieldNumber);
    }

    public Byte getObjectByte(int fieldNumber) {
        return (Byte)properties[fieldNumber];
    }

    public Short getObjectShort(int fieldNumber) {
        return (Short)properties[fieldNumber];
    }

    public Integer getObjectInt(int fieldNumber) {
       return (Integer)properties[fieldNumber];
    }

    public Long getObjectLong(int fieldNumber) {
        return (Long)properties[fieldNumber];
    }

    public Float getObjectFloat(int fieldNumber) {
        return (Float)properties[fieldNumber];
    }

    public Double getObjectDouble(int fieldNumber) {
        return (Double)properties[fieldNumber];
    }

    public BigDecimal getBigDecimal(int fieldNumber) {
        return (BigDecimal)properties[fieldNumber];
    }

    public String getString(int fieldNumber) {
        return (String)properties[fieldNumber];
    }

    public Date getJavaSqlDate(int fieldNumber) {
        return (java.sql.Date)properties[fieldNumber];
    }

    public java.util.Date getJavaUtilDate(int fieldNumber) {
        return (java.util.Date)properties[fieldNumber];
    }

    public Time getJavaSqlTime(int fieldNumber) {
        return (java.sql.Time)properties[fieldNumber];
    }

    public Timestamp getJavaSqlTimestamp(int fieldNumber) {
        return (java.sql.Timestamp)properties[fieldNumber];
    }

    public void setBigInteger(int fieldNumber, BigInteger value) {
        properties[fieldNumber] = value;
    }

    public void setBoolean(int fieldNumber, boolean b) {
        properties[fieldNumber] = b;
    }

    public void setBooleans(int fieldNumber, boolean[] b) {
        properties[fieldNumber] = b;
    }

    public void setByte(int fieldNumber, byte value) {
        properties[fieldNumber] = value;
    }

    public void setBytes(int fieldNumber, byte[] value) {
        properties[fieldNumber] = value;
    }

    public void setShort(int fieldNumber, short value) {
        properties[fieldNumber] = value;
    }

    public void setInt(int fieldNumber, int value) {
        properties[fieldNumber] = value;
    }

    public void setLong(int fieldNumber, long value) {
        properties[fieldNumber] = value;
    }

    public void setFloat(int fieldNumber, float value) {
        properties[fieldNumber] = value;
    }

    public void setDouble(int fieldNumber, double value) {
        properties[fieldNumber] = value;
    }

    public void setObjectBoolean(int fieldNumber, Boolean value) {
        properties[fieldNumber] = value;
    }

    public void setObjectByte(int fieldNumber, Byte value) {
        properties[fieldNumber] = value;
    }

    public void setObjectShort(int fieldNumber, Short value) {
        properties[fieldNumber] = value;
    }

    public void setObjectInt(int fieldNumber, Integer value) {
        properties[fieldNumber] = value;
    }

    public void setObjectLong(int fieldNumber, Long value) {
        properties[fieldNumber] = value;
    }

    public void setObjectFloat(int fieldNumber, Float value) {
        properties[fieldNumber] = value;
    }

    public void setObjectDouble(int fieldNumber, Double value) {
        properties[fieldNumber] = value;
    }

    public void setBigDecimal(int fieldNumber, BigDecimal value) {
        properties[fieldNumber] = value;
    }

    public void setString(int fieldNumber, String value) {
        properties[fieldNumber] = value;
    }

    public void setObject(int fieldNumber, Object value) {
        properties[fieldNumber] = value;
    }

    public void setJavaSqlDate(int fieldNumber, java.sql.Date value) {
        properties[fieldNumber] = value;
    }

    public void setJavaUtilDate(int fieldNumber, java.util.Date value) {
        properties[fieldNumber] = value;
    }

    public void setJavaSqlTime(int fieldNumber, java.sql.Time value) {
        properties[fieldNumber] = value;
    }

    public void setJavaSqlTimestamp(int fieldNumber, java.sql.Timestamp value) {
        properties[fieldNumber] = value;
    }

    public Object get(int columnNumber) {
        return properties[columnNumber];
    }

    public void set(int columnNumber, Object value) {
        modifiedFields.set(columnNumber);
        properties[columnNumber] = value;
    }

    public ColumnMetadata[] columnMetadata() {
        return domainTypeHandler.columnMetadata();
    }

    public void found(Boolean found) {
        this.found = found;
    }

    public Boolean found() {
        return found;
    }

}
