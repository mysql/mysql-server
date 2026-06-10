/*
   Copyright (c) 2024, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.core;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalException;
import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.SessionFactory.State;

import com.mysql.clusterj.core.spi.ValueHandlerFactory;

import com.mysql.clusterj.core.store.ClusterConnection;
import com.mysql.clusterj.core.store.ConnectionHandle;
import com.mysql.clusterj.core.store.ClusterConnectionService;
import com.mysql.clusterj.core.store.DbFactory;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.ConcurrentHashMap;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

class GlobalConnectionPool {

   /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(GlobalConnectionPool.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(GlobalConnectionPool.class);

    /** My class loader */
    static final ClassLoader MY_CLASS_LOADER = GlobalConnectionPool.class.getClassLoader();

    /** The set of all connections (the global connection pool) */
    private static final Set<PooledConnection> allPooledConnections =
        Collections.synchronizedSet(new LinkedHashSet<PooledConnection>());

    /** Map from NDB Connect String to known System Name.

        A user sets the connect string in the connection properties; the system
        name is obtained after the connection is established. Several different
        connect strings can result in a single system name. If a connection using
        a known connect string produces an unexpected system name, then something
        odd has happened (such as a test cluster that has been shut down and
        replaced with a new one); this results in a ClusterJFatalException.

        In many cases this map will have only one entry.
    */
    private static final Map<String, String> systemNameMap =
        new ConcurrentHashMap<String, String>();

    /** Map from connection keys to connections.

        Every connection has a map entry with a key based on the node ID and
        system name, e.g. "Id:49,C:NDB1". When connection pooling is enabled,
        each connection also has a second map entry with a key based on ordinal
        connection number and system name, like "N:1,C:NDB1". Connection pooling
        is enabled unless com.mysql.clusterj.connection.pool.size is set to 0.
    */
    private static final ConcurrentHashMap<String, PooledConnection> connectionMap =
        new ConcurrentHashMap<String, PooledConnection>();

    static private String getKeyByNodeId(String systemName, int nodeId) {
        String key = "Id:" + nodeId + ",C:" + systemName;
        return key;
    }

    static private String getKeyByOrdinal(String systemName, int n) {
        String key = "N:" + n + ",C:" + systemName;
        return key;
    }

    /* A map from SystemName to all session factories connected to that cluster */
    private static final ConcurrentHashMap<String, ArrayList<SessionFactoryImpl>> factoryMap =
        new ConcurrentHashMap<String, ArrayList<SessionFactoryImpl>>();

    /** A public handle to a pooled ClusterConnection */
    public static class Handle implements ConnectionHandle {
        private final PooledConnection parent;

        private Handle(PooledConnection c) { parent = c; }

        private ClusterConnection conn()   { return parent.clusterConnection; }

        public String systemName()         { return conn().systemName(); }

        public int nodeId()                { return parent.nodeId; }

        public State currentState()        { return parent.getState(); }

        public void close()                { parent.closeHandle(); }

        public short getRecvThreadCPUid()  { return conn().getRecvThreadCPUid(); }

        public void reconnect(int timeout) { start_reconnect(this, timeout); }

        public ValueHandlerFactory getSmartValueHandlerFactory() {
            return conn().getSmartValueHandlerFactory();
        }

        public void setRecvThreadCPUid(short cpuid) {
            conn().setRecvThreadCPUid(cpuid);
        }

        public void setRecvThreadActivationThreshold(int t) {
            conn().setRecvThreadActivationThreshold(t);
        }

        public DbFactory createDbFactory(String databaseName, int[] bufferSizes) {
            return conn().createDbFactory(databaseName, bufferSizes);
        }
    }

    /** Record of an actual connection to a cluster. On disconnect, the
        underlying ClusterConnection is deleted; on reconnect, it is replaced.
    */
    private static class PooledConnection {
        final String systemName;
        final int nodeId;
        final int ordinal;
        final boolean anonymous;
        final AtomicInteger userReferenceCount = new AtomicInteger();
        private State state = State.Open;
        ClusterConnection clusterConnection;

        PooledConnection(ClusterConnection conn, int ordinal, boolean anon) {
            systemName = conn.systemName();
            clusterConnection = conn;
            nodeId = conn.nodeId();
            this.ordinal = ordinal;
            this.anonymous = anon;
        }

        synchronized State getState()         { return state; }
        synchronized void setState(State s)   { this.state = s; }

        Handle openHandle() {
            userReferenceCount.incrementAndGet();
            return new Handle(this);
        }

        void closeHandle() {
            assert userReferenceCount.get() > 0;
            if(userReferenceCount.decrementAndGet() == 0)
                disconnect();
        }

        void storeInPool() {
            assert nodeId > 0 ;
            assert systemName != null;
            allPooledConnections.add(this);
            connectionMap.put(getKeyByNodeId(systemName, nodeId), this);
            if(! anonymous)
                connectionMap.put(getKeyByOrdinal(systemName, ordinal), this);
        }

        void disconnect() {
            if(clusterConnection != null) {
                clusterConnection.close();
                state = state.Closed;
                clusterConnection = null;
                connectionMap.remove(getKeyByNodeId(systemName, nodeId));
                if(! anonymous)
                    connectionMap.remove(getKeyByOrdinal(systemName, ordinal));
            }
        }
    }

    private static ClusterConnectionService getClusterConnectionService(String serviceName) {
        return ClusterJHelper.getServiceInstance(ClusterConnectionService.class,
                  serviceName, MY_CLASS_LOADER);
    }

    private static void fail(String msg) {
        logger.warn(msg);
        throw new ClusterJFatalUserException(msg);
    }

    private static void fail(String msg, Throwable ex) {
        logger.warn(msg);
        throw new ClusterJFatalUserException(msg, ex);
    }

    private static class Spec extends PropertyReader {
        final String CONNECTION_SERVICE;
        final String CONNECT_STRING;
        final String NODE_ID_LIST;
        final String BOUND_CPU_LIST;
        final String TLS_SEARCH_PATH;
        final int STRICT_TLS;
        final int CONNECTION_POOL_SIZE;
        final int CONNECT_TIMEOUT_MGM;
        final int CONNECT_RETRIES;
        final int CONNECT_DELAY;
        final int CONNECT_VERBOSE;
        final int CONNECT_TIMEOUT_BEFORE;
        final int CONNECT_TIMEOUT_AFTER;
        final int CONNECT_AUTO_INCREMENT_BATCH_SIZE;
        final long CONNECT_AUTO_INCREMENT_STEP;
        final long CONNECT_AUTO_INCREMENT_START;
        final int RECV_THREAD_ACTIVATION_THRESHOLD;
        final int RECONNECT_TIMEOUT;

        final boolean shouldSetThreshold;
        final List<Integer> nodeIds = new ArrayList<Integer>();
        final boolean poolDisabled;
        final int poolSize;
        final short[] boundCpu;

        Spec(Map<?, ?> props) {
            CONNECTION_SERVICE = getStringProperty(props, PROPERTY_CLUSTER_CONNECTION_SERVICE);
            CONNECT_STRING = getRequiredStringProperty(props, PROPERTY_CLUSTER_CONNECTSTRING);
            NODE_ID_LIST = getStringProperty(props, PROPERTY_CONNECTION_POOL_NODEIDS);
            BOUND_CPU_LIST = getStringProperty(props, PROPERTY_CONNECTION_POOL_RECV_THREAD_CPUIDS);
            TLS_SEARCH_PATH = getStringProperty(props, PROPERTY_TLS_SEARCH_PATH,
                                                DEFAULT_PROPERTY_TLS_SEARCH_PATH);
            STRICT_TLS = getIntProperty(props, PROPERTY_MGM_STRICT_TLS,
                                        DEFAULT_PROPERTY_MGM_STRICT_TLS);
            CONNECTION_POOL_SIZE = getIntProperty(props, PROPERTY_CONNECTION_POOL_SIZE,
                                                  DEFAULT_PROPERTY_CONNECTION_POOL_SIZE);
            RECONNECT_TIMEOUT = getIntProperty(props, PROPERTY_CONNECTION_RECONNECT_TIMEOUT,
                                               DEFAULT_PROPERTY_CONNECTION_RECONNECT_TIMEOUT);
            CONNECT_RETRIES = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_RETRIES,
                                             DEFAULT_PROPERTY_CLUSTER_CONNECT_RETRIES);
            CONNECT_TIMEOUT_MGM = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_TIMEOUT_MGM,
                                                 DEFAULT_PROPERTY_CLUSTER_CONNECT_TIMEOUT_MGM);
            CONNECT_DELAY = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_DELAY,
                                           DEFAULT_PROPERTY_CLUSTER_CONNECT_DELAY);
            CONNECT_VERBOSE = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_VERBOSE,
                                             DEFAULT_PROPERTY_CLUSTER_CONNECT_VERBOSE);
            CONNECT_TIMEOUT_BEFORE = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_TIMEOUT_BEFORE,
                                                    DEFAULT_PROPERTY_CLUSTER_CONNECT_TIMEOUT_BEFORE);
            CONNECT_TIMEOUT_AFTER = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_TIMEOUT_AFTER,
                                                   DEFAULT_PROPERTY_CLUSTER_CONNECT_TIMEOUT_AFTER);
            CONNECT_AUTO_INCREMENT_BATCH_SIZE = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_BATCH_SIZE,
                                                               DEFAULT_PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_BATCH_SIZE);
            CONNECT_AUTO_INCREMENT_STEP = getLongProperty(props, PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_STEP,
                                                          DEFAULT_PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_STEP);
            CONNECT_AUTO_INCREMENT_START = getLongProperty(props, PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_START,
                                                           DEFAULT_PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_START);
            RECV_THREAD_ACTIVATION_THRESHOLD = getIntProperty(props, PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD,
                                                              DEFAULT_PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD);

            shouldSetThreshold = (RECV_THREAD_ACTIVATION_THRESHOLD != DEFAULT_PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD);
            poolSize = configureConnectionPool();
            poolDisabled = checkPoolDisabled();
            boundCpu = new short[poolSize];
            verifyProperties();
        }

        /* Create the list of requested Node IDs and return the actual pool size */
        private int configureConnectionPool() {
            if(NODE_ID_LIST == null) {
                int make_ids = CONNECTION_POOL_SIZE;
                if (make_ids < 1) make_ids = 1;
                for(int i = 0 ; i < make_ids ; i++)
                    nodeIds.add(0);
            } else {
                String[] nodeIdsStringArray = NODE_ID_LIST.split("[,; \t\n\r]+", 48);
                for (String nodeIdString : nodeIdsStringArray) {
                    try {
                        int nodeId = Integer.parseInt(nodeIdString);
                        nodeIds.add(nodeId);
                    } catch (NumberFormatException ex) {
                        fail(local.message("ERR_Node_Ids_Format", NODE_ID_LIST), ex);
                    }
                }
            }

            if (CONNECTION_POOL_SIZE < 2)
                return nodeIds.size();

            if (nodeIds.size() == 1) {
                // add new nodeIds to fill out array
                for (int i = 1; i < CONNECTION_POOL_SIZE; ++i) {
                    nodeIds.add(nodeIds.get(i - 1) + 1);
                }
            }

            if (CONNECTION_POOL_SIZE != nodeIds.size())
                fail(local.message("ERR_Node_Ids_Must_Match_Connection_Pool_Size",
                                   NODE_ID_LIST, CONNECTION_POOL_SIZE));

            return CONNECTION_POOL_SIZE;
        }

        private boolean checkPoolDisabled() {
            if (CONNECTION_POOL_SIZE > 0)
                return false;

            if (nodeIds.size() != 1)   // pool disabled but more than one nodeId specified
                fail(local.message("ERR_Multiple_Node_Ids_For_Disabled_Connection_Pool",
                                   NODE_ID_LIST));
            return true;
        }

        void verifyProperties() {
            if (BOUND_CPU_LIST != null) {
                int i = 0;
                String[] cpuIdsStringArray = BOUND_CPU_LIST.split("[,; \t\n\r]+", 64);
                if (cpuIdsStringArray.length != poolSize) {
                    fail(poolDisabled ?
                         local.message("ERR_Multiple_CPU_Ids_For_Disabled_Connection_Pool",
                                       BOUND_CPU_LIST) :
                         local.message("ERR_CPU_Ids_Must_Match_Connection_Pool_Size",
                                       BOUND_CPU_LIST, poolSize));
                }
                for (String cpuIdString : cpuIdsStringArray) {
                    try {
                        boundCpu[i++] = Short.parseShort(cpuIdString);
                    } catch (NumberFormatException ex) {
                        fail(local.message("ERR_CPU_Ids_Format", BOUND_CPU_LIST));
                    }
                }
            }

           if (RECV_THREAD_ACTIVATION_THRESHOLD < 0)
                fail(local.message("ERR_Invalid_Activation_Threshold", RECV_THREAD_ACTIVATION_THRESHOLD));
        }
    }

    /** Given a set of properties, return a pool of connection handles */
    public static void
      getConnections(List<ConnectionHandle> userHandles, Map<?, ?> props) {
        Spec spec = new Spec(props);
        int nConn = spec.poolSize;
        assert nConn > 0;
        for(int i = 0 ; i < nConn ; i++) {
            PooledConnection conn = null;
            try {
                conn = getConnection(props, spec, i);
            } catch (Throwable e) {
                for(int j = 0 ; j < i ; j++)
                    userHandles.get(j).close();
                throw e;
            }
            Handle handle = conn.openHandle();
            userHandles.add(handle);
        }
    }

    /* Find or create connection n (out of m) for spec */
    private static PooledConnection getConnection(Map<?, ?> props, Spec spec, int n) {
        String systemName;
        PooledConnection result;
        int nodeId = spec.nodeIds.get(n);
        boolean specificId = (nodeId > 0);
        systemName = systemNameMap.get(spec.CONNECT_STRING);

        /* If this is the first request with this connection string, or
           the user has explicitly asked to override pooling (by setting
           pool size to 0), create a new connection. */
        if(systemName == null || spec.poolDisabled)
            return createConnection(props, spec, n);

        /* The connection string is already known, so look for an existing connection. */
        String key = specificId ? getKeyByNodeId(systemName, nodeId)
                                : getKeyByOrdinal(systemName, n);
        result = connectionMap.get(key);

        /* We could not find an existing connection, so create one */
        if(result == null)
            result = createConnection(props, spec, n);

        return result;
    }

    private static PooledConnection createConnection(Map props, Spec spec, int n) {
        int idx;
        int nodeId = spec.nodeIds.get(n);
        String systemName = null;

        ClusterConnection c = createClusterConnection(props, spec, n, nodeId);
        PooledConnection result = new PooledConnection(c, n, spec.poolDisabled);

        /* Check and add the mapping from ConnectString to SystemName */
        systemName = systemNameMap.get(spec.CONNECT_STRING);
        if(systemName == null)
            systemNameMap.put(spec.CONNECT_STRING, result.systemName);
        else if(! systemName.equals(result.systemName))
            throw new ClusterJFatalException(
                local.message("ERR_System_Name_Mismatch", spec.CONNECT_STRING,
                              systemName, result.systemName));

        /* Store the Connection onto the global pool and connection map */
        result.storeInPool();

        return result;
    }

    private static ClusterConnection createClusterConnection(Map<?, ?> props, Spec spec,
                                                             int n, int nodeId) {
        ClusterConnectionService service =
            ClusterJHelper.getServiceInstance(ClusterConnectionService.class,
                                              spec.CONNECTION_SERVICE, MY_CLASS_LOADER);
        ClusterConnection result = null;
        boolean connected = false;
        try {
            result = service.create(spec.CONNECT_STRING, nodeId, spec.CONNECT_TIMEOUT_MGM);
            result.configureTls(spec.TLS_SEARCH_PATH, spec.STRICT_TLS);
            result.connect(spec.CONNECT_RETRIES, spec.CONNECT_DELAY, true);
            result.waitUntilReady(spec.CONNECT_TIMEOUT_BEFORE, spec.CONNECT_TIMEOUT_AFTER);
            // The connection must be closed if the method fails after this point.
            connected = true;
            // Set auto-increment paramaters
            result.initializeAutoIncrement(new long[] {
                spec.CONNECT_AUTO_INCREMENT_BATCH_SIZE,
                spec.CONNECT_AUTO_INCREMENT_STEP,
                spec.CONNECT_AUTO_INCREMENT_START
            });
            // Configure the receive thread
            if (spec.BOUND_CPU_LIST != null && spec.boundCpu[n] != -1)
                result.setRecvThreadCPUid(spec.boundCpu[n]);
            if (spec.shouldSetThreshold)
                result.setRecvThreadActivationThreshold(spec.RECV_THREAD_ACTIVATION_THRESHOLD);
        } catch (Exception ex) {
            if (connected)
                result.close();  // close result if it has connected already
            fail(local.message("ERR_Connecting", props), ex);
        }
        logger.info(local.supplier("INFO_Connected", result.systemName(), result.nodeId()));
        return result;
    }

    /* Reconnection logic */
    protected static void registerSessionFactory(String connectString, SessionFactoryImpl factory) {
        String system = systemNameMap.get(connectString);
        assert system != null;
        ArrayList<SessionFactoryImpl> list = factoryMap.get(system);
        if(list == null) {
            list = new ArrayList<SessionFactoryImpl>();
            ArrayList<SessionFactoryImpl> winner = factoryMap.putIfAbsent(system, list);
            if(winner != null) list = winner;
        }
        list.add(factory);
    }

    protected static void closeSessionFactory(String connectString, SessionFactoryImpl factory) {
        String system = systemNameMap.get(connectString);
        ArrayList<SessionFactoryImpl> list = factoryMap.get(system);
        list.remove(factory);
    }

    private static ThreadGroup threadGroup = new ThreadGroup("Reconnect");

    private static void start_reconnect(Handle handle, int timeout) {
        // Mark the problematic connection ... and all its brethren
        PooledConnection origin = handle.parent;
        for(PooledConnection c : allPooledConnections)
            if(c.systemName.equals(origin.systemName))
                c.setState(State.Reconnecting);

        // Start a thread to handle the reconnect logic
        Thread reconnectThread =
            new Thread(threadGroup, new ReconnectThread(origin.systemName, timeout));
        reconnectThread.start();
        logger.warn(local.message("WARN_Reconnect_started"));
    }

    protected static class ReconnectThread implements Runnable {
        private int timeout;
        private final ArrayList<SessionFactoryImpl> factories = new ArrayList<SessionFactoryImpl>();
        public ReconnectThread(String systemName, int timeout) {
            this.timeout = timeout;
            this.factories.addAll(factoryMap.get(systemName));
        }

        public void run() {
            /* Notify the factories.
             * Only keep the factories that need to participate in this reconnect.
             */
            for(SessionFactoryImpl factory : factories)
                if(factory.check_pool_for_disconnect() == false)
                    factories.remove(factory);

            boolean done = false;
            int iterations = timeout;
            while(!done && iterations-- > 0) {
                int total = 0;
                for(SessionFactoryImpl factory : factories)
                    total += factory.disconnect_check_active_sessions();
                done = (total == 0);
                if(! done) {
                    logger.info(local.supplier("INFO_Reconnect_wait", total));
                    sleep(1000);
                }
            }

            /* Set DatabaseConnections to closing */
            for(SessionFactoryImpl factory : factories)
                factory.disconnect_set_closing();

            if(! done) sleep(1000);

            /* Close all remaining sessions and connections */
            for(SessionFactoryImpl factory : factories)
                factory.disconnect_close();

           /* Delete the connections that were closed */
           for(PooledConnection c : allPooledConnections)
              if(c.getState().equals(State.Closed))
                  allPooledConnections.remove(c);

            /* Ask all SessionFactories to reconnect */
            for(SessionFactoryImpl factory : factories)
                factory.do_reconnect();
        }

        private static void sleep(long millis) {
            try {
                Thread.sleep(millis);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }
}
