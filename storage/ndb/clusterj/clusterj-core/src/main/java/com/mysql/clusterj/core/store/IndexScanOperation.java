/*
   Copyright (c) 2009, 2023, Oracle and/or its affiliates.

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
