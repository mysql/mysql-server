/*
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *  All rights reserved. Use is subject to license terms.
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

import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.Ndb_cluster_connection;

import com.mysql.clusterj.ClusterJDatastoreException;

import com.mysql.clusterj.core.store.Db;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 *
 */
public class ClusterConnectionImpl
        implements com.mysql.clusterj.core.store.ClusterConnection {

    /** Load the ndbjtie system library */
    static {
        loadSystemLibrary("ndbclient");
    }

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ClusterConnectionImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(com.mysql.clusterj.core.store.ClusterConnection.class);

    /** Ndb_cluster_connection: one per factory. */
    protected Ndb_cluster_connection clusterConnection;

    /** Connect to the MySQL Cluster
     * 
     * @param connectString the connect string
     */
    public ClusterConnectionImpl(String connectString) {
        clusterConnection = Ndb_cluster_connection.create(connectString);
        handleError(clusterConnection, connectString);
    }

    static protected void loadSystemLibrary(String name) {
        System.out.println("loading libary...");
        try {
            System.loadLibrary(name);
        } catch (UnsatisfiedLinkError e) {
            String path;
            try {
                path = System.getProperty("java.library.path");
            } catch (Exception ex) {
                path = "<exception caught: " + ex.getMessage() + ">";
            }
            System.err.println("failed loading library '"
                        + name + "'; java.library.path='" + path + "'");
            throw e;
        } catch (SecurityException e) {
            System.err.println("failed loading library '"
                        + name + "'; caught exception: " + e);
            throw e;
        }
        System.out.println("... [" + name + "]");
    }


    public void connect(int connectRetries, int connectDelay, boolean verbose) {
        int returnCode = clusterConnection.connect(connectRetries, connectDelay, verbose?1:0);
        handleError(returnCode, clusterConnection);
    }

    public Db createDb(String database, int maxTransactions) {
        Ndb ndb = Ndb.create(clusterConnection, database, "def");
        handleError(ndb, clusterConnection);
        return new DbImpl(ndb, maxTransactions);
    }

    public void waitUntilReady(int connectTimeoutBefore, int connectTimeoutAfter) {
        int returnCode = clusterConnection.wait_until_ready(connectTimeoutBefore, connectTimeoutAfter);
        handleError(returnCode, clusterConnection);
    }

    protected static void handleError(int returnCode, Ndb_cluster_connection clusterConnection) {
        if (returnCode == 0) {
            return;
        } else {
            throwError(returnCode, clusterConnection);
        }
    }

    protected static void handleError(Object object, Ndb_cluster_connection clusterConnection) {
        if (object != null) {
            return;
        } else {
            throwError(null, clusterConnection);
        }
    }

    protected static void handleError(Ndb_cluster_connection clusterConnection, String connectString) {
        if (clusterConnection == null) {
            throw new ClusterJDatastoreException(
                    local.message("ERR_Cannot_Create_Cluster_Connection", connectString));
        }
    }

    protected static void throwError(Object returnCode, Ndb_cluster_connection clusterConnection) {
        String message = clusterConnection.get_latest_error_msg();
        int errorCode = clusterConnection.get_latest_error();
        String msg = local.message("ERR_NdbError", returnCode, errorCode, message);
        throw new ClusterJDatastoreException(msg);
    }

}
