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

import java.math.BigDecimal;
import java.math.BigInteger;

/**
 *
 */
public interface Operation {

    public void equalBigInteger(Column storeColumn, BigInteger value);

    public void equalBoolean(Column storeColumn, boolean booleanValue);

    public void equalByte(Column storeColumn, byte byteValue);

    public void equalBytes(Column storeColumn, byte[] bytesValue);

    public void equalDecimal(Column storeColumn, BigDecimal bigDecimal);

    public void equalShort(Column storeColumn, short shortValue);

    public void equalInt(Column storeColumn, int intValue);

    public void equalLong(Column storeColumn, long longValue);

    public void equalString(Column storeColumn, String stringValue);

    public void getBlob(Column storeColumn);

    public Blob getBlobHandle(Column storeColumn);

    public void getValue(Column storeColumn);

    public void postExecuteCallback(Runnable callback);

    public ResultData resultData();

    public void setBigInteger(Column storeColumn, BigInteger value);

    public void setBoolean(Column storeColumn, Boolean value);

    public void setByte(Column storeColumn, byte b);

    public void setBytes(Column storeColumn, byte[] b);

    public void setDecimal(Column storeColumn, BigDecimal bigDecimal);

    public void setDouble(Column storeColumn, Double aDouble);

    public void setFloat(Column storeColumn, Float aFloat);

    public void setInt(Column storeColumn, Integer integer);

    public void setLong(Column storeColumn, long longValue);

    public void setNull(Column storeColumn);

    public void setShort(Column storeColumn, Short aShort);

    public void setString(Column storeColumn, String string);

}
