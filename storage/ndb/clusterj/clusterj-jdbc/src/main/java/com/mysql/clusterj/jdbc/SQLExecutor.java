/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.query.QueryDomainTypeImpl;
import com.mysql.clusterj.core.query.QueryExecutionContextImpl;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.jdbc.ParameterBindings;
import com.mysql.jdbc.ResultSetInternalMethods;

import java.sql.SQLException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** This class contains behavior to execute various SQL commands. There is one subclass for each
 * command to be executed. 
 */
public class SQLExecutor {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(SQLExecutor.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(SQLExecutor.class);

    /** The domain type handler for this SQL statement */
    DomainTypeHandler<?> domainTypeHandler = null;

    /** The column names in the SQL statement */
    protected List<String> columnNames = null;

    /** The map of field numbers to parameter numbers */
    protected int[] fieldNumberToColumnNumberMap = null;

    /** The map of column numbers to field numbers */
    protected int[] columnNumberToFieldNumberMap = null;

    /** The map of column names to parameter numbers */
    protected Map<String, Integer> columnNameToFieldNumberMap = new HashMap<String, Integer>();

    /** The query domain type for qualified SELECT and DELETE operations */
    protected QueryDomainTypeImpl<?> queryDomainType;

    public SQLExecutor(DomainTypeHandlerImpl<?> domainTypeHandler, List<String> columnNames) {
        this.domainTypeHandler = domainTypeHandler;
        this.columnNames  = columnNames;
        initializeFieldNumberMap();
    }

    public SQLExecutor(DomainTypeHandlerImpl<?> domainTypeHandler, List<String> columnNames,
            QueryDomainTypeImpl<?> queryDomainType) {
        this(domainTypeHandler, columnNames);
        this.queryDomainType = queryDomainType;
    }
    
    public SQLExecutor(DomainTypeHandlerImpl<?> domainTypeHandler, QueryDomainTypeImpl<?> queryDomainType) {
        this.domainTypeHandler = domainTypeHandler;
        this.queryDomainType = queryDomainType;
    }

    /** This is the public interface exposed to other parts of the component. Calling this
     * method executes the SQL statement via the clusterj api, or returns null indicating that
     * the JDBC driver should execute the SQL statement.
     */
    public interface Executor {

        /** Execute the SQL command
         * @param session the clusterj session which must not be null
         * @param parameterBindings the parameter bindings from the prepared statement
         * @return the result of executing the statement, or null
         * @throws SQLException
         */
        ResultSetInternalMethods execute(SessionSPI session,
                ParameterBindings parameterBindings) throws SQLException;
    }

    /** This class implements the Executor contract but returns null, indicating that
     * the JDBC driver should implement the call itself.
     */
    public static class Noop implements Executor {

        public ResultSetInternalMethods execute(SessionSPI session,
                ParameterBindings parameterBindings) throws SQLException {
            return null;
        }
    }

    /** This class implements the Executor contract for Select operations.
     */
    public static class Select extends SQLExecutor implements Executor {

        public Select(DomainTypeHandlerImpl<?> domainTypeHandler, List<String> columnNames, QueryDomainTypeImpl<?> queryDomainType) {
            super(domainTypeHandler, columnNames, queryDomainType);
            if (queryDomainType == null) {
                throw new ClusterJFatalInternalException("queryDomainType must not be null for Select.");
            }
        }

        public ResultSetInternalMethods execute(SessionSPI session,
                ParameterBindings parameterBindings) throws SQLException {
            logParameterBindings(parameterBindings);
            // create value handler to copy data from parameters to ndb
            Map<String, Object> parameters = createParameterMap(queryDomainType, parameterBindings);
            QueryExecutionContextImpl context = new QueryExecutionContextImpl(session, parameters);
            session.startAutoTransaction();
            try {
                ResultData resultData = queryDomainType.getResultData(context);
                session.endAutoTransaction();
                return new ResultSetInternalMethodsImpl(resultData, columnNumberToFieldNumberMap, 
                        columnNameToFieldNumberMap, session);
            } catch (Exception e) {
                e.printStackTrace();
                session.failAutoTransaction();
                return null;
            }
        }
    }

    /** This class implements the Executor contract for Delete operations.
     */
    public static class Delete extends SQLExecutor implements Executor {

        public Delete (DomainTypeHandlerImpl<?> domainTypeHandler, QueryDomainTypeImpl<?> queryDomainType) {
            super(domainTypeHandler, queryDomainType);
        }

        public ResultSetInternalMethods execute(SessionSPI session,
                ParameterBindings parameterBindings) throws SQLException {
            logParameterBindings(parameterBindings);
            // for now, the only delete operation supported is delete all rows
            if (queryDomainType == null) {
                int rowsDeleted = session.deletePersistentAll(domainTypeHandler);
                return new ResultSetInternalMethodsUpdateCount(rowsDeleted);
            } else {
                return null;
            }
        }
    }

    /** This class implements the Executor contract for Insert operations.
     */
    public static class Insert extends SQLExecutor implements Executor {

        public Insert(DomainTypeHandlerImpl<?> domainTypeHandler, List<String> columnNames) {
            super(domainTypeHandler, columnNames);
        }

        public ResultSetInternalMethods execute(SessionSPI session,
                ParameterBindings parameterBindings) throws SQLException {
            logParameterBindings(parameterBindings);
            // session asks for values by field number which are converted to parameter number
            ValueHandler valueHandler = getValueHandler(parameterBindings, fieldNumberToColumnNumberMap);
            session.insert(domainTypeHandler, valueHandler);
            return new ResultSetInternalMethodsUpdateCount(1);
        }
    }

    /** Create the parameter map assigning each bound parameter a number.
     * The result is a map in which the key is a String whose key is a cardinal number
     * and whose value is the parameter's value.
     * @param queryDomainType the query domain type
     * @param parameterBindings the parameter bindings
     * @return
     * @throws SQLException
     */
    protected Map<String, Object> createParameterMap(QueryDomainTypeImpl<?> queryDomainType,
            ParameterBindings parameterBindings) throws SQLException {
        Map<String, Object> result = new HashMap<String, Object>();
        int i = 1;
        while (true) {
            try {
                // back up until you hear a crash
                Object placeholder = parameterBindings.getObject(i);
                if (logger.isDetailEnabled()) logger.detail("Put placeholder " + i + " value: " + placeholder);
                result.put(String.valueOf(i), placeholder);
                ++i;
            } catch (Exception ex) {
                break;
            }
        }
        // TODO what does this do?
//        if (i > 1) {
//            // if there is at least one parameter, name it "?" in addition to "1"
//            result.put("?", parameterBindings.getObject(1));       
//        }
        return result;
    }

    /** Initialize the mappings between the Java representation of the row (domain type handler)
     * and the JDBC/database representation of the row. The JDBC driver asks for columns by column
     * index or column name, 1-origin. The domain type handler returns data by field number, 0-origin.
     * The domain type handler has representations for all columns in the database, whereas the JDBC
     * driver has a specific set of columns referenced by the SQL statement.
     * For insert, the column number to field number mapping will map parameters to field numbers,
     * e.g. INSERT INTO EMPLOYEE (id, name, age) VALUES (?, ?, ?)
     * For select, the column number to field number mapping will map result set columns to field numbers,
     * e.g. SELECT id, name, age FROM EMPLOYEE
     */
    private void initializeFieldNumberMap() {
        // the index into the int[] is the 0-origin field number (columns in order of definition in the schema)
        // the value is the index into the parameter bindings (columns in order of the sql insert statement)
        String[] fieldNames = domainTypeHandler.getFieldNames();
        fieldNumberToColumnNumberMap = new int[fieldNames.length];
        columnNumberToFieldNumberMap = new int[1 + columnNames.size()];
        for (int i= 0; i < fieldNames.length; ++i) {
            columnNameToFieldNumberMap.put(fieldNames[i], i);
            int index = columnNames.indexOf(fieldNames[i]);
            if (index >= 0) {
                // index origin 1 for JDBC interfaces
                fieldNumberToColumnNumberMap[i] = index + 1;
                columnNumberToFieldNumberMap[index + 1] = i;
            } else {
                // field is not in column list
                fieldNumberToColumnNumberMap[i] = -1;
            }
        }
        // make sure all columns are fields and if not, throw an exception
        for (String columnName: columnNames) {
            if (columnNameToFieldNumberMap.get(columnName) == null) {
                throw new ClusterJUserException(
                        local.message("ERR_Column_Name_Not_In_Table", columnName,
                        Arrays.toString(fieldNames),
                        domainTypeHandler.getTableName()));
            }
        }
    }

    /** Create a value handler (part of the clusterj spi) to retrieve values from jdbc parameter bindings.
     * @param parameterBindings the jdbc parameter bindings from prepared statements
     * @param fieldNumberToParameterNumberMap map from field number to parameter number
     * @return
     */
    protected ValueHandler getValueHandler(ParameterBindings parameterBindings,
            int[] fieldNumberToParameterNumberMap) {
        return new ValueHandlerImpl(parameterBindings, fieldNumberToParameterNumberMap);
    }

    /** If detailed logging is enabled write the parameter bindings to the log.
     * @param parameterBindings the jdbc parameter bindings
     */
    protected static void logParameterBindings(ParameterBindings parameterBindings) {
        if (logger.isDetailEnabled()) {
            int i = 0;
            while (true) {
                try {
                    String value = parameterBindings.getObject(++i).toString();
                    // parameters are 1-origin per jdbc specification
                    logger.detail("parameterBinding: parameter " + i + " has value: " + value);
                } catch (Exception e) {
                    // we don't know how many parameters are bound...
                    break;
                }
            }
        }
    }

}
