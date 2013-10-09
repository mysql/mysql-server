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

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.core.query.QueryDomainTypeImpl;
import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.jdbc.Connection;
import com.mysql.jdbc.ResultSetInternalMethods;
import com.mysql.jdbc.Statement;
import com.mysql.clusterj.jdbc.antlr.ANTLRNoCaseStringStream;
import com.mysql.clusterj.jdbc.antlr.MySQL51Parser;
import com.mysql.clusterj.jdbc.antlr.MySQL51Lexer;
import com.mysql.clusterj.jdbc.antlr.QueuingErrorListener;
import com.mysql.clusterj.jdbc.antlr.node.Node;
import com.mysql.clusterj.jdbc.antlr.node.PlaceholderNode;
import com.mysql.clusterj.jdbc.antlr.node.SelectNode;
import com.mysql.clusterj.jdbc.antlr.node.WhereNode;
import com.mysql.clusterj.query.Predicate;

import com.mysql.clusterj.jdbc.SQLExecutor.Executor;
import java.sql.SQLException;
import java.sql.Savepoint;
import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;

import org.antlr.runtime.CommonTokenStream;
import org.antlr.runtime.RecognitionException;
import org.antlr.runtime.Token;
import org.antlr.runtime.TokenStream;
import org.antlr.runtime.tree.CommonErrorNode;
import org.antlr.runtime.tree.CommonTree;
import org.antlr.runtime.tree.CommonTreeAdaptor;
import org.antlr.runtime.tree.TreeAdaptor;

/** This class implements the behavior associated with connection callbacks for statement execution
 * and connection lifecycle. There is a clusterj session associated with the interceptor that
 * is used to interact with the cluster. There is exactly one statement interceptor and one
 * connection lifecycle interceptor associated with the interceptor.
 * All of the SQL post-parsing behavior is contained here, and uses classes in the org.antlr.runtime and
 * com.mysql.clusterj.jdbc.antlr packages to perform the parsing of the SQL statement. Analysis
 * of the parsed SQL statement occurs here, and clusterj artifacts are constructed for use in
 * other classes, in particular SQLExecutor and its command-specific subclasses.
 */
public class InterceptorImpl {

