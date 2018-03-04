/*
   Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj;

/** Constants used in the ClusterJ project.
 * 
 */
public interface Constants {

	/** The name of the environment variable to set the logger factory */
	static final String ENV_CLUSTERJ_LOGGER_FACTORY_NAME = "CLUSTERJ_LOGGER_FACTORY";

	/** The name of the connection service property */
    static final String PROPERTY_CLUSTER_CONNECTION_SERVICE = "com.mysql.clusterj.connection.service";

    /** The name of the connection string property. For details, see
     * <a href="http://dev.mysql.com/doc/ndbapi/en/ndb-ndb-cluster-connection-methods.html#ndb-ndb-cluster-connection-constructor">Ndb_cluster_connection constructor</a>
     */
    static final String PROPERTY_CLUSTER_CONNECTSTRING = "com.mysql.clusterj.connectstring";

    /** The name of the initial timeout for cluster connection to connect to MGM before connecting to data nodes
     * <a href="http://dev.mysql.com/doc/ndbapi/en/ndb-ndb-cluster-connection-methods.html#ndb-ndb-cluster-connection-set-timeout">Ndb_cluster_connection::set_timeout()</a>
     */
    static final String PROPERTY_CLUSTER_CONNECT_TIMEOUT_MGM = "com.mysql.clusterj.connect.timeout.mgm";

    /** The default value of the connection timeout mgm property */
    static final int DEFAULT_PROPERTY_CLUSTER_CONNECT_TIMEOUT_MGM = 30000;

    /** The name of the connection retries property. For details, see
     * <a href="http://dev.mysql.com/doc/ndbapi/en/ndb-ndb-cluster-connection-methods.html#ndb-ndb-cluster-connection-connect">Ndb_cluster_connection::connect()</a>
     */
    static final String PROPERTY_CLUSTER_CONNECT_RETRIES = "com.mysql.clusterj.connect.retries";

    /** The default value of the connection retries property */
    static final int DEFAULT_PROPERTY_CLUSTER_CONNECT_RETRIES = 4;

    /** The name of the connection pool size property. This is the number of connections to create in the connection
     * pool. The default is 1 (all sessions share the same connection; all requests for a SessionFactory with the same
     * connect string and database will share a single SessionFactory). A setting of 0 disables pooling; each request
     * for a SessionFactory will receive its own unique SessionFactory.
     */
    static final String PROPERTY_CONNECTION_POOL_SIZE = "com.mysql.clusterj.connection.pool.size";

    /** The default value of the connection pool size property */
    static final int DEFAULT_PROPERTY_CONNECTION_POOL_SIZE = 1;

    /** The name of the connection pool node ids property. There is no default. This is the list of node ids to force
     * the connections to be assigned to specific node ids. If this property is specified and connection pool size
     * is not the default, the number of node ids of the list must match the connection pool size, or the number of
     * node ids must be 1 and node ids will be assigned to connections starting with the specified node id.
     */
    static final String PROPERTY_CONNECTION_POOL_NODEIDS = "com.mysql.clusterj.connection.pool.nodeids";

    /** The number of seconds to wait for all sessions to be closed when reconnecting a SessionFactory
     * due to network failures. The default, 0, indicates that the automatic reconnection to the cluster
     * due to network failures is disabled. Reconnection can be enabled by using the method
     * SessionFactory.reconnect(int timeout) and specifying a new timeout value.
     * @since 7.5.7
     */
    static final String PROPERTY_CONNECTION_RECONNECT_TIMEOUT = "com.mysql.clusterj.connection.reconnect.timeout";

    /** The default value of the connection reconnect timeout property. The default means that the
     * automatic reconnection due to network failures is disabled.
     * @since 7.5.7
     */
    static final int DEFAULT_PROPERTY_CONNECTION_RECONNECT_TIMEOUT = 0;

    /** The name of the connection delay property. For details, see
     * <a href="http://dev.mysql.com/doc/ndbapi/en/ndb-ndb-cluster-connection-methods.html#ndb-ndb-cluster-connection-connect">Ndb_cluster_connection::connect()</a>
     */
    static final String PROPERTY_CLUSTER_CONNECT_DELAY = "com.mysql.clusterj.connect.delay";

    /** The default value of the connection delay property */
    static final int DEFAULT_PROPERTY_CLUSTER_CONNECT_DELAY = 5;

    /** The name of the connection autoincrement batch size property.
     */
    static final String PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_BATCH_SIZE = "com.mysql.clusterj.connect.autoincrement.batchsize";

    /** The default value of the connection autoincrement batch size property */
    static final int DEFAULT_PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_BATCH_SIZE = 10;

