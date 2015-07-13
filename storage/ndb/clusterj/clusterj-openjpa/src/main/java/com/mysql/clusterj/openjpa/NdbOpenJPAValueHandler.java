/*
   Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.openjpa;

import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.core.CacheManager;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.lang.reflect.Method;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.BitSet;
import org.apache.openjpa.kernel.OpenJPAStateManager;

/**
 *
 */
public class NdbOpenJPAValueHandler implements ValueHandler {

    protected OpenJPAStateManager sm;

    private NdbOpenJPAStoreManager store;

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(NdbOpenJPAValueHandler.class);

    public void release() {
        this.sm = null;
        this.store = null;
    }

    public boolean wasReleased() {
        return this.sm == null;
    }

    protected OpenJPAStateManager getStateManager() {
        return sm;
    }

    protected NdbOpenJPAStoreManager getStoreManager() {
        return store;
    }

    public NdbOpenJPAValueHandler(OpenJPAStateManager sm) {
        this.sm = sm;
    }

    public NdbOpenJPAValueHandler(OpenJPAStateManager sm, NdbOpenJPAStoreManager store) {
        this.sm = sm;
        this.store = store;
    }

    public boolean isNull(int fieldNumber) {
        return (sm.fetchObject(fieldNumber) == null);
    }

    public boolean isModified(int fieldNumber) {
        BitSet modified = sm.getDirty();
        return modified.get(fieldNumber);
    }

