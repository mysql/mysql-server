/*
   Copyright (C) 2009 Sun Microsystems Inc.
   All rights reserved. Use is subject to license terms.

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

}
