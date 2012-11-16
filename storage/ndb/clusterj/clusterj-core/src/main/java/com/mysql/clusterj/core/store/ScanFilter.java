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

package com.mysql.clusterj.core.store;

import java.math.BigDecimal;
import java.math.BigInteger;

/**
 *
 */
public interface ScanFilter {

    public enum BinaryCondition{COND_GE, COND_LE, COND_EQ, COND_GT, COND_LT, COND_LIKE}

    public enum Group {GROUP_AND, GROUP_OR, GROUP_NAND, GROUP_NOR}

    public void begin();

    public void begin(Group group);

    public void cmpBigInteger(BinaryCondition condition, Column storeColumn, BigInteger value);

    public void cmpBoolean(BinaryCondition condition, Column storeColumn, boolean value);

    public void cmpByte(BinaryCondition condition, Column storeColumn, byte b);

    public void cmpBytes(BinaryCondition condition, Column storeColumn, byte[] value);

    public void cmpDecimal(BinaryCondition condition, Column storeColumn, BigDecimal value);

    public void cmpDouble(BinaryCondition condition, Column storeColumn, double value);

    public void cmpFloat(BinaryCondition condition, Column storeColumn, float value);

    public void cmpShort(BinaryCondition condition, Column storeColumn, short shortValue);

    public void cmpInt(BinaryCondition condition, Column storeColumn, int value);

    public void cmpLong(BinaryCondition condition, Column storeColumn, long longValue);

    public void cmpString(BinaryCondition condition, Column storeColumn, String value);

    public void end();

    public void isNull(Column storeColumn);
    
    public void isNotNull(Column storeColumn);
    
    public void delete();

}