    public void resetModified() {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public void markModified(int fieldNumber) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public boolean getBoolean(int fieldNumber) {
        return sm.fetchBoolean(fieldNumber);
    }

    public boolean[] getBooleans(int fieldNumber) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public Boolean getObjectBoolean(int fieldNumber) {
        return sm.fetchBoolean(fieldNumber);
    }

    public byte getByte(int fieldNumber) {
        return sm.fetchByte(fieldNumber);
    }

    public byte[] getBytes(int fieldNumber) {
        return (byte[])sm.fetchObject(fieldNumber);
    }

    public short getShort(int fieldNumber) {
        return sm.fetchShort(fieldNumber);
    }

    public int getInt(int fieldNumber) {
        int value = sm.fetchInt(fieldNumber);
        if (logger.isDetailEnabled()) logger.detail(" fieldNumber: " + fieldNumber + " value: " + value);
        return value;
    }

    public long getLong(int fieldNumber) {
        long value = sm.fetchLong(fieldNumber);
        if (logger.isDetailEnabled()) logger.detail(" fieldNumber: " + fieldNumber + " value: " + value);
        return value;
    }

    public float getFloat(int fieldNumber) {
        return sm.fetchFloat(fieldNumber);
    }

    public double getDouble(int fieldNumber) {
        return sm.fetchDouble(fieldNumber);
    }

    public Byte getObjectByte(int fieldNumber) {
        return (Byte)sm.fetchObject(fieldNumber);
    }

    public Short getObjectShort(int fieldNumber) {
        return (Short)sm.fetchObject(fieldNumber);
    }

    public Integer getObjectInt(int fieldNumber) {
        Integer value = (Integer)sm.fetchObject(fieldNumber);
        if (logger.isDetailEnabled()) logger.detail(" fieldNumber: " + fieldNumber + " value: " + value);
        return value;
    }

    public Long getObjectLong(int fieldNumber) {
        return (Long)sm.fetchObject(fieldNumber);
    }

    public Float getObjectFloat(int fieldNumber) {
        return (Float)sm.fetchObject(fieldNumber);
    }

    public Double getObjectDouble(int fieldNumber) {
        return (Double)sm.fetchObject(fieldNumber);
    }

    public BigDecimal getBigDecimal(int fieldNumber) {
        return (BigDecimal)sm.fetchObject(fieldNumber);
    }

    public BigInteger getBigInteger(int fieldNumber) {
        return (BigInteger)sm.fetchObject(fieldNumber);
    }

    public String getString(int fieldNumber) {
        return sm.fetchString(fieldNumber);
    }

    public Date getJavaSqlDate(int fieldNumber) {
        return (java.sql.Date)sm.fetchObject(fieldNumber);
    }

    public java.util.Date getJavaUtilDate(int fieldNumber) {
        return (java.util.Date)sm.fetchObject(fieldNumber);
    }

    public Time getJavaSqlTime(int fieldNumber) {
        return (java.sql.Time)sm.fetchObject(fieldNumber);
    }

    public Timestamp getJavaSqlTimestamp(int fieldNumber) {
        return (java.sql.Timestamp)sm.fetchObject(fieldNumber);
    }

    public void setBoolean(int fieldNumber, boolean value) {
        sm.storeBoolean(fieldNumber, value);
    }

    public void setBooleans(int fieldNumber, boolean[] value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setByte(int fieldNumber, byte value) {
        sm.storeByte(fieldNumber, value);
    }

    public void setBytes(int fieldNumber, byte[] value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setShort(int fieldNumber, short value) {
        sm.storeShort(fieldNumber, value);
    }

    public void setInt(int fieldNumber, int value) {
        if (logger.isDetailEnabled()) logger.detail(" fieldNumber: " + fieldNumber + " value: " + value);
//        if (fieldNumber == 0) dumpStackTrace();
        sm.storeInt(fieldNumber, value);
    }

    public void setLong(int fieldNumber, long value) {
        sm.storeLong(fieldNumber, value);
    }

    public void setFloat(int fieldNumber, float value) {
        sm.storeFloat(fieldNumber, value);
    }

    public void setDouble(int fieldNumber, double value) {
        sm.storeDouble(fieldNumber, value);
    }

    public void setObjectBoolean(int fieldNumber, Boolean value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setObjectByte(int fieldNumber, Byte value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setObjectShort(int fieldNumber, Short value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setObjectInt(int fieldNumber, Integer value) {
        if (logger.isDetailEnabled()) logger.detail(" fieldNumber: " + fieldNumber + " value: " + value);
        sm.storeObject(fieldNumber, value);
    }

    public void setObjectLong(int fieldNumber, Long value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setObjectFloat(int fieldNumber, Float value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setObjectDouble(int fieldNumber, Double value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setBigDecimal(int fieldNumber, BigDecimal value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setBigInteger(int fieldNumber, BigInteger value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setString(int fieldNumber, String value) {
        sm.storeString(fieldNumber, value);
    }

    public void setObject(int fieldNumber, Object value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setJavaSqlDate(int fieldNumber, Date value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setJavaUtilDate(int fieldNumber, java.util.Date value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setJavaSqlTime(int fieldNumber, Time value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setJavaSqlTimestamp(int fieldNumber, Timestamp value) {
        sm.storeObject(fieldNumber, value);
    }

    public String pkToString(DomainTypeHandler<?> domainTypeHandler) {
        StringBuffer sb = new StringBuffer(" key: [");
        int[] keys = domainTypeHandler.getKeyFieldNumbers();
        String separator = "";
        for (int i: keys) {
            sb.append(separator);
            sb.append(sm.fetch(i));
            separator = ";";
        }
        sb.append("]");
        return sb.toString();
    }

    @SuppressWarnings("unused")
    private void dumpStackTrace() {
        Throwable t = new Throwable();
        t.printStackTrace();
    }

    public void found(Boolean found) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public ColumnMetadata[] columnMetadata() {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public Boolean found() {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public Object get(int columnNumber) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public void set(int columnNumber, Object value) {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public byte[] getLobBytes(int fieldNumber) {
        return (byte[])sm.fetchObject(fieldNumber);
    }

    public String getLobString(int fieldNumber) {
        return sm.fetchString(fieldNumber);
    }

    public void setCacheManager(CacheManager cm) {
        // we do not need a cache manager...
    }

    public void setLobBytes(int fieldNumber, byte[] value) {
        sm.storeObject(fieldNumber, value);
    }

    public void setLobString(int fieldNumber, String value) {
        sm.storeString(fieldNumber, value);
    }

    public void setProxy(Object proxy) {
        // we do not support Proxy domain model
    }

    public Object invoke(Object proxy, Method method, Object[] args)
            throws Throwable {
        throw new UnsupportedOperationException("Not supported yet.");
    }

    public Object getProxy() {
        throw new UnsupportedOperationException("Not supported yet.");
    }

}
