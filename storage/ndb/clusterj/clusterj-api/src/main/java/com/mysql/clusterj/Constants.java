/*
   Copyright (C) 2009 Sun Microsystems Inc.
   All rights reserved. Use is subject to license terms.

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

package com.mysql.clusterj;

/** Constants used in the ClusterJ project.
 * 
 */
public interface Constants {

    /** The name of the connection service property */
    static final String PROPERTY_CLUSTER_CONNECTION_SERVICE = "com.mysql.clusterj.connection.service";

    /** The name of the connection string property */
    static final String PROPERTY_CLUSTER_CONNECTSTRING = "com.mysql.clusterj.connectstring";

    /** The name of the connection retries property */
    static final String PROPERTY_CLUSTER_CONNECT_RETRIES = "com.mysql.clusterj.connect.retries";

    /** The name of the connection delay property */
    static final String PROPERTY_CLUSTER_CONNECT_DELAY = "com.mysql.clusterj.connect.delay";

    /** The name of the connection verbose property */
    static final String PROPERTY_CLUSTER_CONNECT_VERBOSE = "com.mysql.clusterj.connect.verbose";

    /** The name of the connection timeout before property */
    static final String PROPERTY_CLUSTER_CONNECT_TIMEOUT_BEFORE = "com.mysql.clusterj.connect.timeout.before";

    /** The name of the connection timeout after property */
    static final String PROPERTY_CLUSTER_CONNECT_TIMEOUT_AFTER = "com.mysql.clusterj.connect.timeout.after";

    /** The name of the database property */
    static final String PROPERTY_CLUSTER_DATABASE = "com.mysql.clusterj.database";

    /** The name of the maximum number of transactions property */
    static final String PROPERTY_CLUSTER_MAX_TRANSACTIONS = "com.mysql.clusterj.max.transactions";

    /** The flag for deferred inserts, deletes, and updates */
    static final String PROPERTY_DEFER_CHANGES = "com.mysql.clusterj.defer.changes";

    /** The name of the session factory service interface */
    static final String SESSION_FACTORY_SERVICE_CLASS_NAME = 
            "com.mysql.clusterj.SessionFactoryService";

    /** The name of the files with names of implementation classes for session factory service */
    static final String SESSION_FACTORY_SERVICE_FILE_NAME = 
            "META-INF/services/" + SESSION_FACTORY_SERVICE_CLASS_NAME;

    /** The name of the jdbc driver */
    static final String PROPERTY_JDBC_DRIVER_NAME = 
            "com.mysql.clusterj.jdbc.driver";

    /** The jdbc url */
    static final String PROPERTY_JDBC_URL = 
            "com.mysql.clusterj.jdbc.url";

    /** The jdbc username */
    static final String PROPERTY_JDBC_USERNAME = 
            "com.mysql.clusterj.jdbc.username";

    /** The jdbc password */
    static final String PROPERTY_JDBC_PASSWORD = 
            "com.mysql.clusterj.jdbc.password";
}
