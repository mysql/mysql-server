/*
   Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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
public interface IndexScanOperation extends ScanOperation {

    public enum BoundType { BoundLE, BoundGE, BoundGT, BoundLT, BoundEQ}

    public void setBoundBigInteger(Column storeColumn, BoundType type, BigInteger bigInteger);

    public void setBoundByte(Column storeColumn, BoundType type, byte value);

    public void setBoundBytes(Column storeColumn, BoundType type, byte[] b);

    public void setBoundDecimal(Column storeColumn, BoundType type, BigDecimal bigDecimal);

    public void setBoundDouble(Column storeColumn, BoundType type, Double aDouble);

    public void setBoundFloat(Column storeColumn, BoundType type, Float aFloat);

    public void setBoundShort(Column storeColumn, BoundType type, short shortValue);

    public void setBoundInt(Column storeColumn, BoundType type, Integer integer);

    public void setBoundLong(Column storeColumn, BoundType type, long longValue);

    public void setBoundString(Column storeColumn, BoundType type, String string);

    public void endBound(int rangeNumber);

}
