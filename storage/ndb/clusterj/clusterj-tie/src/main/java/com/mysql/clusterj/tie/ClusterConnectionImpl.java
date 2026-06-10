/*
 *  Copyright (c) 2010, 2026, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.tie;

import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.NdbConst;
import com.mysql.ndbjtie.ndbapi.Ndb_cluster_connection;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.ClusterJUserException;

import com.mysql.clusterj.core.spi.ValueHandlerFactory;
import com.mysql.clusterj.core.store.DbFactory;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 *
 */
public class ClusterConnectionImpl
        implements com.mysql.clusterj.core.store.ClusterConnection {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ClusterConnectionImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(ClusterConnectionImpl.class);

    /** Ndb_cluster_connection is wrapped by ClusterConnection */
    protected Ndb_cluster_connection clusterConnection;

    /** The connection string for this connection */
    final String connectString;

    /** The node id requested for this connection; 0 for default */
    int nodeId;

    /** The timeout value to connect to mgm */
    final int connectTimeoutMgm;

    /** A "big enough" size for error information */
    private int errorBufferSize = 300;

    /** The byte buffer pool for DbImpl error buffers */
    protected FixedByteBufferPoolImpl byteBufferPoolForDBImplError;

    /** The size of the partition key scratch buffer */
    private final static int PARTITION_KEY_BUFFER_SIZE = 10000;

    /** The byte buffer pool for DbImpl error buffers */
    FixedByteBufferPoolImpl byteBufferPoolForPartitionKey;

    /** Bound CPU thread ID for receive thread. -1 means not bound to a CPU. */
    private short recvThdCpu = -1;

    private boolean isClosing = false;

    private long[] autoIncrement;

    private static final String USE_SMART_VALUE_HANDLER_NAME = "com.mysql.clusterj.UseSmartValueHandler";

    private static final boolean USE_SMART_VALUE_HANDLER =
            ClusterJHelper.getBooleanProperty(USE_SMART_VALUE_HANDLER_NAME, "true");

    static boolean queryObjectsInitialized = false;

    /** Connect to the MySQL Cluster
     * 
     * @param connectString the connect string
     * @param nodeId the node id; node id of zero means "any node"
     */
    public ClusterConnectionImpl(String connectString, int nodeId, int connectTimeoutMgm) {
        this.connectString = connectString;
        this.nodeId = nodeId;
        this.connectTimeoutMgm = connectTimeoutMgm;
        byteBufferPoolForDBImplError =
                new FixedByteBufferPoolImpl(errorBufferSize, "DBImplErrorBufferPool");
        byteBufferPoolForPartitionKey =
                new FixedByteBufferPoolImpl(PARTITION_KEY_BUFFER_SIZE, "PartitionKeyBufferPool");
        clusterConnection = Ndb_cluster_connection.create(connectString, nodeId);
        handleError(clusterConnection, connectString, nodeId);
        int timeoutError = clusterConnection.set_timeout(connectTimeoutMgm);
        handleError(timeoutError, connectString, nodeId, connectTimeoutMgm);
        logger.trace(() -> local.message("TRACE_Create_Cluster_Connection",
                                         connectString, connectTimeoutMgm));
    }

    public DbFactory createDbFactory(String databaseName, int[] bufferSizes) {
        return new DbFactoryImpl(this, databaseName, bufferSizes);
    }

    public void connect(int connectRetries, int connectDelay, boolean verbose) {
        checkConnection();
        int returnCode = clusterConnection.connect(connectRetries, connectDelay, verbose?1:0);
        handleError(returnCode, clusterConnection, connectString, nodeId);
        nodeId = clusterConnection.node_id();
    }

    protected Ndb createNdb(String database) {
        Ndb ndb = null;
        synchronized(this) {
            ndb = Ndb.create(clusterConnection, database, "def");
            handleError(ndb, clusterConnection, connectString, nodeId);
        }
        return ndb;
    }

    protected DbImpl createDbImplForFactory(DbFactoryImpl factory, int maxTransactions) {
        checkConnection();
        assert(factory.connectionImpl == this);
        Ndb ndb = createNdb(factory.databaseName);
        DbImpl result = new DbImpl(factory, ndb, maxTransactions);
        result.initializeAutoIncrement(autoIncrement);
        return result;
    }

    protected void initializeAutoIncrement(DbImpl db) {
        db.initializeAutoIncrement(autoIncrement);
    }

    protected DbImplForNdbRecord createDbForNdbRecord(DbFactoryImpl dbc) {
        Ndb ndb = createNdb(dbc.databaseName);
        return new DbImplForNdbRecord(dbc, ndb);
    }

    public void configureTls(String tlsSearchPath, int tlsRequirement) {
        if(tlsSearchPath == null) tlsSearchPath = "";
        clusterConnection.configure_tls(tlsSearchPath, tlsRequirement);
    }

    public void waitUntilReady(int connectTimeoutBefore, int connectTimeoutAfter) {
        checkConnection();
        int returnCode = clusterConnection.wait_until_ready(connectTimeoutBefore, connectTimeoutAfter);
        handleError(returnCode, clusterConnection, connectString, nodeId);
    }

    public String systemName() {
        return clusterConnection.get_system_name();
    }

    public int nodeId() {
        return nodeId;
    }

    protected void checkConnection() {
        if (clusterConnection == null) {
            throw new ClusterJFatalInternalException(local.message("ERR_Cluster_Connection_Must_Not_Be_Null"));
        }
    }

    protected static void handleError(int timeoutError, String connectString, int nodeId, int connectTimeoutMgm) {
        if (timeoutError != 0) {
            String message = local.message("ERR_Set_Timeout_Mgm", connectString, nodeId, connectTimeoutMgm, timeoutError);
            logger.error(message);
            throw new ClusterJDatastoreException(message);
        }
    }

    protected static void handleError(int returnCode, Ndb_cluster_connection clusterConnection,
            String connectString, int nodeId) {
        if (returnCode >= 0) {
            return;
        } else {
            try {
                throwError(returnCode, clusterConnection, connectString, nodeId);
            } finally {
                // all errors on Ndb_cluster_connection are fatal
                Ndb_cluster_connection.delete(clusterConnection);
            }
        }
    }

    protected static void handleError(Object object, Ndb_cluster_connection clusterConnection,
            String connectString, int nodeId) {
        if (object != null) {
            return;
        } else {
            throwError(null, clusterConnection, connectString, nodeId);
        }
    }

    protected static void handleError(Ndb_cluster_connection clusterConnection, String connectString, int nodeId) {
        if (clusterConnection == null) {
            String message = local.message("ERR_Connect", connectString, nodeId);
            logger.error(message);
            throw new ClusterJDatastoreException(message);
        }
    }

    protected static void throwError(Object returnCode, Ndb_cluster_connection clusterConnection,
            String connectString, int nodeId) {
        String message = clusterConnection.get_latest_error_msg();
        int errorCode = clusterConnection.get_latest_error();
        String msg = local.message("ERR_NdbError", returnCode, errorCode, message, connectString, nodeId);
        throw new ClusterJDatastoreException(msg);
    }

    public void close() {
       if (clusterConnection != null) {
            logger.info(local.supplier("INFO_Close_Cluster_Connection", connectString, nodeId));
            Ndb_cluster_connection.delete(clusterConnection);
            clusterConnection = null;
        }
    }

    public ValueHandlerFactory getSmartValueHandlerFactory() {
        ValueHandlerFactory result = null;
        if (USE_SMART_VALUE_HANDLER) {
            result = new NdbRecordSmartValueHandlerFactoryImpl();
        }
        return result;
    }

    public void initializeAutoIncrement(long[] autoIncrement) {
        this.autoIncrement = autoIncrement;
    }

    public short getRecvThreadCPUid() {
        return recvThdCpu;
    }

    public void setRecvThreadCPUid(short cpuid) {
        int ret = 0;
        if (cpuid == -1) {
            unsetRecvThreadCPUid();
            return;
        } else if (cpuid < 0) {
            throw new ClusterJUserException(local.message("ERR_Invalid_CPU_Id", cpuid));
        }
        ret = clusterConnection.set_recv_thread_cpu(cpuid);
        if (ret == 0) recvThdCpu = cpuid;
        else {
            // Error in binding cpu
            switch (ret) {
            case 22:    /* EINVAL - Invalid CPU id error in Linux/FreeBSD */
            case 31994: /* CPU_ID_MISSING_ERROR - Invalid CPU id error in Windows */
                throw new ClusterJUserException(
                        local.message("ERR_Invalid_CPU_Id", cpuid));
            case 31999: /* BIND_CPU_NOT_SUPPORTED_ERROR */
                throw new ClusterJFatalUserException(
                        local.message("ERR_Bind_CPU_Not_Supported"));
            default:
                // Unknown error code. Print it to user.
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Binding_Recv_Thread_To_CPU", cpuid, ret));
            }
        }
    }

    public void unsetRecvThreadCPUid() {
        int ret = clusterConnection.unset_recv_thread_cpu();
        if (ret == 31999) {
            // BIND_CPU_NOT_SUPPORTED_ERROR
            throw new ClusterJFatalUserException(
                    local.message("ERR_Bind_CPU_Not_Supported"));
        } else if (ret != 0) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Unbinding_Recv_Thread_From_CPU", ret));
        }
        recvThdCpu = -1;
    }

    public void setRecvThreadActivationThreshold(int threshold) {
        if (clusterConnection.set_recv_thread_activation_threshold(threshold) == -1) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Setting_Activation_Threshold"));
        }
    }
}
