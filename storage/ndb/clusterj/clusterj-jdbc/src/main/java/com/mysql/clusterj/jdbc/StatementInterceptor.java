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

import com.mysql.jdbc.Connection;
import com.mysql.jdbc.ResultSetInternalMethods;
import com.mysql.jdbc.Statement;

import java.sql.SQLException;
import java.util.Properties;

/** This interceptor is called for execution of each statement of the associated connection.
 * It is registered with the connection via the connection URL parameter
 * statementInterceptors=com.mysql.clusterj.jdbc.StatementInterceptor.
 * It must be used in conjunction with the ConnectionLifecycleInterceptor.
 */
public class StatementInterceptor
        implements com.mysql.jdbc.StatementInterceptorV2 {

    /** The delegate for all methods. */
    private InterceptorImpl interceptorImpl;

    public void init(Connection connection, Properties properties) throws SQLException {
        // find the interceptor if it's already registered; otherwise create it
        this.interceptorImpl = InterceptorImpl.getInterceptorImpl(this, connection, properties);
    }

    public void destroy() {
        interceptorImpl.destroy(this);
        interceptorImpl = null;
    }

    public boolean executeTopLevelOnly() {
        return interceptorImpl.executeTopLevelOnly();
    }

    public ResultSetInternalMethods postProcess(String sql, Statement statement,
            ResultSetInternalMethods result, Connection connection, int arg4,
            boolean arg5, boolean arg6, SQLException sqlException) throws SQLException {
        return interceptorImpl.postProcess(sql, statement,
                result, connection, arg4,
                arg5, arg6, sqlException);
    }

    public ResultSetInternalMethods preProcess(String sql, Statement statement,
            Connection connection) throws SQLException {
        return interceptorImpl.preProcess(sql, statement, connection);
    }

}
