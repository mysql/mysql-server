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

package jdbctest;

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.jdbc.Connection;

import java.sql.SQLException;
import java.sql.Savepoint;
import java.util.Properties;

public class BadConnectionLifecycleInterceptor
        implements com.mysql.jdbc.ConnectionLifecycleInterceptor {

    public void close() throws SQLException {
     // TODO Auto-generated method stub

     }

     public boolean commit() throws SQLException {
     // TODO Auto-generated method stub
     return false;
     }

     public boolean rollback() throws SQLException {
     // TODO Auto-generated method stub
     return false;
     }

     public boolean rollback(Savepoint arg0) throws SQLException {
     // TODO Auto-generated method stub
     return false;
     }

     public boolean setAutoCommit(boolean arg0) throws SQLException {
     // TODO Auto-generated method stub
     return false;
     }

     public boolean setCatalog(String arg0) throws SQLException {
     // TODO Auto-generated method stub
     return false;
     }

     public boolean transactionBegun() throws SQLException {
     // TODO Auto-generated method stub
     return false;
     }

     public boolean transactionCompleted() throws SQLException {
     // TODO Auto-generated method stub
     return false;
     }

     public void destroy() {
     // TODO Auto-generated method stub

     }

     public void init(Connection arg0, Properties arg1) throws SQLException {
     throw new ClusterJUserException("BadConnectionLifecycleInterceptor is built to fail.");
     }

}

