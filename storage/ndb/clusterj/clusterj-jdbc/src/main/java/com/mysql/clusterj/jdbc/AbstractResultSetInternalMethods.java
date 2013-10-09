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

import java.io.InputStream;
import java.io.Reader;
import java.math.BigDecimal;
import java.net.URL;
import java.sql.Array;
import java.sql.Blob;
import java.sql.Clob;
import java.sql.Date;
import java.sql.NClob;
import java.sql.Ref;
import java.sql.ResultSetMetaData;
import java.sql.RowId;
import java.sql.SQLException;
import java.sql.SQLWarning;
import java.sql.SQLXML;
import java.sql.Statement;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.Calendar;
import java.util.Map;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.jdbc.CachedResultSetMetaData;
import com.mysql.jdbc.Field;
import com.mysql.jdbc.PreparedStatement;
import com.mysql.jdbc.ResultSetInternalMethods;
import com.mysql.jdbc.StatementImpl;

public abstract class AbstractResultSetInternalMethods implements
        ResultSetInternalMethods {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(InterceptorImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(InterceptorImpl.class);

    /** First char of query */
    protected char firstCharOfQuery;

    public void buildIndexMapping() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void clearNextResult() {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur")); 
    }

    public ResultSetInternalMethods copy() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public int getBytesSize() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public char getFirstCharOfQuery() {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public ResultSetInternalMethods getNextResultSet() {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public Object getObjectStoredProc(int arg0, int arg1) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Object getObjectStoredProc(String arg0, int arg1)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Object getObjectStoredProc(int arg0, Map arg1, int arg2)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Object getObjectStoredProc(String arg0, Map arg1, int arg2)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public String getServerInfo() {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public long getUpdateCount() {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public long getUpdateID() {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public void initializeFromCachedMetaData(CachedResultSetMetaData arg0) {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public void initializeWithMetadata() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public void populateCachedMetaData(CachedResultSetMetaData arg0)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public void realClose(boolean arg0) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public boolean reallyResult() {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public void redefineFieldsForDBMD(Field[] arg0) {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public void setFirstCharOfQuery(char arg0) {
        firstCharOfQuery = arg0;
    }

    public void setOwningStatement(StatementImpl arg0) {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public void setStatementUsedForFetchingRows(PreparedStatement arg0) {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public void setWrapperStatement(Statement arg0) {
        throw new RuntimeException(local.message("ERR_Should_Not_Occur"));
    }

    public boolean absolute(int row) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public void afterLast() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public void beforeFirst() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public void cancelRowUpdates() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public void clearWarnings() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public void close() throws SQLException {
    }

    public void deleteRow() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public int findColumn(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public boolean first() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Array getArray(int i) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Array getArray(String colName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public InputStream getAsciiStream(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public InputStream getAsciiStream(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public BigDecimal getBigDecimal(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public BigDecimal getBigDecimal(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public BigDecimal getBigDecimal(int columnIndex, int scale)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public BigDecimal getBigDecimal(String columnName, int scale)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public InputStream getBinaryStream(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public InputStream getBinaryStream(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Blob getBlob(int i) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Blob getBlob(String colName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public boolean getBoolean(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public boolean getBoolean(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public byte getByte(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public byte getByte(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public byte[] getBytes(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public byte[] getBytes(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Reader getCharacterStream(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Reader getCharacterStream(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Clob getClob(int i) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Clob getClob(String colName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public int getConcurrency() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public String getCursorName() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Date getDate(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Date getDate(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Date getDate(int columnIndex, Calendar cal) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Date getDate(String columnName, Calendar cal) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public double getDouble(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public double getDouble(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public int getFetchDirection() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public int getFetchSize() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public float getFloat(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public float getFloat(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public int getHoldability() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public int getInt(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public int getInt(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public long getLong(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public long getLong(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public ResultSetMetaData getMetaData() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Reader getNCharacterStream(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Reader getNCharacterStream(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public String getNString(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public String getNString(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public Object getObject(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Object getObject(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Object getObject(int i, Map<String, Class<?>> map)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Object getObject(String colName, Map<String, Class<?>> map)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Ref getRef(int i) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public Ref getRef(String colName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public int getRow() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public RowId getRowId(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public RowId getRowId(String columnLabel) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public short getShort(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public short getShort(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public SQLXML getSQLXML(String columnLabel) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public SQLXML getSQLXML(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public NClob getNClob(String columnLabel) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public NClob getNClob(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public Statement getStatement() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public String getString(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public String getString(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public Time getTime(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public Time getTime(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public Time getTime(int columnIndex, Calendar cal) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public Time getTime(String columnName, Calendar cal) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public Timestamp getTimestamp(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public Timestamp getTimestamp(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public Timestamp getTimestamp(int columnIndex, Calendar cal)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public Timestamp getTimestamp(String columnName, Calendar cal)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public int getType() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public URL getURL(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public URL getURL(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public InputStream getUnicodeStream(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public InputStream getUnicodeStream(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public SQLWarning getWarnings() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void insertRow() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean isAfterLast() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean isBeforeFirst() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean isClosed() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean isFirst() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean isLast() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean isWrapperFor(Class<?> iface) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean last() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void moveToCurrentRow() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void moveToInsertRow() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean next() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean previous() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void refreshRow() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean relative(int rows) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean rowDeleted() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean rowInserted() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean rowUpdated() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void setFetchDirection(int direction) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void setFetchSize(int rows) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public <T> T unwrap(Class<T> iface) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateArray(int columnIndex, Array x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateArray(String columnName, Array x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateAsciiStream(int columnIndex, InputStream x, int length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateAsciiStream(String columnName, InputStream x, int length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateAsciiStream(int columnIndex, InputStream x, long length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateAsciiStream(String columnName, InputStream x, long length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateAsciiStream(int columnIndex, InputStream x)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateAsciiStream(String columnName, InputStream x)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBigDecimal(int columnIndex, BigDecimal x)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBigDecimal(String columnName, BigDecimal x)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBinaryStream(int columnIndex, InputStream x, int length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBinaryStream(String columnName, InputStream x, int length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }


    public void updateBinaryStream(int columnIndex, InputStream x, long length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBinaryStream(String columnName, InputStream x, long length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBinaryStream(int columnIndex, InputStream x)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBinaryStream(String columnName, InputStream x)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBlob(int columnIndex, Blob x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBlob(String columnName, Blob x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBlob(int columnIndex, InputStream istream) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBlob(String columnName, InputStream istream) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBlob(int columnIndex, InputStream istream, long length) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBlob(String columnName, InputStream istream, long length) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBoolean(int columnIndex, boolean x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBoolean(String columnName, boolean x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateByte(int columnIndex, byte x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateByte(String columnName, byte x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBytes(int columnIndex, byte[] x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateBytes(String columnName, byte[] x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateCharacterStream(int columnIndex, Reader x)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateCharacterStream(String columnName, Reader x)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateCharacterStream(int columnIndex, Reader x, int length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateCharacterStream(String columnName, Reader reader, int length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateCharacterStream(int columnIndex, Reader x, long length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateCharacterStream(String columnName, Reader reader, long length)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateClob(int columnIndex, Clob x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateClob(String columnName, Clob x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateClob(String columnName, Reader reader) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateClob(int columnIndex, Reader reader) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateClob(String columnName, Reader reader, long length) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateClob(int columnIndex, Reader reader, long length) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateDate(int columnIndex, Date x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateDate(String columnName, Date x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateDouble(int columnIndex, double x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateDouble(String columnName, double x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateFloat(int columnIndex, float x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateFloat(String columnName, float x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateInt(int columnIndex, int x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur"));
    }

    public void updateInt(String columnName, int x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateLong(int columnIndex, long x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateLong(String columnName, long x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNCharacterStream(int columnIndex, Reader reader) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNCharacterStream(String columnName, Reader reader) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNCharacterStream(int columnIndex, Reader reader, long length) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNCharacterStream(String columnName, Reader reader, long length) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNClob(int columnIndex, NClob nclob) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNClob(String columnName, NClob nclob) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNClob(int columnIndex, Reader reader) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNClob(String columnName, Reader reader) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNClob(int columnIndex, Reader reader, long length) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNClob(String columnName, Reader reader, long length) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNString(int columnIndex, String string) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNString(String columnName, String string) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNull(int columnIndex) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateNull(String columnName) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateObject(int columnIndex, Object x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateObject(String columnName, Object x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateObject(int columnIndex, Object x, int scale)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateObject(String columnName, Object x, int scale)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateRef(int columnIndex, Ref x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateRef(String columnName, Ref x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateRow() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateRowId(int columnIndex, RowId x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateRowId(String columnLabel, RowId x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateShort(int columnIndex, short x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateShort(String columnName, short x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateSQLXML(String columnLabel, SQLXML xmlObject) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateSQLXML(int columnIndex, SQLXML xmlObject) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateString(int columnIndex, String x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateString(String columnName, String x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateTime(int columnIndex, Time x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateTime(String columnName, Time x) throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateTimestamp(int columnIndex, Timestamp x)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public void updateTimestamp(String columnName, Timestamp x)
            throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

    public boolean wasNull() throws SQLException {
        throw new SQLException(local.message("ERR_Should_Not_Occur")); 
    }

}
