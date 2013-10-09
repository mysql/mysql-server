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

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.query.QueryDomainTypeImpl;
import com.mysql.clusterj.core.query.QueryExecutionContextImpl;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
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

    /** The number of fields in the domain object (also the number of mapped columns) */
    protected int numberOfFields;

    /** The number of parameters in the where clause */
    protected int numberOfParameters;

    /** The map of field numbers to parameter numbers */
    protected int[] fieldNumberToColumnNumberMap = null;

    /** The map of column numbers to field numbers */
    protected int[] columnNumberToFieldNumberMap = null;

    /** The map of column names to parameter numbers */
    protected Map<String, Integer> columnNameToFieldNumberMap = new HashMap<String, Integer>();

    /** The query domain type for qualified SELECT and DELETE operations */
    protected QueryDomainTypeImpl<?> queryDomainType;

    public SQLExecutor(DomainTypeHandlerImpl<?> domainTypeHandler, List<String> columnNames, int numberOfParameters) {
        this(domainTypeHandler, columnNames);
        this.numberOfParameters = numberOfParameters;
    }

    public SQLExecutor(DomainTypeHandlerImpl<?> domainTypeHandler, List<String> columnNames) {
        this.domainTypeHandler = domainTypeHandler;
        this.columnNames  = columnNames;
        initializeFieldNumberMap();
    }

    public SQLExecutor(DomainTypeHandlerImpl<?> domainTypeHandler) {
        this.domainTypeHandler = domainTypeHandler;
    }

    public SQLExecutor(DomainTypeHandlerImpl<?> domainTypeHandler, List<String> columnNames,
            QueryDomainTypeImpl<?> queryDomainType) {
        this(domainTypeHandler, columnNames);
        this.queryDomainType = queryDomainType;
        initializeFieldNumberMap();
    }
    
    public SQLExecutor(DomainTypeHandlerImpl<?> domainTypeHandler, QueryDomainTypeImpl<?> queryDomainType,
            int numberOfParameters) {
        this.domainTypeHandler = domainTypeHandler;
        this.queryDomainType = queryDomainType;
        this.numberOfParameters = numberOfParameters;
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
        ResultSetInternalMethods execute(InterceptorImpl interceptor,
                ParameterBindings parameterBindings) throws SQLException;
    }

    /** This class implements the Executor contract but returns null, indicating that
     * the JDBC driver should implement the call itself.
     */
    public static class Noop implements Executor {

        public ResultSetInternalMethods execute(InterceptorImpl interceptor,
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

        public ResultSetInternalMethods execute(InterceptorImpl interceptor,
                ParameterBindings parameterBindings) throws SQLException {
            // create value handler to copy data from parameters to ndb
            // count the parameters
            int count = countParameters(parameterBindings);
            SessionSPI session = interceptor.getSession();
            Map<String, Object> parameters = createParameterMap(queryDomainType, parameterBindings, 0, count);
            QueryExecutionContext context = new QueryExecutionContextImpl(session, parameters);
            session.startAutoTransaction();
            try {
                ResultData resultData = queryDomainType.getResultData(context);
                // session.endAutoTransaction();
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

        public Delete (DomainTypeHandlerImpl<?> domainTypeHandler, QueryDomainTypeImpl<?> queryDomainType,
                int numberOfParameters) {
            super(domainTypeHandler, queryDomainType, numberOfParameters);
        }

        public Delete (DomainTypeHandlerImpl<?> domainTypeHandler) {
            super(domainTypeHandler);
        }

        public ResultSetInternalMethods execute(InterceptorImpl interceptor,
                ParameterBindings parameterBindings) throws SQLException {
            SessionSPI session = interceptor.getSession();
            if (queryDomainType == null) {
                int rowsDeleted = session.deletePersistentAll(domainTypeHandler);
                if (logger.isDebugEnabled()) logger.debug("deleteAll deleted: " + rowsDeleted);
                return new ResultSetInternalMethodsUpdateCount(rowsDeleted);
            } else {
                int numberOfBoundParameters = countParameters(parameterBindings);
                int numberOfStatements = numberOfBoundParameters / numberOfParameters;
                long[] deleteResults = new long[numberOfStatements];
                if (logger.isDebugEnabled()) logger.debug(
                        " numberOfParameters: " + numberOfParameters
                        + " numberOfBoundParameters: " + numberOfBoundParameters
                        + " numberOfStatements: " + numberOfStatements
                        );
                QueryExecutionContextJDBCImpl context = 
                    new QueryExecutionContextJDBCImpl(session, parameterBindings, numberOfParameters);
                for (int i = 0; i < numberOfStatements; ++i) {
                    // this will execute each statement in the batch using different parameters
                    int statementRowsDeleted = queryDomainType.deletePersistentAll(context);
                    if (logger.isDebugEnabled()) logger.debug("statement " + i
                            + " deleted " + statementRowsDeleted);
                    deleteResults[i] = statementRowsDeleted;
                    context.nextStatement();
                }
                return new ResultSetInternalMethodsUpdateCount(deleteResults);
            }
        }
    }

    /** This class implements the Executor contract for Insert operations.
     */
    public static class Insert extends SQLExecutor implements Executor {

        public Insert(DomainTypeHandlerImpl<?> domainTypeHandler, List<String> columnNames) {
            super(domainTypeHandler, columnNames, columnNames.size());
        }

        public ResultSetInternalMethods execute(InterceptorImpl interceptor,
                ParameterBindings parameterBindings) throws SQLException {
            SessionSPI session = interceptor.getSession();
            int numberOfBoundParameters = countParameters(parameterBindings);
            int numberOfStatements = numberOfBoundParameters / numberOfParameters;
            if (logger.isDebugEnabled()) logger.debug("SQLExecutor.Insert.execute"
                    + " numberOfParameters: " + numberOfParameters
                    + " numberOfBoundParameters: " + numberOfBoundParameters
                    + " numberOfFields: " + numberOfFields
                    + " numberOfStatements: " + numberOfStatements
                    );
            // interceptor.beforeClusterjStart();
            // session asks for values by field number which are converted to parameter number
            for (int offset = 0; offset < numberOfBoundParameters; offset += numberOfParameters) {
                ValueHandler valueHandler = getValueHandler(parameterBindings, fieldNumberToColumnNumberMap, offset);
                session.insert(domainTypeHandler, valueHandler);
            }
            session.flush();
            // interceptor.afterClusterjStart();
            return new ResultSetInternalMethodsUpdateCount(numberOfStatements);
        }
    }

    /** Create the parameter map assigning each bound parameter a number.
     * The result is a map in which the key is a String whose key is a cardinal number
     * starting with 1 (for JDBC which uses 1-origin for numbering)
     * and whose value is the parameter's value.
     * @param queryDomainType the query domain type
     * @param parameterBindings the parameter bindings
     * @param offset the number of parameters to skip
     * @param count the number of parameters to use
     * @return
     * @throws SQLException
     */
    protected Map<String, Object> createParameterMap(QueryDomainTypeImpl<?> queryDomainType,
            ParameterBindings parameterBindings, int offset, int count) throws SQLException {
        Map<String, Object> result = new HashMap<String, Object>();
        int first = offset + 1;
        int last = offset + count + 1;
        for (int i = first; i < last; ++i) {
            Object placeholder = parameterBindings.getObject(i);
            if (logger.isDetailEnabled())
                logger.detail("Put placeholder " + i + " value: " + placeholder + " of type " + placeholder.getClass());
            result.put(String.valueOf(i), placeholder);
        }
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
        numberOfFields = fieldNames.length;
        fieldNumberToColumnNumberMap = new int[numberOfFields];
        columnNumberToFieldNumberMap = new int[1 + columnNames.size()];
        for (int i= 0; i < numberOfFields; ++i) {
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
        if (logger.isDetailEnabled()) {
            StringBuilder buffer = new StringBuilder();
            for (int i = 0; i < fieldNumberToColumnNumberMap.length; ++i) {
                int columnNumber = fieldNumberToColumnNumberMap[i];
                buffer.append("field ");
                buffer.append(i);
                buffer.append(" mapped to ");
                buffer.append(columnNumber);
                buffer.append("[");
                buffer.append(columnNumber == -1?"nothing":(columnNames.get(columnNumber - 1)));
                buffer.append("];");
            }
            logger.detail(buffer.toString());
        }
    }

    /** Create a value handler (part of the clusterj spi) to retrieve values from jdbc parameter bindings.
     * @param parameterBindings the jdbc parameter bindings from prepared statements
     * @param fieldNumberToParameterNumberMap map from field number to parameter number
     * @param offset into the parameter bindings for this instance (used for batch execution)
     * @return
     */
    protected ValueHandler getValueHandler(ParameterBindings parameterBindings,
            int[] fieldNumberToParameterNumberMap, int offset) {
        return new ValueHandlerImpl(parameterBindings, fieldNumberToParameterNumberMap, offset);
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

    /** Count the number of bound parameters. If this is a batch execution, then the
     * number of bound parameters is the number of statements in the batch times the
     * number of parameters per statement.
     * If detailed logging is enabled write the parameter bindings to the log.
     * @param parameterBindings the jdbc parameter bindings
     */
    protected static int countParameters(ParameterBindings parameterBindings) {
        int i = 0;
        while (true) {
            try {
                ++i;
                // parameters are 1-origin per jdbc specification
                Object objectValue = parameterBindings.getObject(i);
                if (logger.isDetailEnabled()) {
                    logger.detail("parameterBinding: parameter " + i
                            + " has value: " + objectValue
                            + " of type " + objectValue.getClass());
                }
            } catch (Exception e) {
                // we don't know how many parameters are bound...
                break;
            }
        }
        return i - 1;
    }

}
