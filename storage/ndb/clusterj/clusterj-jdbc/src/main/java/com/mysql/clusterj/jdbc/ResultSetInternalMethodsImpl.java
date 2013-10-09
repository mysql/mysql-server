/*
 *  Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.jdbc;

import java.math.BigDecimal;
import java.sql.Date;
import java.sql.SQLException;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.Calendar;
import java.util.HashMap;
import java.util.Map;

import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/** This class is part of the statement interceptor contract with the MySQL JDBC connection.
 * When a statement is intercepted and executed, an instance of this class is returned if there
 * is a real result to be iterated. A sibling class, ResultSetInternalMethodsUpdateCount, is
 * returned if only an insert/delete/update count is returned.
 * This class in turn delegates to the clusterj ResultData to retrieve data from the cluster.
 */
public class ResultSetInternalMethodsImpl extends AbstractResultSetInternalMethods {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ResultSetInternalMethodsImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(ResultSetInternalMethodsImpl.class);

    private ResultData resultData;

    private SessionSPI session;

    private int[] columnIndexToFieldNumberMap;

    private Map<String, Integer> columnNameToFieldNumberMap = new HashMap<String, Integer>();

    private boolean autotransaction = true;

    public ResultSetInternalMethodsImpl(ResultData resultData, int[] columnIndexToFieldNumberMap, 
            Map<String, Integer> columnNameToFieldNumberMap, SessionSPI session) {
        this.columnIndexToFieldNumberMap = columnIndexToFieldNumberMap;
        this.columnNameToFieldNumberMap = columnNameToFieldNumberMap;
        this.resultData = resultData;
        this.session = session;
    }

    @Override
    public boolean reallyResult() {
        return true;
    }

    @Override
    public boolean next() {
        boolean hasNext = resultData.next();
        // startAutoTransaction was called in SQLExecutor.Select.execute and 
        // endAutoTransaction must be called exactly once after all results have been read
        if (autotransaction & !hasNext) {
            session.endAutoTransaction();
            autotransaction = false;
        }
        if (logger.isDetailEnabled()) logger.detail("ResultSetInternalMethods.next returned: " + hasNext);
        return hasNext;
    }

    @Override
    public long getUpdateID() {
        return 0;
    }

    @Override
    public boolean getBoolean(int columnIndex) throws SQLException {
        boolean result = resultData.getBoolean(columnIndexToFieldNumberMap[columnIndex]);
        return result;
    }

    @Override
    public boolean getBoolean(String columnName) throws SQLException {
        boolean result = resultData.getBoolean(getFieldNumberForColumnName(columnName));
        return result;
    }

    @Override
    public byte getByte(int columnIndex) throws SQLException {
        byte result = resultData.getByte(columnIndexToFieldNumberMap[columnIndex]);
        return result;
    }

    @Override
    public byte getByte(String columnName) throws SQLException {
        byte result = resultData.getByte(getFieldNumberForColumnName(columnName));
        return result;
    }

    @Override
    public byte[] getBytes(int columnIndex) throws SQLException {
        byte[] result = resultData.getBytes(columnIndexToFieldNumberMap[columnIndex]);
        return result;
    }

    @Override
    public byte[] getBytes(String columnName) throws SQLException {
        byte[] result = resultData.getBytes(getFieldNumberForColumnName(columnName));
        return result;
    }

    @Override
    public Date getDate(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public Date getDate(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public Date getDate(int columnIndex, Calendar cal) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public Date getDate(String columnName, Calendar cal) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    @Override
    public double getDouble(int columnIndex) throws SQLException {
        double result = resultData.getDouble(columnIndexToFieldNumberMap[columnIndex]);
        return result;
    }

    @Override
    public double getDouble(String columnName) throws SQLException {
        double result = resultData.getDouble(getFieldNumberForColumnName(columnName));
        return result;
    }

    @Override
    public float getFloat(int columnIndex) throws SQLException {
        float result = resultData.getFloat(columnIndexToFieldNumberMap[columnIndex]);
        return result;
    }

    @Override
    public float getFloat(String columnName) throws SQLException {
        float result = resultData.getFloat(getFieldNumberForColumnName(columnName));
        return result;
    }

    @Override
    public long getLong(int columnIndex) throws SQLException {
        long result = resultData.getLong(columnIndexToFieldNumberMap[columnIndex]);
        return result;
    }

    @Override
    public long getLong(String columnName) throws SQLException {
        long result = resultData.getLong(getFieldNumberForColumnName(columnName));
        return result;
    }

    @Override
    public int getInt(int columnIndex) throws SQLException {
        int result = resultData.getInt(columnIndexToFieldNumberMap[columnIndex]);
        return result;
    }

    @Override
    public int getInt(String columnName) throws SQLException {
        int result = resultData.getInt(getFieldNumberForColumnName(columnName));
        return result;
    }

    @Override
    public short getShort(int columnIndex) throws SQLException {
        short result = resultData.getShort(columnIndexToFieldNumberMap[columnIndex]);
        return result;
    }

    @Override
    public short getShort(String columnName) throws SQLException {
        short result = resultData.getShort(getFieldNumberForColumnName(columnName));
        return result;
    }

    @Override
    public String getString(int columnIndex) throws SQLException {
        String result = resultData.getString(columnIndexToFieldNumberMap[columnIndex]);
        return result;
    }

    @Override
    public String getString(String columnName) throws SQLException {
        String result = resultData.getString(getFieldNumberForColumnName(columnName));
        return result;
    }

    @Override
    public Time getTime(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    @Override
    public Time getTime(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    @Override
    public Time getTime(int columnIndex, Calendar cal) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    @Override
    public Time getTime(String columnName, Calendar cal) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    @Override
    public Timestamp getTimestamp(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    @Override
    public Timestamp getTimestamp(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    @Override
    public Timestamp getTimestamp(int columnIndex, Calendar cal) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    @Override
    public Timestamp getTimestamp(String columnName, Calendar cal) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    @Override
    public BigDecimal getBigDecimal(int columnIndex) throws SQLException {
        BigDecimal result = resultData.getDecimal(columnIndexToFieldNumberMap[columnIndex]);
        return result;
    }

    @Override
    public BigDecimal getBigDecimal(String columnName) throws SQLException {
        BigDecimal result = resultData.getDecimal(getFieldNumberForColumnName(columnName));
        return result;
    }

    @Override
    public void realClose(boolean arg0) throws SQLException {
        // if next() was never called to end the autotransaction, do so now
        if (autotransaction) {
            session.endAutoTransaction();
            autotransaction = false;
        }
    }

    private int getFieldNumberForColumnName(String columnName) throws SQLException {
        Integer fieldNumber = columnNameToFieldNumberMap.get(columnName);
        if (fieldNumber != null) {
            return fieldNumber.intValue();
        }
        throw new SQLException(local.message("ERR_Column_Name_Not_In_Result", columnName));
    }

}
