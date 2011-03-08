/*
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.clusterj.tie;

import java.util.IdentityHashMap;
import java.util.Map;

import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.Ndb_cluster_connection;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.Db;

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
            .getInstance(com.mysql.clusterj.core.store.ClusterConnection.class);

    /** Load the ndbjtie system library */
    static {
        loadSystemLibrary("ndbclient");
        // initialize the charset map
        Utility.getCharsetMap();
    }

    /** Ndb_cluster_connection is wrapped by ClusterConnection */
    protected Ndb_cluster_connection clusterConnection;

    /** The connection string for this connection */
    final String connectString;

    /** The node id requested for this connection; 0 for default */
    final int nodeId;

    /** All dbs given out by this cluster connection */
    private Map<DbImpl, Object> dbs = new IdentityHashMap<DbImpl, Object>();

    /** Connect to the MySQL Cluster
     * 
     * @param connectString the connect string
     * @param nodeId the node id; node id of zero means "any node"
     */
    public ClusterConnectionImpl(String connectString, int nodeId) {
        this.connectString = connectString;
        this.nodeId = nodeId;
        clusterConnection = Ndb_cluster_connection.create(connectString, nodeId);
        handleError(clusterConnection, connectString, nodeId);
        logger.info(local.message("INFO_Create_Cluster_Connection", connectString, nodeId));
    }

    static protected void loadSystemLibrary(String name) {
        String message;
        String path;
        try {
            System.loadLibrary(name);
        } catch (UnsatisfiedLinkError e) {
            path = getLoadLibraryPath();
            message = local.message("ERR_Failed_Loading_Library",
                    name, path, "UnsatisfiedLinkError", e.getLocalizedMessage());
            logger.fatal(message);
            throw e;
        } catch (SecurityException e) {
            path = getLoadLibraryPath();
            message = local.message("ERR_Failed_Loading_Library",
                    name, path, "SecurityException", e.getLocalizedMessage());
            logger.fatal(message);
            throw e;
        }
    }

    /**
     * @return the load library path or the Exception string
     */
    private static String getLoadLibraryPath() {
        String path;
        try {
            path = System.getProperty("java.library.path");
        } catch (Exception ex) {
            path = "<Exception: " + ex.getMessage() + ">";
        }
        return path;
    }


    public void connect(int connectRetries, int connectDelay, boolean verbose) {
        checkConnection();
        int returnCode = clusterConnection.connect(connectRetries, connectDelay, verbose?1:0);
        handleError(returnCode, clusterConnection, connectString, nodeId);
    }

    public Db createDb(String database, int maxTransactions) {
        checkConnection();
        Ndb ndb = null;
        // synchronize because create is not guaranteed thread-safe
        synchronized(this) {
            ndb = Ndb.create(clusterConnection, database, "def");
            handleError(ndb, clusterConnection, connectString, nodeId);
        }
        DbImpl result = new DbImpl(this, ndb, maxTransactions);
        dbs.put(result, null);
        return result;
    }

    public void waitUntilReady(int connectTimeoutBefore, int connectTimeoutAfter) {
        checkConnection();
        int returnCode = clusterConnection.wait_until_ready(connectTimeoutBefore, connectTimeoutAfter);
        handleError(returnCode, clusterConnection, connectString, nodeId);
    }

    private void checkConnection() {
        if (clusterConnection == null) {
            throw new ClusterJFatalInternalException(local.message("ERR_Cluster_Connection_Must_Not_Be_Null"));
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
            logger.info(local.message("INFO_Close_Cluster_Connection", connectString, nodeId));
            Ndb_cluster_connection.delete(clusterConnection);
            clusterConnection = null;
        }
    }

    public void close(Db db) {
        dbs.remove(db);
    }

    public int dbCount() {
        return dbs.size();
    }

}
