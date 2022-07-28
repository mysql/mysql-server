/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

package com.mysql.clusterj.core;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandlerFactory;
import com.mysql.clusterj.core.spi.ValueHandlerFactory;
import com.mysql.clusterj.core.metadata.DomainTypeHandlerFactoryImpl;

import com.mysql.clusterj.core.store.Db;
import com.mysql.clusterj.core.store.ClusterConnection;
import com.mysql.clusterj.core.store.ClusterConnectionService;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class SessionFactoryImpl implements SessionFactory, Constants {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(SessionFactoryImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(SessionFactoryImpl.class);

    /** My class loader */
    static final ClassLoader SESSION_FACTORY_IMPL_CLASS_LOADER = SessionFactoryImpl.class.getClassLoader();

    /** The status of this session factory */
    protected State state;

    /** The properties */
    protected Map<?, ?> props;

    /** NdbCluster connect properties */
    String CLUSTER_CONNECTION_SERVICE;
    String CLUSTER_CONNECT_STRING;
    int CLUSTER_CONNECT_TIMEOUT_MGM;
    int CLUSTER_CONNECT_RETRIES;
    int CLUSTER_CONNECT_DELAY;
    int CLUSTER_CONNECT_VERBOSE;
    int CLUSTER_CONNECT_TIMEOUT_BEFORE;
    int CLUSTER_CONNECT_TIMEOUT_AFTER;
    String CLUSTER_DATABASE;
    int CLUSTER_MAX_TRANSACTIONS;
    int CLUSTER_CONNECT_AUTO_INCREMENT_BATCH_SIZE;
    long CLUSTER_CONNECT_AUTO_INCREMENT_STEP;
    long CLUSTER_CONNECT_AUTO_INCREMENT_START;
    int[] CLUSTER_BYTE_BUFFER_POOL_SIZES;
    int CLUSTER_RECONNECT_TIMEOUT;
    int CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD;


    /** Node ids obtained from the property PROPERTY_CONNECTION_POOL_NODEIDS */
    List<Integer> nodeIds = new ArrayList<Integer>();

    /** Connection pool size obtained from the property PROPERTY_CONNECTION_POOL_SIZE */
    int connectionPoolSize;

    /** Boolean flag indicating if connection pool is disabled or not */
    boolean connectionPoolDisabled = false;

    /** Map of Proxy Interfaces to Domain Class */
    final private Map<String, Class<?>> proxyInterfacesToDomainClassMap = new HashMap<String, Class<?>>();

    /** Map of Domain Class to DomainTypeHandler. */
    final private Map<Class<?>, DomainTypeHandler<?>> typeToHandlerMap =
            new HashMap<Class<?>, DomainTypeHandler<?>>();

    /** DomainTypeHandlerFactory for this session factory. */
    DomainTypeHandlerFactory domainTypeHandlerFactory = new DomainTypeHandlerFactoryImpl();

    /** The session factories. */
    static final protected Map<String, SessionFactoryImpl> sessionFactoryMap =
            new HashMap<String, SessionFactoryImpl>();

    /** The key for this factory */
    final String key;

    /** Cluster connections that together can be used to manage sessions */
    private List<ClusterConnection> pooledConnections = new ArrayList<ClusterConnection>();

    /** Get a cluster connection service.
     * @return the cluster connection service
     */
    protected ClusterConnectionService getClusterConnectionService() {
        return ClusterJHelper.getServiceInstance(ClusterConnectionService.class,
                    CLUSTER_CONNECTION_SERVICE, SESSION_FACTORY_IMPL_CLASS_LOADER);
    }

    /** The smart value handler factory */
    protected ValueHandlerFactory smartValueHandlerFactory;

    /** The cpuids to which the receive threads of the connections in the connection pools are locked */
    short[] recvThreadCPUids;

    /** Get a session factory. If using connection pooling and there is already a session factory
     * with the same connect string and database, return it, regardless of whether other
     * properties of the factory are the same as specified in the Map.
     * If not using connection pooling (maximum sessions per connection == 0), create a new session factory.
     * @param props properties of the session factory
     * @return the session factory
     */
    static public SessionFactoryImpl getSessionFactory(Map<?, ?> props) {
        int connectionPoolSize = getIntProperty(props, 
                PROPERTY_CONNECTION_POOL_SIZE, DEFAULT_PROPERTY_CONNECTION_POOL_SIZE);
        String sessionFactoryKey = getSessionFactoryKey(props);
        SessionFactoryImpl result = null;
        if (connectionPoolSize != 0) {
            // if using connection pooling, see if already a session factory created
            synchronized(sessionFactoryMap) {
                result = sessionFactoryMap.get(sessionFactoryKey);
                if (result == null) {
                    result = new SessionFactoryImpl(props);
                    sessionFactoryMap.put(sessionFactoryKey, result);
                }
            }
        } else {
            // if not using connection pooling, create a new session factory
            result = new SessionFactoryImpl(props);
        }
        return result;
    }

    private static String getSessionFactoryKey(Map<?, ?> props) {
        String clusterConnectString = 
            getRequiredStringProperty(props, PROPERTY_CLUSTER_CONNECTSTRING);
        String clusterDatabase = getStringProperty(props, PROPERTY_CLUSTER_DATABASE,
                Constants.DEFAULT_PROPERTY_CLUSTER_DATABASE);
        return clusterConnectString + "+" + clusterDatabase;
    }

    /** Create a new SessionFactoryImpl from the properties in the Map, and
     * connect to the ndb cluster.
     *
     * @param props the properties for the factory
     */
    protected SessionFactoryImpl(Map<?, ?> props) {
        this.props = props;
        this.key = getSessionFactoryKey(props);
        this.connectionPoolSize = getIntProperty(props, 
                PROPERTY_CONNECTION_POOL_SIZE, DEFAULT_PROPERTY_CONNECTION_POOL_SIZE);
        if (connectionPoolSize == 0) {
            // Connection pool is disabled. This is handled internally almost
            // same as a SessionFactory with a connection pool of size 1.
            connectionPoolSize = 1;
            connectionPoolDisabled = true;
        }
        CLUSTER_RECONNECT_TIMEOUT = getIntProperty(props,
                PROPERTY_CONNECTION_RECONNECT_TIMEOUT, DEFAULT_PROPERTY_CONNECTION_RECONNECT_TIMEOUT);
        CLUSTER_CONNECT_STRING = getRequiredStringProperty(props, PROPERTY_CLUSTER_CONNECTSTRING);
        CLUSTER_CONNECT_RETRIES = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_RETRIES,
                Constants.DEFAULT_PROPERTY_CLUSTER_CONNECT_RETRIES);
        CLUSTER_CONNECT_TIMEOUT_MGM = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_TIMEOUT_MGM,
                Constants.DEFAULT_PROPERTY_CLUSTER_CONNECT_TIMEOUT_MGM);
        CLUSTER_CONNECT_DELAY = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_DELAY,
                Constants.DEFAULT_PROPERTY_CLUSTER_CONNECT_DELAY);
        CLUSTER_CONNECT_VERBOSE = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_VERBOSE,
                Constants.DEFAULT_PROPERTY_CLUSTER_CONNECT_VERBOSE);
        CLUSTER_CONNECT_TIMEOUT_BEFORE = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_TIMEOUT_BEFORE,
                Constants.DEFAULT_PROPERTY_CLUSTER_CONNECT_TIMEOUT_BEFORE);
        CLUSTER_CONNECT_TIMEOUT_AFTER = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_TIMEOUT_AFTER,
                Constants.DEFAULT_PROPERTY_CLUSTER_CONNECT_TIMEOUT_AFTER);
        CLUSTER_DATABASE = getStringProperty(props, PROPERTY_CLUSTER_DATABASE,
                Constants.DEFAULT_PROPERTY_CLUSTER_DATABASE);
        CLUSTER_MAX_TRANSACTIONS = getIntProperty(props, PROPERTY_CLUSTER_MAX_TRANSACTIONS,
                Constants.DEFAULT_PROPERTY_CLUSTER_MAX_TRANSACTIONS);
        CLUSTER_CONNECT_AUTO_INCREMENT_BATCH_SIZE = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_BATCH_SIZE,
                Constants.DEFAULT_PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_BATCH_SIZE);
        CLUSTER_CONNECT_AUTO_INCREMENT_STEP = getLongProperty(props, PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_STEP,
                Constants.DEFAULT_PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_STEP);
        CLUSTER_CONNECT_AUTO_INCREMENT_START = getLongProperty(props, PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_START,
                Constants.DEFAULT_PROPERTY_CLUSTER_CONNECT_AUTO_INCREMENT_START);
        CLUSTER_CONNECTION_SERVICE = getStringProperty(props, PROPERTY_CLUSTER_CONNECTION_SERVICE);
        CLUSTER_BYTE_BUFFER_POOL_SIZES = getByteBufferPoolSizes(props);
        CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD = getIntProperty(props, PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD,
                Constants.DEFAULT_PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD);
        if (CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD < 0) {
            // threshold should be non negative
            String msg = local.message("ERR_Invalid_Activation_Threshold",
                    CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD);
            logger.warn(msg);
            throw new ClusterJFatalUserException(msg);
        }
        createClusterConnectionPool();
        // now get a Session for each connection in the pool and
        // complete a transaction to make sure that each connection is ready
        verifyConnectionPool();
        state = State.Open;
    }

    protected void createClusterConnectionPool() {
        String msg;
        String nodeIdsProperty = getStringProperty(props, PROPERTY_CONNECTION_POOL_NODEIDS);
        if (nodeIdsProperty != null) {
            // separators are any combination of white space, commas, and semicolons
            String[] nodeIdsStringArray = nodeIdsProperty.split("[,; \t\n\r]+", 48);
            for (String nodeIdString : nodeIdsStringArray) {
                try {
                    int nodeId = Integer.parseInt(nodeIdString);
                    nodeIds.add(nodeId);
                } catch (NumberFormatException ex) {
                    msg = local.message("ERR_Node_Ids_Format", nodeIdsProperty);
                    logger.warn(msg);
                    throw new ClusterJFatalUserException(msg, ex);
                }
            }
            // validate the size of the node ids with the connection pool size
            if (connectionPoolSize != DEFAULT_PROPERTY_CONNECTION_POOL_SIZE) {
                // both are specified; they must match or nodeIds size must be 1
                if (nodeIds.size() ==1) {
                    // add new nodeIds to fill out array
                    for (int i = 1; i < connectionPoolSize; ++i) {
                        nodeIds.add(nodeIds.get(i - 1) + 1);
                    }
                }
                if (connectionPoolSize != nodeIds.size()) {
                    msg = local.message("ERR_Node_Ids_Must_Match_Connection_Pool_Size",
                            nodeIdsProperty, connectionPoolSize);
                    logger.warn(msg);
                    throw new ClusterJFatalUserException(msg);
                }
            } else if (connectionPoolDisabled) {
                if (nodeIds.size() != 1) {
                    // Connection pool is disabled but more than one nodeId specified
                    msg = local.message("ERR_Multiple_Node_Ids_For_Disabled_Connection_Pool",
                            nodeIdsProperty);
                    logger.warn(msg);
                    throw new ClusterJFatalUserException(msg);
                }
            } else {
                // only node ids are specified; make pool size match number of node ids
                connectionPoolSize = nodeIds.size();
            }
        }
        // parse and read the cpu ids given for binding with the recv threads
        recvThreadCPUids = new short[connectionPoolSize];
        String cpuIdsProperty = getStringProperty(props, PROPERTY_CONNECTION_POOL_RECV_THREAD_CPUIDS);
        if (cpuIdsProperty != null) {
            // separators are any combination of white space, commas, and semicolons
            String[] cpuIdsStringArray = cpuIdsProperty.split("[,; \t\n\r]+", 64);
            if (cpuIdsStringArray.length != connectionPoolSize) {
                // cpu ids property didn't match connection pool size
                if (connectionPoolDisabled) {
                    msg = local.message("ERR_Multiple_CPU_Ids_For_Disabled_Connection_Pool",
                        cpuIdsProperty);
                } else {
                    msg = local.message("ERR_CPU_Ids_Must_Match_Connection_Pool_Size",
                        cpuIdsProperty, connectionPoolSize);
                }
                logger.warn(msg);
                throw new ClusterJFatalUserException(msg);
            }
            int i = 0;
            for (String cpuIdString : cpuIdsStringArray) {
                try {
                    recvThreadCPUids[i++] = Short.parseShort(cpuIdString);
                } catch (NumberFormatException ex) {
                    msg = local.message("ERR_CPU_Ids_Format", cpuIdsProperty);
                    logger.warn(msg);
                    throw new ClusterJFatalUserException(msg, ex);
                }
            }
        } else {
            // cpuids not present. fill in the default value -1
            for (int i = 0; i < connectionPoolSize; i++) {
                recvThreadCPUids[i] = -1;
            }
        }
        ClusterConnectionService service = getClusterConnectionService();
        if (nodeIds.size() == 0) {
            // node ids were not specified
            for (int i = 0; i < connectionPoolSize; ++i) {
                createClusterConnection(service, props, 0, i);
            }
        } else {
            for (int i = 0; i < connectionPoolSize; ++i) {
                createClusterConnection(service, props, nodeIds.get(i), i);
            }
        }
        // get the smart value handler factory for this connection; it will be the same for all connections
        if (pooledConnections.size() != 0) {
            smartValueHandlerFactory = pooledConnections.get(0).getSmartValueHandlerFactory();
        }
    }

    protected void verifyConnectionPool() {
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

    protected ClusterConnection createClusterConnection(
            ClusterConnectionService service, Map<?, ?> props, int nodeId, int connectionId) {
        ClusterConnection result = null;
        boolean connected = false;
        try {
            result = service.create(CLUSTER_CONNECT_STRING, nodeId, CLUSTER_CONNECT_TIMEOUT_MGM);
            result.setByteBufferPoolSizes(CLUSTER_BYTE_BUFFER_POOL_SIZES);
            result.connect(CLUSTER_CONNECT_RETRIES, CLUSTER_CONNECT_DELAY,true);
            result.waitUntilReady(CLUSTER_CONNECT_TIMEOUT_BEFORE,CLUSTER_CONNECT_TIMEOUT_AFTER);
            // Cluster connection successful.
            // The connection has to be closed if the method fails after this point.
            connected = true;
            if (CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD !=
                    DEFAULT_PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD) {
                // set the activation threshold iff the value passed is not default
                result.setRecvThreadActivationThreshold(CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD);
            }
            // bind the connection's recv thread to cpu if the cpuid is passed in the property.
            if (recvThreadCPUids[connectionId] != -1) {
                result.setRecvThreadCPUid(recvThreadCPUids[connectionId]);
            }
        } catch (Exception ex) {
            // close result if it has connected already
            if (connected) {
                result.closing();
                result.close();
            }
            // need to clean up if some connections succeeded
            for (ClusterConnection connection: pooledConnections) {
                connection.close();
            }
            pooledConnections.clear();
            throw new ClusterJFatalUserException(
                    local.message("ERR_Connecting", props), ex);
        }
        this.pooledConnections.add(result);
        result.initializeAutoIncrement(new long[] {
                CLUSTER_CONNECT_AUTO_INCREMENT_BATCH_SIZE,
                CLUSTER_CONNECT_AUTO_INCREMENT_STEP,
                CLUSTER_CONNECT_AUTO_INCREMENT_START
        });
        return result;
    }

    /** Get the byteBufferPoolSizes from properties */
    int[] getByteBufferPoolSizes(Map<?, ?> props) {
        int[] result;
        String byteBufferPoolSizesProperty = getStringProperty(props, PROPERTY_CLUSTER_BYTE_BUFFER_POOL_SIZES,
                DEFAULT_PROPERTY_CLUSTER_BYTE_BUFFER_POOL_SIZES);
        // separators are any combination of white space, commas, and semicolons
        String[] byteBufferPoolSizesList = byteBufferPoolSizesProperty.split("[,; \t\n\r]+", 48);
        int count = byteBufferPoolSizesList.length;
        result = new int[count];
        for (int i = 0; i < count; ++i) {
            try {
                result[i] = Integer.parseInt(byteBufferPoolSizesList[i]);
            } catch (NumberFormatException ex) {
                throw new ClusterJFatalUserException(local.message(
                        "ERR_Byte_Buffer_Pool_Sizes_Format", byteBufferPoolSizesProperty), ex);
            }
        }
        return result;
    }

    /** Get a session to use with the cluster.
     *
     * @return the session
     */
    public Session getSession() {
        return getSession(null, false);
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
            synchronized(this) {
                if (!(State.Open.equals(state)) && !internal) {
                    throw new ClusterJUserException(local.message("ERR_SessionFactory_not_open"));
                }
                ClusterConnection clusterConnection = getClusterConnectionFromPool();
                checkConnection(clusterConnection);
                db = clusterConnection.createDb(CLUSTER_DATABASE, CLUSTER_MAX_TRANSACTIONS);
            }
            Dictionary dictionary = db.getDictionary();
            return new SessionImpl(this, properties, db, dictionary);
        } catch (ClusterJException ex) {
            throw ex;
        } catch (Exception ex) {
            throw new ClusterJFatalException(
                    local.message("ERR_Create_Ndb"), ex);
        }
    }

    private ClusterConnection getClusterConnectionFromPool() {
        if (connectionPoolSize == 1) {
            return pooledConnections.get(0);
        }
        // find the best pooled connection (the connection with the least active sessions)
        // this is not perfect without synchronization since a connection might close sessions
        // after getting the dbCount but we don't care about perfection here. 
        ClusterConnection result = null;
        int bestCount = Integer.MAX_VALUE;
        for (ClusterConnection pooledConnection: pooledConnections ) {
            int count = pooledConnection.dbCount();
            if (count < bestCount) {
                bestCount = count;
                result = pooledConnection;
            }
        }
        return result;
    }

    private void checkConnection(ClusterConnection clusterConnection) {
        if (clusterConnection == null) {
            throw new ClusterJUserException(local.message("ERR_Session_Factory_Closed"));
        }
    }

    /** Get the DomainTypeHandler for a class. If the handler is not already
     * available, null is returned. 
     * @param cls the Class for which to get domain type handler
     * @return the DomainTypeHandler or null if not available
     */
    public <T> DomainTypeHandler<T> getDomainTypeHandler(Class<T> cls) {
        // synchronize here because the map is not synchronized
        synchronized(typeToHandlerMap) {
            @SuppressWarnings( "unchecked" )
            DomainTypeHandler<T> domainTypeHandler = (DomainTypeHandler<T>) typeToHandlerMap.get(cls);
            return domainTypeHandler;
        }
    }

    /** Generate a key from the given interfaces to lookup the proxyInterfacesToDomainClass map
     * @param proxyInterfaces List of proxy interfaces to generate the key from
     * @return the generated lookup key
     */
    private static String generateProxyInterfacesKey(Class<?>[] proxyInterfaces) {
        // The generated key is of form : CanonicalNameOfInterface1;CanonicalNameOfInterface2;...
        StringBuilder key = new StringBuilder();
        for (Class<?> proxyInterface : proxyInterfaces) {
            key.append(proxyInterface.getCanonicalName()).append(';');
        }
        return key.toString();
    }

    /** Create or get the DomainTypeHandler for a class.
     * Use the dictionary to validate against schema.
     * @param cls the Class for which to get domain type handler
     * @param dictionary the dictionary to validate against
     * @return the type handler
     */
    
    public <T> DomainTypeHandler<T> getDomainTypeHandler(Class<T> cls, Dictionary dictionary) {
        // synchronize here because the map is not synchronized
        synchronized(typeToHandlerMap) {
            @SuppressWarnings("unchecked")
            DomainTypeHandler<T> domainTypeHandler = (DomainTypeHandler<T>) typeToHandlerMap.get(cls);
            if (logger.isDetailEnabled()) logger.detail("DomainTypeToHandler for "
                    + cls.getName() + "(" + cls
                    + ") returned " + domainTypeHandler);
            if (domainTypeHandler == null) {
                domainTypeHandler = domainTypeHandlerFactory.createDomainTypeHandler(cls,
                        dictionary, smartValueHandlerFactory);
                if (logger.isDetailEnabled()) logger.detail("createDomainTypeHandler for "
                        + cls.getName() + "(" + cls
                        + ") returned " + domainTypeHandler);
                typeToHandlerMap.put(cls, domainTypeHandler);
                Class<?>[] proxyInterfaces = domainTypeHandler.getProxyInterfaces();
                if (proxyInterfaces != null) {
                    String key = generateProxyInterfacesKey(proxyInterfaces);
                    proxyInterfacesToDomainClassMap.put(key, cls);
                }
            }
            return domainTypeHandler;
        }
    }

    /** Create or get the DomainTypeHandler for an instance.
     * Use the dictionary to validate against schema.
     * @param object the object
     * @param dictionary the dictionary for metadata access
     * @return the DomainTypeHandler for the object
     */
    public <T> DomainTypeHandler<T> getDomainTypeHandler(T object, Dictionary dictionary) {
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
            // The underlying class is a Proxy. Retrieve the interfaces implemented
            // by the proxy class and use them to fetch the domain class from
            // proxyInterfacesToDomainClass map.
            Class<?>[] proxyInterfaces = cls.getInterfaces();
            String key = generateProxyInterfacesKey(proxyInterfaces);
            cls = proxyInterfacesToDomainClassMap.get(key);
        }
        return (Class<T>)cls;
    }

    public <T> T newInstance(Class<T> cls, Dictionary dictionary, Db db) {
        DomainTypeHandler<T> domainTypeHandler = getDomainTypeHandler(cls, dictionary);
        return domainTypeHandler.newInstance(db);
    }

    public Table getTable(String tableName, Dictionary dictionary) {
        Table result;
        try {
            result = dictionary.getTable(tableName);
        } catch(Exception ex) {
            throw new ClusterJFatalInternalException(
                        local.message("ERR_Get_Table"), ex);
        }
        return result;
    }

    /** Get the property from the properties map as a String.
     * @param props the properties
     * @param propertyName the name of the property
     * @return the value from the properties (may be null)
     */
    protected static String getStringProperty(Map<?, ?> props, String propertyName) {
        return (String)props.get(propertyName);
    }

    /** Get the property from the properties map as a String. If the user has not
     * provided a value in the props, use the supplied default value.
     * @param props the properties
     * @param propertyName the name of the property
     * @param defaultValue the value to return if there is no property by that name
     * @return the value from the properties or the default value
     */
    protected static String getStringProperty(Map<?, ?> props, String propertyName, String defaultValue) {
        String result = (String)props.get(propertyName);
        if (result == null) {
            result = defaultValue;
        }
        return result;
    }

    /** Get the property from the properties map as a String. If the user has not
     * provided a value in the props, throw an exception.
     * @param props the properties
     * @param propertyName the name of the property
     * @return the value from the properties (may not be null)
     */
    protected static String getRequiredStringProperty(Map<?, ?> props, String propertyName) {
        String result = (String)props.get(propertyName);
        if (result == null) {
                throw new ClusterJFatalUserException(
                        local.message("ERR_NullProperty", propertyName));                            
        }
        return result;
    }

    /** Get the property from the properties map as an int. If the user has not
     * provided a value in the props, use the supplied default value.
     * @param props the properties
     * @param propertyName the name of the property
     * @param defaultValue the value to return if there is no property by that name
     * @return the value from the properties or the default value
     */
    protected static int getIntProperty(Map<?, ?> props, String propertyName, int defaultValue) {
        Object property = props.get(propertyName);
        if (property == null) {
            return defaultValue;
        }
        if (Number.class.isAssignableFrom(property.getClass())) {
            return ((Number)property).intValue();
        }
        if (property instanceof String) {
            try {
                int result = Integer.parseInt((String)property);
                return result;
            } catch (NumberFormatException ex) {
                throw new ClusterJFatalUserException(
                        local.message("ERR_NumericFormat", propertyName, property));
            }
        }
        throw new ClusterJUserException(local.message("ERR_NumericFormat", propertyName, property));
    }

    /** Get the property from the properties map as a long. If the user has not
     * provided a value in the props, use the supplied default value.
     * @param props the properties
     * @param propertyName the name of the property
     * @param defaultValue the value to return if there is no property by that name
     * @return the value from the properties or the default value
     */
    protected static long getLongProperty(Map<?, ?> props, String propertyName, long defaultValue) {
        Object property = props.get(propertyName);
        if (property == null) {
            return defaultValue;
        }
        if (Number.class.isAssignableFrom(property.getClass())) {
            return ((Number)property).longValue();
        }
        if (property instanceof String) {
            try {
                long result = Long.parseLong((String)property);
                return result;
            } catch (NumberFormatException ex) {
                throw new ClusterJFatalUserException(
                        local.message("ERR_NumericFormat", propertyName, property));
            }
        }
        throw new ClusterJUserException(local.message("ERR_NumericFormat", propertyName, property));
    }

    public synchronized void close() {
        // close all of the cluster connections
        for (ClusterConnection clusterConnection: pooledConnections) {
            clusterConnection.close();
        }
        pooledConnections.clear();
        synchronized(sessionFactoryMap) {
            // now remove this from the map
            sessionFactoryMap.remove(key);
        }
        state = State.Closed;
    }

    public void setDomainTypeHandlerFactory(DomainTypeHandlerFactory domainTypeHandlerFactory) {
        this.domainTypeHandlerFactory = domainTypeHandlerFactory;
    }

    public DomainTypeHandlerFactory getDomainTypeHandlerFactory() {
        return domainTypeHandlerFactory;
    }

    public List<Integer> getConnectionPoolSessionCounts() {
        List<Integer> result = new ArrayList<Integer>();
        for (ClusterConnection connection: pooledConnections) {
            result.add(connection.dbCount());
        }
        return result;
    }

    public String unloadSchema(Class<?> cls, Dictionary dictionary) {
        synchronized(typeToHandlerMap) {
            String tableName = null;
            DomainTypeHandler<?> domainTypeHandler = typeToHandlerMap.remove(cls);
            if (domainTypeHandler != null) {
                // remove the ndb dictionary cached table definition
                tableName = domainTypeHandler.getTableName();
                if (tableName != null) {
                    if (logger.isDebugEnabled())logger.debug("Removing dictionary entry for table " + tableName
                            + " for class " + cls.getName());
                    dictionary.removeCachedTable(tableName);
                    for (ClusterConnection clusterConnection: pooledConnections) {
                        clusterConnection.unloadSchema(tableName);
                    }
                }
            }
            return tableName;
        }
    }
    protected ThreadGroup threadGroup = new ThreadGroup("Reconnect");

    protected Thread reconnectThread;

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
        logger.warn(local.message("WARN_Reconnect", getConnectionPoolSessionCounts().toString()));
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
            // set the state of this session factory to reconnecting
            state = State.Reconnecting;
            // create a thread to manage the reconnect operation
            // create thread group
            threadGroup = new ThreadGroup("Stuff");
            // create reconnect thread
            reconnectThread = new Thread(threadGroup, new ReconnectThread(this));
            reconnectThread.start();
            logger.warn(local.message("WARN_Reconnect_started"));
        }
    }

    protected static int countSessions(SessionFactoryImpl factory) {
        return countSessions(factory.getConnectionPoolSessionCounts());
    }

    protected static int countSessions(List<Integer> sessionCounts) {
        int result = 0;
        for (int i: sessionCounts) {
            result += i;
        }
        return result;
    }

    protected static class ReconnectThread implements Runnable {
        SessionFactoryImpl factory;
        ReconnectThread(SessionFactoryImpl factory) {
            this.factory = factory;
        }
        public void run() {
            List<Integer> sessionCounts = factory.getConnectionPoolSessionCounts();
            boolean done = false;
            int iterations = factory.CLUSTER_RECONNECT_TIMEOUT;
            while (!done && iterations-- > 0) {
                done = countSessions(sessionCounts) == 0;
                if (!done) {
                    logger.info(local.message("INFO_Reconnect_wait", sessionCounts.toString()));
                    sleep(1000);
                    sessionCounts = factory.getConnectionPoolSessionCounts();
                }
            }
            if (!done) {
                // timed out waiting for sessions to close
                logger.warn(local.message("WARN_Reconnect_timeout", sessionCounts.toString()));
            }
            logger.warn(local.message("WARN_Reconnect_closing"));
            // mark all cluster connections as closing
            for (ClusterConnection clusterConnection: factory.pooledConnections) {
                clusterConnection.closing();
            }
            // wait for connections to close on their own
            sleep(1000);
            // hard close connections that didn't close on their own
            for (ClusterConnection clusterConnection: factory.pooledConnections) {
                clusterConnection.close();
            }
            factory.pooledConnections.clear();
            // remove all DomainTypeHandlers, as they embed references to
            // Ndb dictionary objects which have been removed
            factory.typeToHandlerMap.clear();
            factory.proxyInterfacesToDomainClassMap.clear();

            logger.warn(local.message("WARN_Reconnect_creating"));
            factory.createClusterConnectionPool();
            factory.verifyConnectionPool();
            logger.warn(local.message("WARN_Reconnect_reopening"));
            synchronized(factory) {
                factory.state = State.Open;
            }
        }
    }

    public void setRecvThreadCPUids(short[] cpuids) {
        // validate the size of the node ids with the connection pool size
        if (connectionPoolSize != cpuids.length) {
            throw new ClusterJUserException(
                    local.message("ERR_CPU_Ids_Must_Match_Connection_Pool_Size",
                            Arrays.toString(cpuids), connectionPoolSize));
        }
        // set cpuid to individual connections in the pool
        short newRecvThreadCPUids[] = new short[cpuids.length];
        try {
            int i = 0;
            for (ClusterConnection clusterConnection: pooledConnections) {
                // No need to bind if the thread is already bound to same cpuid.
                if (cpuids[i] != recvThreadCPUids[i]){
                    if (cpuids[i] != -1) {
                        clusterConnection.setRecvThreadCPUid(cpuids[i]);
                    } else {
                        // cpu id is -1 which is a request for unlocking the thread from cpu
                        clusterConnection.unsetRecvThreadCPUid();
                    }
                }
                newRecvThreadCPUids[i] = cpuids[i];
                i++;
            }
            // binding success
            recvThreadCPUids = newRecvThreadCPUids;
        } catch (Exception ex) {
            // Binding cpuid failed.
            // To avoid partial settings, restore back the cpu bindings to the old values.
            for (int i = 0; newRecvThreadCPUids[i] != 0 && i < newRecvThreadCPUids.length; i++) {
                ClusterConnection clusterConnection = pooledConnections.get(i);
                if (recvThreadCPUids[i] != newRecvThreadCPUids[i]) {
                    if (recvThreadCPUids[i] == -1) {
                        clusterConnection.unsetRecvThreadCPUid();
                    } else {
                        clusterConnection.setRecvThreadCPUid(recvThreadCPUids[i]);
                    }
                }
            }
            throw ex;
        }
    }

    public short[] getRecvThreadCPUids() {
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
        for (ClusterConnection clusterConnection: pooledConnections) {
            clusterConnection.setRecvThreadActivationThreshold(threshold);
        }
    }

    public int getRecvThreadActivationThreshold() {
        return CLUSTER_RECV_THREAD_ACTIVATION_THRESHOLD;
    }
}
