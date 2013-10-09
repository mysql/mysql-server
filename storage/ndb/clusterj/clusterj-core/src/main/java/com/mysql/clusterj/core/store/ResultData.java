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

package com.mysql.clusterj.core.store;

/**
 *
 */
public interface ResultData {

    public java.math.BigDecimal getDecimal(Column storeColumn);

    public java.math.BigInteger getBigInteger(Column columnName);

    public Blob getBlob(Column storeColumn);

    public boolean getBoolean(Column storeColumn);

    public boolean[] getBooleans(Column storeColumn);

    public byte getByte(Column storeColumn);

    public double getDouble(Column storeColumn);

    public float getFloat(Column storeColumn);

    public int getInt(Column storeColumn);

    public long getLong(Column storeColumn);

    public short getShort(Column storeColumn);

    public Boolean getObjectBoolean(Column storeColumn);

    public Byte getObjectByte(Column storeColumn);

    public Double getObjectDouble(Column storeColumn);

    public Float getObjectFloat(Column storeColumn);

    public Integer getObjectInteger(Column storeColumn);

    public Long getObjectLong(Column storeColumn);

    public Short getObjectShort(Column storeColumn);

    public String getString(Column storeColumn);

    public boolean next();

    public byte[] getBytes(Column storeColumn);

    public Object getObject(Column storeColumn);

    public java.math.BigDecimal getDecimal(int columnNumber);

    public java.math.BigInteger getBigInteger(int columnNumber);

    public Blob getBlob(int columnNumber);

    public boolean getBoolean(int columnNumber);

    public boolean[] getBooleans(int columnNumber);

    public byte getByte(int columnNumber);

    public double getDouble(int columnNumber);

    public float getFloat(int columnNumber);

    public int getInt(int columnNumber);

    public long getLong(int columnNumber);

    public short getShort(int columnNumber);

    public Boolean getObjectBoolean(int columnNumber);

    public Byte getObjectByte(int columnNumber);

    public Double getObjectDouble(int columnNumber);

    public Float getObjectFloat(int columnNumber);

    public Integer getObjectInteger(int columnNumber);

    public Long getObjectLong(int columnNumber);

    public Short getObjectShort(int columnNumber);

    public String getString(int columnNumber);

    public byte[] getBytes(int columnNumber);

    public Object getObject(int column);

    public Column[] getColumns();

}
