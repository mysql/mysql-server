/*
   Copyright (c) 2010, 2026, Oracle and/or its affiliates.

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

import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Connection;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandlerFactory;
import com.mysql.clusterj.core.spi.ValueHandlerFactory;
import com.mysql.clusterj.core.metadata.DomainTypeHandlerFactoryImpl;
import com.mysql.clusterj.core.store.Db;
import com.mysql.clusterj.core.store.DbFactory;
import com.mysql.clusterj.core.store.ConnectionHandle;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class SessionFactoryImpl implements SessionFactory {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(SessionFactoryImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(SessionFactoryImpl.class);

    /** My class loader */
    static final ClassLoader SESSION_FACTORY_IMPL_CLASS_LOADER = SessionFactoryImpl.class.getClassLoader();

    /** The status of this session factory */
    protected State state;

    /** The properties */
    private final Map<?, ?> props;

    /** NdbCluster connect properties */
    static class Spec extends PropertyReader {
        final int CONNECTION_POOL_SIZE;
        final String CONNECT_STRING;
        final String DATABASE;
        final int MAX_TRANSACTIONS;
        final int RECONNECT_TIMEOUT;
        final int RECV_THREAD_ACTIVATION_THRESHOLD;
        final String BUFFER_POOL_SIZE_LIST;
        final int[] BYTE_BUFFER_POOL_SIZES;
        final int SESSION_CACHE_SIZE;
        final int TABLE_WAIT_MSEC;
        final boolean MULTI_DB;

        Spec(Map<?, ?> props) {
            CONNECTION_POOL_SIZE = getIntProperty(props, PROPERTY_CONNECTION_POOL_SIZE,
                                                  DEFAULT_PROPERTY_CONNECTION_POOL_SIZE);
            CONNECT_STRING = getRequiredStringProperty(props, PROPERTY_CLUSTER_CONNECTSTRING);
            DATABASE = getStringProperty(props, PROPERTY_CLUSTER_DATABASE,
                                         DEFAULT_PROPERTY_CLUSTER_DATABASE);
            MAX_TRANSACTIONS = getIntProperty(props, PROPERTY_CLUSTER_MAX_TRANSACTIONS,
                                              DEFAULT_PROPERTY_CLUSTER_MAX_TRANSACTIONS);
            RECONNECT_TIMEOUT = getIntProperty(props, PROPERTY_CONNECTION_RECONNECT_TIMEOUT,
                                               DEFAULT_PROPERTY_CONNECTION_RECONNECT_TIMEOUT);
            RECV_THREAD_ACTIVATION_THRESHOLD = getIntProperty(props, PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD,
                                                              DEFAULT_PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD);
            BUFFER_POOL_SIZE_LIST = getStringProperty(props, PROPERTY_CLUSTER_BYTE_BUFFER_POOL_SIZES,
                                                      DEFAULT_PROPERTY_CLUSTER_BYTE_BUFFER_POOL_SIZES);
            BYTE_BUFFER_POOL_SIZES = getByteBufferPoolSizes();
            SESSION_CACHE_SIZE = getIntProperty(props, PROPERTY_CLUSTER_MAX_CACHED_SESSIONS,
                                                DEFAULT_PROPERTY_CLUSTER_MAX_CACHED_SESSIONS);
            TABLE_WAIT_MSEC = getIntProperty(props, PROPERTY_TABLE_WAIT_MSEC,
                                             DEFAULT_PROPERTY_TABLE_WAIT_MSEC);
            MULTI_DB = getBooleanProperty(props, PROPERTY_CLUSTER_MULTI_DB,
                                          DEFAULT_PROPERTY_CLUSTER_MULTI_DB);

            if(SESSION_CACHE_SIZE < 0)
                throw new ClusterJFatalUserException(
                    local.message("ERR_value_low", PROPERTY_CLUSTER_MAX_CACHED_SESSIONS, 0));
            if(TABLE_WAIT_MSEC < 0)
                throw new ClusterJFatalUserException(
                    local.message("ERR_value_low", PROPERTY_TABLE_WAIT_MSEC, 0));
            if(TABLE_WAIT_MSEC > 1000)
                throw new ClusterJFatalUserException(
                    local.message("ERR_value_high", PROPERTY_TABLE_WAIT_MSEC, 1000));
        }

        Spec(Spec other, String database) {
            CONNECTION_POOL_SIZE = other.CONNECTION_POOL_SIZE;
            CONNECT_STRING = other.CONNECT_STRING;
            MAX_TRANSACTIONS = other.MAX_TRANSACTIONS;
            RECONNECT_TIMEOUT = other.RECONNECT_TIMEOUT;
            RECV_THREAD_ACTIVATION_THRESHOLD = other.RECV_THREAD_ACTIVATION_THRESHOLD;
            BUFFER_POOL_SIZE_LIST = other.BUFFER_POOL_SIZE_LIST;
            BYTE_BUFFER_POOL_SIZES = other.BYTE_BUFFER_POOL_SIZES;
            SESSION_CACHE_SIZE = other.SESSION_CACHE_SIZE;
            TABLE_WAIT_MSEC = other.TABLE_WAIT_MSEC;
            MULTI_DB = other.MULTI_DB;
            DATABASE = database;
        }

        /** Get the byteBufferPoolSizes from properties */
        private int[] getByteBufferPoolSizes() {
            int[] result;
            // separators are any combination of white space, commas, and semicolons
            String[] byteBufferPoolSizesList = BUFFER_POOL_SIZE_LIST.split("[,; \t\n\r]+", 48);
            int count = byteBufferPoolSizesList.length;
            result = new int[count];
            for (int i = 0; i < count; ++i) {
                try {
                    result[i] = Integer.parseInt(byteBufferPoolSizesList[i]);
                } catch (NumberFormatException ex) {
                    fail(local.message("ERR_Byte_Buffer_Pool_Sizes_Format",
                                       BUFFER_POOL_SIZE_LIST), ex);
                }
            }
            return result;
        }

        private static void fail(String msg, Throwable ex) {
            logger.warn(msg);
            throw new ClusterJFatalUserException(msg, ex);
        }
    }

    private final Spec spec;
    private int CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD = 0;
    private int CLUSTER_RECONNECT_TIMEOUT = 0;

    /** Node ids obtained from the property PROPERTY_CONNECTION_POOL_NODEIDS */
    List<Integer> nodeIds = new ArrayList<Integer>();

    /** Actual number of connection handles obtained from the global connection pool */
    int connectionPoolSize;

    /** Internal class of one-per-connection data members.
    */
    static class PooledConnection {
         final ConnectionHandle connection;
         final DbFactory dbFactory;

        PooledConnection(ConnectionHandle c, Spec spec) {
            connection = c;
            dbFactory = connection.createDbFactory(spec.DATABASE,
                                                   spec.BYTE_BUFFER_POOL_SIZES);
            dbFactory.useSessionCache(spec.SESSION_CACHE_SIZE);
            dbFactory.setTableWaitTime(spec.TABLE_WAIT_MSEC);
        }

        Db createDb(int maxTransactions) {
            return dbFactory.createDb(maxTransactions);
        }

        ConnectionHandle handle()         { return connection; }

        State currentState()              { return connection.currentState(); }

        void unloadSchema(String table)   { dbFactory.unloadSchema(table); }

        int dbCount()                     { return dbFactory.dbCount(); }

        ValueHandlerFactory getSmartValueHandlerFactory() {
            return connection.getSmartValueHandlerFactory();
        }

        void setClosing()                 { dbFactory.closing(); }

        void close() {
            dbFactory.close();
            connection.close();
        }

        void reconnect(int timeout)       { connection.reconnect(timeout); }

        void setRecvThreadCPUid(short id) { connection.setRecvThreadCPUid(id); }

        short getRecvThreadCPUid()        { return connection.getRecvThreadCPUid(); }

        void setRecvThreadActivationThreshold(int t) {
            connection.setRecvThreadActivationThreshold(t);
        }

        boolean isReconnecting() {
            return connection.currentState().equals(State.Reconnecting);
        }
    }

    /** Boolean flag indicating if connection pool is disabled or not */
    boolean connectionPoolDisabled = false;

    /** Map of Proxy to Class */
    static private Map<Class<?>, Class<?>> proxyClassToDomainClass =
            new ConcurrentHashMap<>();

    /** Main map of Domain Class to DomainTypeHandler */
    final private ConcurrentMap<Class<?>, DomainTypeHandler<?>> typeToHandlerMap =
            new ConcurrentHashMap<Class<?>, DomainTypeHandler<?>>();

    /** DomainTypeHandler map used only during schema change handling */
    final private Map<Class<?>, DomainTypeHandler<?>> schemaLocks =
            new HashMap<Class<?>, DomainTypeHandler<?>>();

    /** DomainTypeHandlerFactory for this session factory. */
    DomainTypeHandlerFactory domainTypeHandlerFactory = new DomainTypeHandlerFactoryImpl();

    /** The session factories. */
    static final protected Map<String, SessionFactory> sessionFactoryMap =
            new HashMap<String, SessionFactory>();

    /** The key for this factory */
    final private String key;

    /** Cluster connections that together can be used to manage sessions */
    private List<PooledConnection> pooledConnections = new ArrayList<PooledConnection>();

    /** The smart value handler factory */
    protected ValueHandlerFactory smartValueHandlerFactory;

    /** Get a session factory. If using connection pooling and there is already a session factory
     * with the same connect string and database, return it, regardless of whether other
     * properties of the factory are the same as specified in the Map.
     * If not using connection pooling (maximum sessions per connection == 0), create a new session factory.
     * @param props properties of the session factory
     * @return the session factory
     */
    static public SessionFactory getSessionFactory(Map<?, ?> props) {
        SessionFactory result = null;
        Spec spec = new Spec(props);

        if(spec.CONNECTION_POOL_SIZE > 0) {
            String sessionFactoryKey = getSessionFactoryKey(spec);
            synchronized(sessionFactoryMap) {
                result = sessionFactoryMap.get(sessionFactoryKey);
                if (result == null) {
                    if(spec.MULTI_DB) {
                        result = new MultiDbSessionFactory(spec, props);
                    } else {
                        result = new SessionFactoryImpl(spec, props);
                    }
                    sessionFactoryMap.put(sessionFactoryKey, result);
                }
            }
        } else {
            if(spec.MULTI_DB) {
                throw new ClusterJFatalUserException(local.message("ERR_multidb_no_pool"));
            }
            // if not using connection pooling or multidb, create a new session factory
            result = new SessionFactoryImpl(spec, props);
        }
        return result;
    }

    static void removeFactoryFromMap(Spec spec) {
        assert spec.MULTI_DB;
        synchronized(sessionFactoryMap) {
            sessionFactoryMap.remove(getSessionFactoryKey(spec));
        }
    }

    private static String getSessionFactoryKey(Spec spec) {
        String key = spec.CONNECT_STRING;
        key += spec.MULTI_DB ? "+.MultiDB."
                             : "+" + spec.DATABASE;
        key = key + "+Csz" + spec.SESSION_CACHE_SIZE
                  + "+wait" + spec.TABLE_WAIT_MSEC
                  + "+Bbp" + Arrays.hashCode(spec.BYTE_BUFFER_POOL_SIZES);
        return key;
    }

    /* Returns a ConnectionHandle to SessionImpl for session.getConnection() */
    protected ConnectionHandle getConnectionHandle(int index) {
        return pooledConnections.get(index).handle();
    }

    /** Create a new SessionFactoryImpl from the properties in the Map, and
     * connect to the ndb cluster.
     *
     * @param props the properties for the factory
     */
    SessionFactoryImpl(Spec spec, Map<?, ?> props) {
        this.spec = spec;
        this.props = props;
        key = getSessionFactoryKey(spec);
        CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD = spec.RECV_THREAD_ACTIVATION_THRESHOLD;
        CLUSTER_RECONNECT_TIMEOUT = spec.RECONNECT_TIMEOUT;
        connectionPoolSize = createClusterConnectionPool();
        verifyConnectionPool();
        state = State.Open;
        GlobalConnectionPool.registerSessionFactory(spec.CONNECT_STRING, this);
    }

    private int createClusterConnectionPool() {
        List<ConnectionHandle> handles = new ArrayList<ConnectionHandle>();

        // Pass props to global pool to obtain a list of connection handles
        GlobalConnectionPool.getConnections(handles, props);

       // Move the handles into PooledConnections
        for(ConnectionHandle handle : handles)
            pooledConnections.add(new PooledConnection(handle, spec));

        // get the smart value handler factory (it will be the same for all connections)
        smartValueHandlerFactory = pooledConnections.get(0).getSmartValueHandlerFactory();

        return pooledConnections.size();
    }

    protected void verifyConnectionPool() {
        assert connectionPoolSize > 0;
        // Get a Session for each connection in the pool and complete
        // a transaction to make sure that each connection is ready
        List<Integer> sessionCounts = null;
        String msg;
        try {
            List<Session> sessions = new ArrayList<Session>(pooledConnections.size());
            for (int i = 0; i < pooledConnections.size(); ++i) {
                sessions.add(getSession(null, true));
            }
            sessionCounts = getConnectionPoolSessionCounts();
            for (Session session: sessions) {
                session.currentTransaction().begin();
                session.currentTransaction().commit();
                session.close();
            }
        } catch (RuntimeException e) {
            msg = local.message("ERR_Session_Factory_Impl_Failed_To_Complete_Transaction");
            logger.warn(msg);
            throw e;
        }
        // verify that the session counts were correct
        for (Integer count: sessionCounts) {
            if (count != 1) {
                msg = local.message("ERR_Session_Counts_Wrong_Creating_Factory",
                        sessionCounts.toString());
                logger.warn(msg);
                throw new ClusterJFatalInternalException(msg);
            }
        }
    }

    /** Get a session to use with the cluster.
     *
     * @return the session
     */
    public Session getSession() {
        return getSession(null, false);
    }

    public Session getSession(String database) {
        if(database == null) return getSession();
        if(database.equals(spec.DATABASE)) return getSession();

        throw new ClusterJUserException(local.message("ERR_Not_MultiDB"));
    }

    public Session getSession(Map properties) {
        return getSession(null, false);
    }

    /** Get a session to use with the cluster, overriding some properties.
     * Properties PROPERTY_CLUSTER_CONNECTSTRING, PROPERTY_CLUSTER_DATABASE,
     * and PROPERTY_CLUSTER_MAX_TRANSACTIONS may not be overridden.
     * @param properties overriding some properties for this session
     * @return the session
     */
    public Session getSession(Map properties, boolean internal) {
        try {
            Db db = null;
            int idx = 0;
            synchronized(this) {
                if (!(State.Open.equals(state)) && !internal) {
                    throw new ClusterJUserException(local.message("ERR_SessionFactory_not_open"));
                }
                idx = getIndexOfBestPooledConnection();
                PooledConnection connection = pooledConnections.get(idx);
                checkConnection(connection);
                db = connection.createDb(spec.MAX_TRANSACTIONS);
            }
            return new SessionImpl(this, idx, db);
        } catch (ClusterJException ex) {
            throw ex;
        } catch (Exception ex) {
            throw new ClusterJFatalException(
                    local.message("ERR_Create_Ndb"), ex);
        }
    }

    private int getIndexOfBestPooledConnection() {
        int result = 0;
        if (connectionPoolSize == 1) {
            return result;
        }
        // find the best pooled connection (the connection with the least active sessions)
        // this is not perfect without synchronization since a connection might close sessions
        // after getting the dbCount but we don't care about perfection here. 
        int bestCount = Integer.MAX_VALUE;
        for (int i = 0 ; i < connectionPoolSize ; i++) {
            PooledConnection connection = pooledConnections.get(i);
            int count = connection.dbCount();
            if (count < bestCount) {
                bestCount = count;
                result = i;
            }
        }
        return result;
    }

    private void checkConnection(PooledConnection connection) {
        if (connection == null) {
            throw new ClusterJUserException(local.message("ERR_Session_Factory_Closed"));
        }
    }

    /** Get the DomainTypeHandler for a class. If the handler is not already
     * available, null is returned. 
     * @param cls the Class for which to get domain type handler
     * @return the DomainTypeHandler or null if not available
     */
    <T> DomainTypeHandler<T> getDomainTypeHandler(Class<T> cls) {
        @SuppressWarnings( "unchecked" )
        DomainTypeHandler<T> domainTypeHandler = (DomainTypeHandler<T>) typeToHandlerMap.get(cls);
        if(domainTypeHandler.isClosing())
            throw ClusterJDatastoreException.forSchemaChange(domainTypeHandler);
        return domainTypeHandler;
    }

    /** Create or get the DomainTypeHandler for a class.
     * Use the dictionary to validate against schema.
     * @param cls the Class for which to get domain type handler
     * @param dictionary the dictionary to validate against
     * @return the type handler
     */
    public <T> DomainTypeHandler<T> getDomainTypeHandler(Class<T> cls, Dictionary dictionary) {
        @SuppressWarnings("unchecked")
        DomainTypeHandler<T> domainTypeHandler = (DomainTypeHandler<T>) typeToHandlerMap.get(cls);

        if (domainTypeHandler == null) {
            domainTypeHandler = createTypeHandlerInMaps(cls, dictionary);
        } else if(domainTypeHandler.isClosing()) {
            throw ClusterJDatastoreException.forSchemaChange(domainTypeHandler);
        }

        return domainTypeHandler;
    }

    /* Creation of DomainTypeHandlers must be serialized, because there are
       components (possibly gcreate() in jtie, possibly in NdbDictionary...) that
       misbehave under concurrent use. After failing in getDomainTypeHandler()
       above, a thread will wait for the lock here, and then check again in the
       map, and, failing again, will create the DomainTypeHandler.

       Some threads might delete an entry from typeToHandlerMap from outside this
       function (and not holding the SessionFactoryImpl intrinsic lock), but no
       thread may insert a value into the map except from this function.
    */
    synchronized <T> DomainTypeHandler<T> createTypeHandlerInMaps(Class<T> cls, Dictionary dictionary) {
        @SuppressWarnings("unchecked")
        DomainTypeHandler<T> domainTypeHandler = (DomainTypeHandler<T>) typeToHandlerMap.get(cls);

        if(domainTypeHandler != null) return domainTypeHandler;

        domainTypeHandler = domainTypeHandlerFactory.createDomainTypeHandler(
            cls, dictionary, smartValueHandlerFactory);
        typeToHandlerMap.put(cls, domainTypeHandler);
        logger.detail(() -> "Created DomainTypeHandler for class " + cls.getName());

        Class<?> proxyClass = domainTypeHandler.getProxyClass();
        if (proxyClass != null) {
            proxyClassToDomainClass.put(proxyClass, cls);
        }

        return domainTypeHandler;
    }

    /** Create or get the DomainTypeHandler for an instance.
     * Use the dictionary to validate against schema.
     * @param object the object
     * @param dictionary the dictionary for metadata access
     * @return the DomainTypeHandler for the object
     */
    <T> DomainTypeHandler<T> getDomainTypeHandler(T object, Dictionary dictionary) {
        Class<T> cls = getClassForProxy(object);
        DomainTypeHandler<T> result = getDomainTypeHandler(cls, dictionary);
        return result;
    }

    @SuppressWarnings("unchecked")
    /** Get the domain class of the given proxy object.
     * @param object the object
     * @return the Domain class of the object
     */
    protected <T> Class<T> getClassForProxy(T object) {
        Class<?> cls = object.getClass();
        if (java.lang.reflect.Proxy.isProxyClass(cls)) {
            cls = proxyClassToDomainClass.get(cls);
        }
        return (Class<T>)cls;
    }

    public Table getTable(String tableName, Dictionary dictionary) {
        return dictionary.getTable(tableName);
    }

    public synchronized void close() {
        // close all of the cluster connections
        for (PooledConnection connection: pooledConnections) {
            connection.close();
        }
        pooledConnections.clear();

        // remove this from the map
        if(! spec.MULTI_DB) {
            synchronized(sessionFactoryMap) {
                sessionFactoryMap.remove(key);
            }
        }
        state = State.Closed;
        GlobalConnectionPool.closeSessionFactory(spec.CONNECT_STRING, this);
    }

    public void setDomainTypeHandlerFactory(DomainTypeHandlerFactory domainTypeHandlerFactory) {
        this.domainTypeHandlerFactory = domainTypeHandlerFactory;
    }

    public DomainTypeHandlerFactory getDomainTypeHandlerFactory() {
        return domainTypeHandlerFactory;
    }

    public List<Integer> getConnectionPoolSessionCounts() {
        List<Integer> result = new ArrayList<Integer>();
        for (PooledConnection connection: pooledConnections) {
            result.add(connection.dbCount());
        }
        return result;
    }

    /*                    == Schema Change Handling ==

       When stale metadata has caused a DomainTypeHandler to become unusable,
       set the DomainTypeHandler to closing, copy it into the schemaLocks map,
       and then use its intrinsic lock to ensure that schema change handling is
       properly serialized. Handling consists of removing cached objects that
       depend on the metadata, purging the metadata from the local dictionary,
       and then reloading fresh metadata over the network.

       getSchemaLock() is only called from unloadSchema().

       After stale-metadata errors (like 241, 284...) the first thread
       to call unloadSchema() creates the schema lock. Other threads in
       getDomainTypeHandler() will see that the TypeHandler is closing and
       throw a "schema change in progress" exception. Subsequent threads
       that call into unloadSchema() and fetch the same schema lock will
       block, waiting for the change to complete. The first thread completes
       the schema change handling, sets the TypeHandler to closed, removes
       it from the main map, and then releases the lock.
    */
    private DomainTypeHandler<?> getSchemaLock(Class<?> cls) {
        synchronized(schemaLocks) {
            DomainTypeHandler<?> handler = schemaLocks.get(cls);

            if(handler == null) {
                handler = typeToHandlerMap.get(cls);
                if(handler == null) {
                    String tableName = getTableNameForClass(cls);
                    if(tableName != null) {
                        for (DomainTypeHandler<?> entry : typeToHandlerMap.values()) {
                            if(entry.getTableName().equals(tableName)) {
                                handler = entry;
                                break;
                            }
                        }
                    }
                }
                if(handler == null) {
                    // The table has yet not been mapped to any class. The first
                    // time here, SessionImpl will create a DomainTypeHandler
                    // by calling newInstance(cls), and then retry.
                    return null;
                }
                logger.debug(() -> "Creating schema lock");
                handler.setClosing();
                schemaLocks.put(cls, handler);
            }

            return handler;
        }
    }

    public String unloadSchema(Class<?> cls, Dictionary dictionary) {
        DomainTypeHandler<?> typeHandler = getSchemaLock(cls);
        if(typeHandler == null)
            return null;

        // Threads wait here, then run the following block one at a time
        synchronized(typeHandler) {
            cls = typeHandler.getDomainClass();
            if(typeHandler.isClosed()) {
                // Some other thread has done the work
                return typeHandler.getTableName();
            }

            // The first thread to get here handles the schema change
            String tableName = typeHandler.getTableName();
            assert tableName != null;
            String oldVer = typeHandler.getTableVersion();

            // The DbFactories will remove cached NdbRecords, and
            // flush the stale table from the global dictionary cache
            for (PooledConnection connection: pooledConnections) {
                connection.unloadSchema(tableName);
            }

            // Also remove the table from the session's local dictionary cache
            dictionary.invalidateTable(tableName);

            // Try to create a new DomainTypeHandler now, to force the metadata
            // to be refreshed over the network; ignore any errors.
            try {
                DomainTypeHandler<?> dummy = domainTypeHandlerFactory
                    .createDomainTypeHandler(cls, dictionary, smartValueHandlerFactory);
                String newVer = dummy.getTableVersion();
                logger.info(() -> "Schema change - replaced DomainTypeHandler for " +
                            tableName + " version " + oldVer + " with version " + newVer);
            } catch (ClusterJDatastoreException ex) {
                logger.info(() -> "Schema change - removed DomainTypeHandler for " +
                            tableName + " version " + oldVer);
            }

            // Remove the old handler from the main map
            typeToHandlerMap.remove(cls);

            // Close the old DomainTypeHandler
            typeHandler.setClosed();

            // Remove the schema lock
            schemaLocks.remove(cls);

            return tableName;
        }
    }

    private <T> String getTableNameForClass(Class<T> cls) {
        String tableName = null;
        if (DynamicObject.class.isAssignableFrom(cls)) {
            try {
                DynamicObject test = (DynamicObject) cls.getDeclaredConstructor().newInstance();
                tableName = test.table();
            } catch (Exception e) {
                logger.warn(local.message("ERR_Create_Instance", cls.toString()));
                return null;
            }
        } else {
            PersistenceCapable persistenceCapable = cls.getAnnotation(PersistenceCapable.class);
            if (persistenceCapable != null) {
                tableName = persistenceCapable.table();
            }
        }
        return tableName;
    }

    /** Shut down the session factory by closing all pooled cluster connections
     * and restarting.
     * @since 7.5.7
     * @param cjde the exception that initiated the reconnection
     */
    public void checkConnection(ClusterJDatastoreException cjde) {
        if (CLUSTER_RECONNECT_TIMEOUT == 0) {
            return;
        } else {
            reconnect(CLUSTER_RECONNECT_TIMEOUT);
        }
    }

    private static void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    /** Get the current state of this session factory.
     * @since 7.5.7
     * @see SessionFactory.State
     */
    public State currentState() {
        return state;
    }

    /** Reconnect this session factory using the default timeout value.
     * @since 7.5.7
     */
    public void reconnect() {
        reconnect(CLUSTER_RECONNECT_TIMEOUT);
    }

    /** Reconnect this session factory using the specified timeout value.
     * @since 7.5.7
     */
    public void reconnect(int timeout) {
        synchronized(this) {
            // if already restarting, do nothing
            if (State.Reconnecting.equals(state)) {
                logger.warn(local.message("WARN_Reconnect_already"));
                return;
            }
            // set the reconnect timeout to the current value
            CLUSTER_RECONNECT_TIMEOUT = timeout;
            if (timeout == 0) {
                logger.warn(local.message("WARN_Reconnect_timeout0"));
                return;
            }
            logger.warn(local.message("WARN_Reconnect", getConnectionPoolSessionCounts().toString()));
            pooledConnections.get(0).reconnect(timeout);
        }
    }

    private static int countSessions(List<Integer> sessionCounts) {
        int result = 0;
        for (int i: sessionCounts) {
            result += i;
        }
        return result;
    }

    int disconnect_check_active_sessions() {
        return countSessions(getConnectionPoolSessionCounts());
    }

    synchronized boolean check_pool_for_disconnect() {
        /* Check whether our own connections will be shutdown */
        if(pooledConnections.get(0).currentState().equals(State.Reconnecting)) {
            state = State.Reconnecting;
            return true;
        }
        return false;
    }

    void disconnect_set_closing() {
        List<Integer> sessionCounts = getConnectionPoolSessionCounts();
        if (countSessions(sessionCounts) != 0)
            logger.warn(local.message("WARN_Reconnect_timeout", sessionCounts.toString()));

        logger.warn(local.message("WARN_Reconnect_closing"));
        for (PooledConnection connection: pooledConnections) {
            connection.setClosing();
        }
    }

    void disconnect_close() {
       for (PooledConnection connection: pooledConnections) {
            connection.close();
        }
    }

    synchronized void do_reconnect() {
        pooledConnections.clear();
        // remove all DomainTypeHandlers, as they embed references to
        // Ndb dictionary objects which have been removed
        typeToHandlerMap.clear();
        schemaLocks.clear();

        logger.warn(local.message("WARN_Reconnect_creating"));
        createClusterConnectionPool();
        verifyConnectionPool();
        logger.warn(local.message("WARN_Reconnect_reopening"));
        state = State.Open;
    }

    public void setRecvThreadCPUids(short[] newCpuId) {
        if (connectionPoolSize != newCpuId.length) {
            throw new ClusterJUserException(
                    local.message("ERR_CPU_Ids_Must_Match_Connection_Pool_Size",
                            Arrays.toString(newCpuId), connectionPoolSize));
        }
        // set cpuid to individual connections in the pool
        short oldCpuId[] = new short[newCpuId.length];
        int i = 0;
        try {
            for (PooledConnection connection: pooledConnections) {
                oldCpuId[i] = connection.getRecvThreadCPUid();
                // No need to bind if the thread is already bound to same cpuid.
                if (newCpuId[i] != oldCpuId[i]) {
                    connection.setRecvThreadCPUid(newCpuId[i]);
                }
                i++;
            }
        } catch (Exception ex) {
            // Binding cpuid failed.
            // To avoid partial settings, restore back the cpu bindings to the old values.
            for (; i >= 0; i--) {
                PooledConnection connection = pooledConnections.get(i);
                if (oldCpuId[i] != newCpuId[i]) {
                    connection.setRecvThreadCPUid(oldCpuId[i]);
                }
            }
            throw ex;
        }
    }

    public short[] getRecvThreadCPUids() {
        short recvThreadCPUids[] = new short[pooledConnections.size()];
        for (int i = 0; i < pooledConnections.size(); i++)
            recvThreadCPUids[i] = pooledConnections.get(i).getRecvThreadCPUid();
        return recvThreadCPUids;
    }

    public void setRecvThreadActivationThreshold(int threshold) {
        if (threshold < 0) {
            // threshold should be a non negative value
            throw new ClusterJUserException(
                    local.message("ERR_Invalid_Activation_Threshold", threshold));
        }
        // any threshold above 15 is interpreted as 256 internally
        CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD = (threshold >= 16)?256:threshold;
        for (PooledConnection connection: pooledConnections) {
            connection.setRecvThreadActivationThreshold(threshold);
        }
    }

    public int getRecvThreadActivationThreshold() {
        return CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD;
    }
}
