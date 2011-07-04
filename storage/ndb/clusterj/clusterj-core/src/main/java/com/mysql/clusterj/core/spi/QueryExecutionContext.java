/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

}