    /** The name of the connection autoincrement step property.
     */
    static final String PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_STEP = "com.mysql.clusterj.connect.autoincrement.increment";

    /** The default value of the connection autoincrement step property */
    static final long DEFAULT_PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_STEP = 1;

    /** The name of the connection autoincrement start property.
     */
    static final String PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_START = "com.mysql.clusterj.connect.autoincrement.offset";

    /** The default value of the connection autoincrement start property */
    static final long DEFAULT_PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_START = 1;

    /** The name of the connection verbose property. For details, see
     * <a href="http://dev.mysql.com/doc/ndbapi/en/ndb-ndb-cluster-connection-methods.html#ndb-ndb-cluster-connection-connect">Ndb_cluster_connection::connect()</a>
     */
    static final String PROPERTY_CLUSTER_CONNECT_VERBOSE = "com.mysql.clusterj.connect.verbose";

    /** The default value of the connection verbose property */
    static final int DEFAULT_PROPERTY_CLUSTER_CONNECT_VERBOSE = 0;

    /** The name of the connection timeout before property. For details, see
     * <a href="http://dev.mysql.com/doc/ndbapi/en/ndb-ndb-cluster-connection-methods.html#ndb-ndb-cluster-connection-wait-until-ready">Ndb_cluster_connection::wait_until_ready()</a>
     */
    static final String PROPERTY_CLUSTER_CONNECT_TIMEOUT_BEFORE = "com.mysql.clusterj.connect.timeout.before";

    /** The default value of the connection timeout before property */
    static final int DEFAULT_PROPERTY_CLUSTER_CONNECT_TIMEOUT_BEFORE = 30;

    /** The name of the connection timeout after property. For details, see
     * <a href="http://dev.mysql.com/doc/ndbapi/en/ndb-ndb-cluster-connection-methods.html#ndb-ndb-cluster-connection-wait-until-ready">Ndb_cluster_connection::wait_until_ready()</a>
     */
    static final String PROPERTY_CLUSTER_CONNECT_TIMEOUT_AFTER = "com.mysql.clusterj.connect.timeout.after";

    /** The default value of the connection timeout after property */
    static final int DEFAULT_PROPERTY_CLUSTER_CONNECT_TIMEOUT_AFTER = 20;

    /** The cpu binding of the receive threads for the connections in the
     * connection pool. The default is no cpu binding for receive threads.
     * If this property is specified and connection pool size is not the
     * default (1), the number of cpuids of the list must match the
     * connection pool size.
     */
    static final String PROPERTY_CONNECTION_POOL_RECV_THREAD_CPUIDS = "com.mysql.clusterj.connection.pool.recv.thread.cpuids";

    /** The receive thread activation threshold for all connections in the
     * connection pool. The default is no activation threshold.
     */
    static final String PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD = "com.mysql.clusterj.connection.pool.recv.thread.activation.threshold";

    /** The default value of the receive thread activation threshold */
    static final int DEFAULT_PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD = 8;

    /** The name of the database property. For details, see the catalogName parameter in the Ndb constructor.
     * <a href="http://dev.mysql.com/doc/ndbapi/en/ndb-ndb-methods.html#ndb-ndb-constructor">Ndb constructor</a>
     */
    static final String PROPERTY_CLUSTER_DATABASE = "com.mysql.clusterj.database";

    /** The default value of the database property */
    static final String DEFAULT_PROPERTY_CLUSTER_DATABASE = "test";

    /** The name of the maximum number of transactions property. For details, see
     * <a href="http://dev.mysql.com/doc/ndbapi/en/ndb-ndb-methods.html#ndb-ndb-init">Ndb::init()</a>
     */
    static final String PROPERTY_CLUSTER_MAX_TRANSACTIONS = "com.mysql.clusterj.max.transactions";

    /** The name of the byte buffer pool sizes property. To disable buffer pooling for blob objects,
     * set the value of this property to "1". With this setting, buffers will be allocated and freed
     * (and cleaned if possible) immediately after being used for blob data transfer. */
    static final String PROPERTY_CLUSTER_BYTE_BUFFER_POOL_SIZES = "com.mysql.clusterj.byte.buffer.pool.sizes";

    /** The default value of the byte buffer pool sizes property: 256, 10K, 100K, 1M*/
    static final String DEFAULT_PROPERTY_CLUSTER_BYTE_BUFFER_POOL_SIZES = "256, 10240, 102400, 1048576";

    /** The default value of the maximum number of transactions property */
    static final int DEFAULT_PROPERTY_CLUSTER_MAX_TRANSACTIONS = 4;

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
