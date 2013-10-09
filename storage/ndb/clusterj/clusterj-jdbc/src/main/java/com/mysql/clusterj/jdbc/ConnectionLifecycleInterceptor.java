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

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.jdbc.Connection;

import java.sql.SQLException;
import java.sql.Savepoint;

import java.util.Properties;

/** This interceptor is a plugin for the MySQL JDBC driver, Connector-J.
 * It is called for each change of state of the associated connection.
 * It is registered with the connection via the connection URL parameter
 * connectionLifecycleInterceptors=com.mysql.clusterj.jdbc.ConnectionLifecycleInterceptor.
 * It must be used in conjunction with the StatementInterceptor.
 */
public class ConnectionLifecycleInterceptor
            implements com.mysql.jdbc.ConnectionLifecycleInterceptor {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ConnectionLifecycleInterceptor.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(ConnectionLifecycleInterceptor.class);

    /** The delegate for all methods. */
    private InterceptorImpl interceptorImpl;

    /** This method is called during connection setup with a new instance of ConnectionLifecycleInterceptor.
     * An instance of InterceptorImpl is either created or found by lookup.
     * Before the interceptor is useable, both the connection lifecycle interceptor and
     * the statement interceptor must be registered.
     * @param conn the associated connection
     * @param properties the properties used to create the connection
     */
    public void init(Connection conn, Properties properties) throws SQLException {
        // find the interceptor if it's already registered; otherwise create it
        this.interceptorImpl = InterceptorImpl.getInterceptorImpl(this, conn, properties);
    }

    public void destroy() {
        interceptorImpl.destroy(this);
        interceptorImpl = null;
    }

    public void close() throws SQLException {
        interceptorImpl.close();
    }

    public boolean commit() throws SQLException {
        return interceptorImpl.commit();
    }

    public boolean rollback() throws SQLException {
        return interceptorImpl.rollback();
    }

    public boolean rollback(Savepoint savepoint) throws SQLException {
        return interceptorImpl.rollback(savepoint);
    }

    public boolean setAutoCommit(boolean autocommit) throws SQLException {
        return interceptorImpl.setAutoCommit(autocommit);
    }

    public boolean setCatalog(String catalog) throws SQLException {
        return interceptorImpl.setCatalog(catalog);
    }

    public boolean transactionBegun() throws SQLException {
        return interceptorImpl.transactionBegun();
    }

    public boolean transactionCompleted() throws SQLException {
        return interceptorImpl.transactionCompleted();
    }

}