    /** Register logger for JDBC stuff. */
    static {
        LoggerFactoryService.getFactory().registerLogger("com.mysql.clusterj.jdbc");
    }

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(InterceptorImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(InterceptorImpl.class);

    static Map<String, Executor> parsedSqlMap = new IdentityHashMap<String, Executor>();

    /** The map of connection to interceptor */
    private static Map<Connection, InterceptorImpl> interceptorImplMap =
            new IdentityHashMap<Connection, InterceptorImpl>();

    /** The connection properties */
    private Properties properties;

    /** The connection being intercepted */
    private Connection connection;

    /** The session factory for this connection */
    SessionFactory sessionFactory;

    /** The current session (null if no session) */
    private SessionSPI session;

    /** The statement interceptor (only used during initialization) */
    private StatementInterceptor statementInterceptor;

    /** The connection lifecycle interceptor (only used during initialization) */
    private ConnectionLifecycleInterceptor connectionLifecycleInterceptor;

    /** The interceptor is ready (both interceptors are registered) */
    private boolean ready = false;

    private boolean autocommit;

    private static String LOTSOBLANKS = "                                                                          "; 

    /** Create the interceptor.
     * 
     * @param connection the connection being intercepted
     * @param properties the connection properties
     */
    public InterceptorImpl(Connection connection, Properties properties) {
        if (logger.isDebugEnabled()) logger.debug("constructed with properties: " + properties);
        this.properties = properties;
        this.connection = connection;
        // if database name is not specified, translate DBNAME to the required clusterj property
        String dbname = properties.getProperty("com.mysql.clusterj.database",
                properties.getProperty("DBNAME"));
        properties.put("com.mysql.clusterj.database", dbname);
    }

    /** Return the interceptor for the connection lifecycle callbacks.
     * 
     * @param connectionLifecycleInterceptor the connection lifecycle interceptor
     * @param connection the connection
     * @param properties the connection properties
     * @return the interceptor delegate
     */
    public static InterceptorImpl getInterceptorImpl(
            ConnectionLifecycleInterceptor connectionLifecycleInterceptor,
            Connection connection, Properties properties) {
        InterceptorImpl result = getInterceptorImpl(connection, properties);
        if (result.connectionLifecycleInterceptor != null) {
            if (result.connectionLifecycleInterceptor != connectionLifecycleInterceptor) {
                throw new ClusterJUserException(
                        local.message("ERR_Duplicate_Connection_Lifecycle_Interceptor"));
            }
        } else {
            result.connectionLifecycleInterceptor = connectionLifecycleInterceptor;
        }
        if (result.statementInterceptor != null) {
            result.ready = true;
        }
        return result;
    }

    /** Return the interceptor for the statement interceptor callbacks.
     * 
     * @param statementInterceptor the statement interceptor
     * @param connection the connection
     * @param properties the connection properties
     * @return the interceptor delegate
     */
    public static InterceptorImpl getInterceptorImpl(
            StatementInterceptor statementInterceptor, Connection connection,
            Properties properties) {
        InterceptorImpl result = getInterceptorImpl(connection, properties);
        if (result.statementInterceptor != null) {
            throw new ClusterJUserException(
                    local.message("ERR_Duplicate_Statement_Interceptor"));
        }
        result.statementInterceptor = statementInterceptor;
        if (result.connectionLifecycleInterceptor != null) {
            result.ready = true;
        }
        return result;
    }

    /** Create the interceptor to handle both connection lifecycle and statement interceptors.
     * 
     * @param connection the connection
     * @param properties the connection properties
     * @return
     */
    public static InterceptorImpl getInterceptorImpl(Connection connection, Properties properties) {
        InterceptorImpl result;
        synchronized(interceptorImplMap) {
            result = interceptorImplMap.get(connection);
            if (result == null) {
                result = new InterceptorImpl(connection, properties);
                interceptorImplMap.put(connection, result);
            }
        }
        return result;
    }

    /** Return the interceptor assigned to the connection. If there is no interceptor, return null.
     * 
     * @param connection the connection
     * @return the interceptor for this connection or null if there is no interceptor
     */
    public static InterceptorImpl getInterceptorImpl(java.sql.Connection connection) {
        synchronized (interceptorImplMap) {
            return interceptorImplMap.get(connection);
        }
    }

    @Override
    public String toString() {
        return "InterceptorImpl "
//        + " properties: "+ properties.toString()
        ;
    }

    void destroy() {
        if (sessionFactory != null) {
            if (session != null) {
                session.close();
            }
            sessionFactory.close();
            sessionFactory = null;
            synchronized(interceptorImplMap) {
                interceptorImplMap.remove(connection);
            }
        }
    }

    public SessionSPI getSession() {
        if (session == null) {
            session = (SessionSPI)sessionFactory.getSession();
        }
        return session;
    }

    public boolean executeTopLevelOnly() {
//        assertReady();
        boolean result = true;
        return result;
    }

    public ResultSetInternalMethods postProcess(String sql, Statement statement,
            ResultSetInternalMethods result, Connection connection, int arg4,
            boolean arg5, boolean arg6, SQLException sqlException) throws SQLException {
        assertReady();
        return null;
    }

    public ResultSetInternalMethods preProcess(String sql, Statement statement,
            Connection connection) throws SQLException {
        assertReady();
        if (statement instanceof com.mysql.jdbc.PreparedStatement) {
            com.mysql.jdbc.PreparedStatement preparedStatement =
                (com.mysql.jdbc.PreparedStatement)statement;
            // key must be interned because we are using IdentityHashMap
            String preparedSql = preparedStatement.getPreparedSql().intern();
            // see if we have a parsed version of this query
            Executor sQLExecutor = null;
            synchronized(parsedSqlMap) {
                sQLExecutor = parsedSqlMap.get(preparedSql);
            }
            // if no cached SQLExecutor, create it, which might take some time
            if (sQLExecutor == null) {
                sQLExecutor = createSQLExecutor(preparedSql);
                if (sQLExecutor != null) {
                    // multiple thread might have created a SQLExecutor but it's ok
                    synchronized(parsedSqlMap) {
                        parsedSqlMap.put(preparedSql, sQLExecutor);
                    }
                }
            }
            return sQLExecutor.execute(this, preparedStatement.getParameterBindings());
        }
        return null;
    }

    /**
     * @param preparedSql
     */
    private Executor createSQLExecutor(String preparedSql) {
        if (logger.isDetailEnabled()) logger.detail(preparedSql);
        Executor result = null;
        // parse the sql
        CommonTree root = parse(preparedSql);
        // get the root of the tree
        int tokenType = root.getType();
        // perform command-specific actions
        String tableName = "";
        CommonTree tableNode;
        WhereNode whereNode;
        List<String> columnNames = new ArrayList<String>();
        Dictionary dictionary;
        DomainTypeHandlerImpl<?> domainTypeHandler;
        QueryDomainTypeImpl<?> queryDomainType = null;
        switch (tokenType) {
            case MySQL51Parser.INSERT:
                tableNode = (CommonTree)root.getFirstChildWithType(MySQL51Parser.TABLE);
                tableName = getTableName(tableNode);
                getSession();
                dictionary = session.getDictionary();
                domainTypeHandler = getDomainTypeHandler(tableName, dictionary);
                CommonTree insertValuesNode = (CommonTree)root.getFirstChildWithType(MySQL51Parser.INSERT_VALUES);
                CommonTree columnsNode = (CommonTree)insertValuesNode.getFirstChildWithType(MySQL51Parser.COLUMNS);
                List<CommonTree> fields = columnsNode.getChildren();
                for (CommonTree field: fields) {
                    columnNames.add(getColumnName(field));
                }
                if (logger.isDetailEnabled()) logger.detail(
                        "StatementInterceptorImpl.preProcess parse result INSERT INTO " + tableName
                        + " COLUMNS " + columnNames);
                result = new SQLExecutor.Insert(domainTypeHandler, columnNames);
                break;
            case MySQL51Parser.SELECT:
                CommonTree fromNode = (CommonTree)root.getFirstChildWithType(MySQL51Parser.FROM);
                if (fromNode == null) {
                    // no from clause; cannot handle this case so return a do-nothing ParsedSQL
                    result = new SQLExecutor.Noop();
                    break;
                }
                try {
                    // this currently handles only FROM clauses with a single table
                    tableNode = (CommonTree) fromNode.getFirstChildWithType(MySQL51Parser.TABLE);
                    tableName = getTableName(tableNode);
                } catch (Exception e) {
                    // trouble with the FROM clause; log the SQL statement and the parser output
                    logger.info("Problem with FROM clause in SQL statement: " + preparedSql);
                    logger.info(walk(root));
                    result = new SQLExecutor.Noop();
                    break;
                }
                getSession();
                dictionary = session.getDictionary();
                domainTypeHandler = getDomainTypeHandler(tableName, dictionary);
                columnsNode = (CommonTree)root.getFirstChildWithType(MySQL51Parser.COLUMNS);
                List<CommonTree> selectExprNodes = columnsNode.getChildren();
                for (CommonTree selectExprNode: selectExprNodes) {
                    columnNames.add(getColumnName(getFieldNode(selectExprNode)));
                }
                String whereType = "empty";
                if (logger.isDetailEnabled()) logger.detail(
                        "SELECT FROM " + tableName
                        + " COLUMNS " + columnNames);
                // we need to distinguish three cases:
                // - no where clause (select all rows)
                // - where clause that cannot be executed by clusterj
                // - where clause that can be executed by clusterj
                whereNode = ((SelectNode)root).getWhereNode();
                queryDomainType = (QueryDomainTypeImpl<?>) session.createQueryDomainType(domainTypeHandler);
                if (whereNode == null) {
                    // no where clause (select all rows)
                    result = new SQLExecutor.Select(domainTypeHandler, columnNames, queryDomainType);
                } else {
                    // create a predicate from the tree
                    Predicate predicate = whereNode.getPredicate(queryDomainType);
                    if (predicate != null) {
                        // where clause that can be executed by clusterj
                        queryDomainType.where(predicate);
                        result = new SQLExecutor.Select(domainTypeHandler, columnNames, queryDomainType);
                        whereType = "clusterj";
                    } else {
                        // where clause that cannot be executed by clusterj
                        result = new SQLExecutor.Noop();
                        whereType = "non-clusterj";
                    }
                    if (logger.isDetailEnabled()) logger.detail(walk(root));
                }
                if (logger.isDetailEnabled()) {
                    logger.detail(
                        "SELECT FROM " + tableName
                        + " COLUMNS " + columnNames + " whereType " + whereType);
                    logger.detail(walk(root));
                }
                break;
            case MySQL51Parser.DELETE:
                tableNode = (CommonTree)root.getFirstChildWithType(MySQL51Parser.TABLE);
                tableName = getTableName(tableNode);
                getSession();
                dictionary = session.getDictionary();
                domainTypeHandler = getDomainTypeHandler(tableName, dictionary);
                whereNode = ((WhereNode)root.getFirstChildWithType(MySQL51Parser.WHERE));
                int numberOfParameters = 0;
                if (whereNode == null) {
                    // no where clause (delete all rows)
                    result = new SQLExecutor.Delete(domainTypeHandler);
                    whereType = "empty";
                } else {
                    // create a predicate from the tree
                    queryDomainType = (QueryDomainTypeImpl<?>) session.createQueryDomainType(domainTypeHandler);
                    Predicate predicate = whereNode.getPredicate(queryDomainType);
                    if (predicate != null) {
                        // where clause that can be executed by clusterj
                        queryDomainType.where(predicate);
                        numberOfParameters = whereNode.getNumberOfParameters();
                        result = new SQLExecutor.Delete(domainTypeHandler, queryDomainType, numberOfParameters);
                        whereType = "clusterj";
                    } else {
                        // where clause that cannot be executed by clusterj
                        result = new SQLExecutor.Noop();
                        whereType = "non-clusterj";
                    }
                    if (logger.isDetailEnabled()) logger.detail(walk(root));
                }
                if (logger.isDetailEnabled()) logger.detail(
                        "DELETE FROM " + tableName
                        + " whereType " + whereType
                        + " number of parameters " + numberOfParameters);
                break;
            default:
                // return a do-nothing ParsedSQL
                if (logger.isDetailEnabled()) logger.detail("ClusterJ cannot process this SQL statement: unsupported statement type.");
                result = new SQLExecutor.Noop();
        }
        return result;
    }

    private String getPrimaryKeyFieldName(CommonTree whereNode) {
        String result = null;
        CommonTree operation = (CommonTree) whereNode.getChild(0);
        if (MySQL51Parser.EQUALS == operation.getType()) {
            result = operation.getChild(0).getChild(0).getText();
        } else {
            throw new ClusterJUserException("Cannot find primary key in WHERE clause.");
        }
        return result;
    }

    private String walk(CommonTree tree) {
        StringBuilder buffer = new StringBuilder();
        walk(tree, buffer, 0);
        return buffer.toString();
    }

    @SuppressWarnings("unchecked") // tree.getChildren()
    private void walk(CommonTree tree, StringBuilder buffer, int level) {
            String indent = LOTSOBLANKS.substring(0, level);
            Token token = tree.token;
            int tokenType = token.getType();
            String tokenText = token.getText();
            int childCount = tree.getChildCount();
            int childIndex = tree.getChildIndex();
            buffer.append('\n');
            buffer.append(indent);
            buffer.append(tokenText);
            buffer.append(" class: ");
            buffer.append(tree.getClass().getName());
            buffer.append(" tokenType ");
            buffer.append(tokenType);
            buffer.append(" child count ");
            buffer.append(childCount);
            buffer.append(" child index ");
            buffer.append(childIndex);
            List<CommonTree> children = tree.getChildren();
            if (children == null) {
                return;
            }
            for (CommonTree child: children) {
                walk(child, buffer, level + 2);
            }
    }

    private CommonTree parse(String preparedSql) {
        CommonTree result = null;
        ANTLRNoCaseStringStream inputStream = new ANTLRNoCaseStringStream(preparedSql);
        MySQL51Lexer lexer = new MySQL51Lexer(inputStream);
        CommonTokenStream tokens = new CommonTokenStream(lexer);
        lexer.setErrorListener(new QueuingErrorListener(lexer));
        tokens.getTokens();
        if (lexer.getErrorListener().hasErrors()) {
            logger.warn(local.message("ERR_Lexing_SQ",preparedSql));
            return result;
        }
        PlaceholderNode.resetId();
        MySQL51Parser parser = new MySQL51Parser(tokens);
        parser.setTreeAdaptor(mySQLTreeAdaptor);
        parser.setErrorListener(new QueuingErrorListener(parser));
        try {
            CommonTree stmtTree = (CommonTree) parser.statement().getTree();
            result = stmtTree;
        } catch (RecognitionException e) {
            logger.warn(local.message("ERR_Parsing_SQL", preparedSql));
        }
        if (parser.getErrorListener().hasErrors()) {
            logger.warn(local.message("ERR_Parsing_SQL", preparedSql));
        }
        return result;
    }

    private TreeAdaptor mySQLTreeAdaptor = new CommonTreeAdaptor() {
        public Object create(Token token) { return new Node(token); }
        public Object dupNode(Object t) {
            if ( t==null ) return null;
            return create(((Node)t).token);
        }
    };

    private String getTableName(CommonTree tableNode) {
        return tableNode.getChild(0).getText();
    }

    private String getColumnName(CommonTree fieldNode) {
        return fieldNode.getChild(0).getText();
    }

    private CommonTree getFieldNode(CommonTree selectExprNode) {
        return (CommonTree)selectExprNode.getChild(0);
    }

    public void destroy(StatementInterceptor statementInterceptor) {
    }

    public void destroy(
            ConnectionLifecycleInterceptor connectionLifecycleInterceptor) {
    }

    private void assertReady() {
        if (!ready) {
            if (statementInterceptor == null) {
                throw new ClusterJUserException(local.message("ERR_No_Statement_Interceptor"));
            }
            if (connectionLifecycleInterceptor == null) {
                throw new ClusterJUserException(local.message("ERR_No_Connection_Lifecycle_Interceptor"));
            }
        } else {
            if (sessionFactory == null) {
                sessionFactory = ClusterJHelper.getSessionFactory(properties);
            }
        }
    }

    /** TODO This needs to be rewritten with a proper state machine. */
    public boolean setAutoCommit(boolean autocommit) throws SQLException {
        assertReady();
        logStatus("setAutoCommit(" + autocommit + ")");
        this.autocommit = autocommit;
        getSession();
        if (!autocommit) {
            // start a transaction
            if (!session.currentTransaction().isActive()) {
                session.begin();
            }
        } else {
            // roll back the previous transaction if active
            if (session.currentTransaction().isActive()) {
                session.rollback();
            }
        }
        return true; // let the driver perform its own autocommit behavior
    }

    public void close() {
    }

    public boolean commit() throws SQLException {
        logStatus("commit");
        if (session.currentTransaction().isActive()) {
            session.commit();
        } else {
            System.out.println("WARNING: commit called when session.transaction is not active");
        }
        session.begin();
        return true;
    }

    public boolean rollback() throws SQLException {
        logStatus("rollback");
        session.rollback();
        session.begin();
        return true;
    }

    public boolean rollback(Savepoint savepoint) throws SQLException {
        logStatus("rollback(Savepoint)");
        return true;
    }

    public boolean setCatalog(String catalog) throws SQLException {
        if (logger.isDebugEnabled()) logger.debug("catalog: " + catalog);
        return true;
    }

    public boolean transactionCompleted() throws SQLException {
        logStatus("transactionCompleted");
        return true;
    }

    public boolean transactionBegun() throws SQLException {
        logStatus("transactionBegun");
        return true;
    }

    private DomainTypeHandlerImpl<?> getDomainTypeHandler(String tableName, Dictionary dictionary) {
        DomainTypeHandlerImpl<?> domainTypeHandler = 
            DomainTypeHandlerImpl.getDomainTypeHandler(tableName, dictionary);
        return domainTypeHandler;
    }

    private void logStatus(String s) throws SQLException {
        if (logger.isDetailEnabled()) {
            StringBuilder builder = new StringBuilder("In ");
            builder.append(s);
            builder.append(" with");
            if (connection != null) {
                builder.append(" connection.getAutocommit: " + connection.getAutoCommit());
            }
            if (session != null) {
                builder.append(" session.isActive: " + session.currentTransaction().isActive());
            }
            builder.append('\n');
            String message = builder.toString();
            logger.detail(message);
        }
    }

}
