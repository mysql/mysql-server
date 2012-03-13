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

package com.mysql.clusterj.core.spi;

import java.lang.reflect.InvocationHandler;
import java.math.BigDecimal;
import java.math.BigInteger;

import com.mysql.clusterj.DynamicObjectDelegate;
import com.mysql.clusterj.core.CacheManager;

/** ValueHandler is the interface that must be implemented for core
 * components to access values of a managed instance.
 *
 */
public interface ValueHandler extends DynamicObjectDelegate, InvocationHandler {

    public String pkToString(DomainTypeHandler<?> domainTypeHandler);

    boolean isNull(int fieldNumber);
    boolean isModified(int fieldNumber);
    void markModified(int fieldNumber);
    void resetModified();

    BigInteger getBigInteger(int fieldNumber);
    boolean getBoolean(int fieldNumber);
    boolean[] getBooleans(int fieldNumber);
    byte getByte(int fieldNumber);
    byte[] getBytes(int fieldNumber);
    short getShort(int fieldNumber);
    int getInt(int fieldNumber);
    byte[] getLobBytes(int fieldNumber);
    String getLobString(int fieldNumber);
    long getLong(int fieldNumber);
    float getFloat(int fieldNumber);
    double getDouble(int fieldNumber);
    Boolean getObjectBoolean(int fieldNumber);
    Byte getObjectByte(int fieldNumber);
    Short getObjectShort(int fieldNumber);
    Integer getObjectInt(int fieldNumber);
    Long getObjectLong(int fieldNumber);
    Float getObjectFloat(int fieldNumber);
    Double getObjectDouble(int fieldNumber);
    BigDecimal getBigDecimal(int fieldNumber);
    String getString(int fieldNumber);
    java.sql.Date getJavaSqlDate(int fieldNumber);
    java.util.Date getJavaUtilDate(int fieldNumber);
    java.sql.Time getJavaSqlTime(int fieldNumber);
    java.sql.Timestamp getJavaSqlTimestamp(int fieldNumber);

    void setBigInteger(int fieldNumber, BigInteger bigIntegerExact);
    void setBoolean(int fieldNumber, boolean b);
    void setBooleans(int fieldNumber, boolean[] b);
    void setByte(int fieldNumber, byte value);
    void setBytes(int fieldNumber, byte[] value);
    void setShort(int fieldNumber, short value);
    void setInt(int fieldNumber, int value);
    void setLong(int fieldNumber, long value);
    void setFloat(int fieldNumber, float value);
    void setDouble(int fieldNumber, double value);
    void setLobBytes(int fieldNumber, byte[] value);
    void setLobString(int fieldNumber, String value);
    void setObjectBoolean(int fieldNumber, Boolean value);
    void setObjectByte(int fieldNumber, Byte value);
    void setObjectShort(int fieldNumber, Short value);
    void setObjectInt(int fieldNumber, Integer value);
    void setObjectLong(int fieldNumber, Long value);
    void setObjectFloat(int fieldNumber, Float value);
    void setObjectDouble(int fieldNumber, Double value);
    void setBigDecimal(int fieldNumber, BigDecimal value);
    void setString(int fieldNumber, String value);
    void setObject(int fieldNumber, Object value);
    void setJavaSqlDate(int fieldNumber, java.sql.Date value);
    void setJavaUtilDate(int fieldNumber, java.util.Date value);
    void setJavaSqlTime(int fieldNumber, java.sql.Time value);
    void setJavaSqlTimestamp(int fieldNumber, java.sql.Timestamp value);

    void setCacheManager(CacheManager cm);
    void setProxy(Object proxy);
    Object getProxy();

}
