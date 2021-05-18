/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.core.spi;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.Map;

import com.mysql.clusterj.core.store.ScanFilter;

public interface QueryExecutionContext {

    Object getByte(String index);

    Boolean getBoolean(String index);

    byte[] getBytes(String index);

    String getString(String index);

    BigDecimal getBigDecimal(String index);

    BigInteger getBigInteger(String index);

    Double getDouble(String index);

    Float getFloat(String index);

    Integer getInt(String index);

    java.sql.Date getJavaSqlDate(String index);

    java.sql.Time getJavaSqlTime(String index);

    java.sql.Timestamp getJavaSqlTimestamp(String index);

    java.util.Date getJavaUtilDate(String index);

    Long getLong(String index);

    Short getShort(String index);

    Object getObject(String index);

    SessionSPI getSession();

    void setExplain(Map<String, Object> explain);

    Object getParameterValue(String parameterName);

    boolean isBound(String parameterName);

    void addFilter(ScanFilter scanFilter);

    void deleteFilters();

    boolean hasNoNullParameters();

}
