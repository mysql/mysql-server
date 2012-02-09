/*
 *  Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.Ndb_cluster_connection;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.Db;
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
            .getInstance(com.mysql.clusterj.core.store.ClusterConnection.class);

    /** Ndb_cluster_connection is wrapped by ClusterConnection */
    protected Ndb_cluster_connection clusterConnection;

    /** The connection string for this connection */
    final String connectString;

    /** The node id requested for this connection; 0 for default */
    final int nodeId;

    /** All dbs given out by this cluster connection */
    private Map<Db, Object> dbs = new IdentityHashMap<Db, Object>();

    /** The map of table name to NdbRecordImpl */
    private ConcurrentMap<String, NdbRecordImpl> ndbRecordImplMap = new ConcurrentHashMap<String, NdbRecordImpl>();

    /** The dictionary used to create NdbRecords */
    Dictionary dictionaryForNdbRecord = null;

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
            if (dictionaryForNdbRecord == null) {
                // create a dictionary for NdbRecord
                Ndb ndbForNdbRecord = Ndb.create(clusterConnection, database, "def");
                handleError(ndbForNdbRecord, clusterConnection, connectString, nodeId);
                DbImplForNdbRecord dbForNdbRecord = new DbImplForNdbRecord(this, ndbForNdbRecord);
                dbs.put(dbForNdbRecord, null);
                dictionaryForNdbRecord = dbForNdbRecord.getNdbDictionary();
            }
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
            for (NdbRecordImpl ndbRecord: ndbRecordImplMap.values()) {
                ndbRecord.releaseNdbRecord();
            }
            ndbRecordImplMap.clear();
            if (dbs.size() != 0) {
                Map<Db, Object> dbsToClose = new IdentityHashMap<Db, Object>(dbs);
                for (Db db: dbsToClose.keySet()) {
                    db.close();
                }
            }
            Ndb_cluster_connection.delete(clusterConnection);
            clusterConnection = null;
        }
    }

    public void close(Db db) {
        dbs.remove(db);
    }

    public int dbCount() {
        // one of the dbs is for the NdbRecord dictionary if it is not null
        int dbForNdbRecord = (dictionaryForNdbRecord == null)?0:1;
        return dbs.size() - dbForNdbRecord;
    }

    /** 
     * Get the cached NdbRecord implementation for this cluster connection.
     * There are three possibilities:
     * <ul><li>Case 1: return the already-cached NdbRecord
     * </li><li>Case 2: return a new instance created by this method
     * </li><li>Case 3: return the winner of a race with another thread
     * </li></ul>
     * @param storeTable the store table
     * @param ndbDictionary the ndb dictionary
     * @return the NdbRecordImpl
     */
    protected NdbRecordImpl getCachedNdbRecordImpl(Table storeTable) {
        String tableName = storeTable.getName();
        // find the NdbRecordImpl in the global cache
        NdbRecordImpl result = ndbRecordImplMap.get(tableName);
        if (result != null) {
            // case 1
            if (logger.isDebugEnabled())logger.debug("NdbRecordImpl found for " + tableName);
            return result;
        } else {
            NdbRecordImpl newNdbRecordImpl = new NdbRecordImpl(storeTable, dictionaryForNdbRecord);
            NdbRecordImpl winner = ndbRecordImplMap.putIfAbsent(tableName, newNdbRecordImpl);
            if (winner == null) {
                // case 2: the previous value was null, so return the new (winning) value
                if (logger.isDebugEnabled())logger.debug("NdbRecordImpl created for " + tableName);
                return newNdbRecordImpl;
            } else {
                // case 3: another thread beat us, so return the winner and garbage collect ours
                if (logger.isDebugEnabled())logger.debug("NdbRecordImpl lost race for " + tableName);
                newNdbRecordImpl.releaseNdbRecord();
                return winner;
            }
        }
    }

    /** Remove the cached NdbRecord associated with this table. This allows schema change to work.
     * @param tableName the name of the table
     */
    public void unloadSchema(String tableName) {
        if (logger.isDebugEnabled())logger.debug("Removing cached NdbRecord for " + tableName);
        NdbRecordImpl ndbRecordImpl = ndbRecordImplMap.remove(tableName);
        if (ndbRecordImpl != null) {
            ndbRecordImpl.releaseNdbRecord();
        }
        if (logger.isDebugEnabled())logger.debug("Removing dictionary entry for cached table " + tableName);
        dictionaryForNdbRecord.removeCachedTable(tableName);
    }

}
