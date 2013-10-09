/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.openjpa;

import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.core.store.Dictionary;

import org.apache.openjpa.jdbc.conf.JDBCConfiguration;
import org.apache.openjpa.jdbc.meta.ClassMapping;

/**
 * Configuration that defines the properties necessary to configure
 * runtime and connect to an Ndb Cluster.
 *
 */
public interface NdbOpenJPAConfiguration
    extends JDBCConfiguration {

    /**
     * Name of the logger for SQL execution messages:
     * <code>openjpa.jdbc.SQL</code>.
     */
    public static final String LOG_NDB_SQL = "openjpa.ndb.SQL";

    /**
     * Name of the logger for JDBC-related messages:
     * <code>openjpa.jdbc.JDBC</code>.
     */
    public static final String LOG_NDB_JDBC = "openjpa.ndb.JDBC";

//com.mysql.clusterj.connectstring=localhost:9311
//com.mysql.clusterj.connect.retries=4
//com.mysql.clusterj.connect.delay=5
//com.mysql.clusterj.connect.verbose=1
//com.mysql.clusterj.connect.timeout.before=30
//com.mysql.clusterj.connect.timeout.after=20
//com.mysql.clusterj.username=
//com.mysql.clusterj.password=
//com.mysql.clusterj.database=test
//com.mysql.clusterj.max.transactions=1024

    public String getConnectString();
    public void setConnectString(String value);

    public int getConnectRetries();
    public void setConnectRetries(int value);

    public int getConnectDelay();
    public void setConnectDelay(int value);

    public int getConnectVerbose();
    public void setConnectVerbose(int value);

    public int getConnectTimeoutBefore();
    public void setConnectTimeoutBefore(int value);

    public int getConnectTimeoutAfter();
    public void setConnectTimeoutAfter(int value);

    public String getUsername();
    public void setUsername(String value);

    public String getPassword();
    public void setPassword(String value);

    public String getDatabase();
    public void setDatabase(String value);

    public int getMaxTransactions();
    public void setMaxTransactions(int value);

    public SessionFactory getSessionFactory();
    public void setSessionFactory(SessionFactory value);

    public boolean getFailOnJDBCPath();
    public void setFailOnJDBCPath(boolean value);

    public NdbOpenJPADomainTypeHandlerImpl<?> getDomainTypeHandler(ClassMapping cmd, Dictionary dictionary);

}
