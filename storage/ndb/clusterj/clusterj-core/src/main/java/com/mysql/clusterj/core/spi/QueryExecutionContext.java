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
