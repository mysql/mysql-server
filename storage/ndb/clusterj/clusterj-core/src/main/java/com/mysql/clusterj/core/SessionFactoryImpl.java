/*
   Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.core;

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


    /** Node ids obtained from the property PROPERTY_CONNECTION_POOL_NODEIDS */
    List<Integer> nodeIds = new ArrayList<Integer>();

    /** Connection pool size obtained from the property PROPERTY_CONNECTION_POOL_SIZE */
    int connectionPoolSize;

    /** Map of Proxy to Class */
    // TODO make this non-static
    static private Map<Class<?>, Class<?>> proxyClassToDomainClass = new HashMap<Class<?>, Class<?>>();

    /** Map of Domain Class to DomainTypeHandler. */
    // TODO make this non-static
    static final protected Map<Class<?>, DomainTypeHandler<?>> typeToHandlerMap =
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
                    CLUSTER_CONNECTION_SERVICE);
    }

    /** The smart value handler factory */
    protected ValueHandlerFactory smartValueHandlerFactory;

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
        createClusterConnectionPool();
        // now get a Session and complete a transaction to make sure that the cluster is ready
        try {
            Session session = getSession(null);
            session.currentTransaction().begin();
            session.currentTransaction().commit();
            session.close();
        } catch (Exception e) {
            if (e instanceof ClusterJException) {
                logger.warn(local.message("ERR_Session_Factory_Impl_Failed_To_Complete_Transaction"));
                throw (ClusterJException)e;
            }
        }
    }

    protected void createClusterConnectionPool() {
        String nodeIdsProperty = getStringProperty(props, PROPERTY_CONNECTION_POOL_NODEIDS);
        if (nodeIdsProperty != null) {
            // separators are any combination of white space, commas, and semicolons
            String[] nodeIdsStringArray = nodeIdsProperty.split("[,; \t\n\r]+", 48);
            for (String nodeIdString : nodeIdsStringArray) {
                try {
                    int nodeId = Integer.parseInt(nodeIdString);
                    nodeIds.add(nodeId);
                } catch (NumberFormatException ex) {
                    throw new ClusterJFatalUserException(local.message("ERR_Node_Ids_Format", nodeIdsProperty), ex);
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
                    throw new ClusterJFatalUserException(
                            local.message("ERR_Node_Ids_Must_Match_Connection_Pool_Size",
                                    nodeIdsProperty, connectionPoolSize));
                    
                }
            } else {
                // only node ids are specified; make pool size match number of node ids
                connectionPoolSize = nodeIds.size();
            }
        }
        ClusterConnectionService service = getClusterConnectionService();
        if (nodeIds.size() == 0) {
            // node ids were not specified
            for (int i = 0; i < connectionPoolSize; ++i) {
                createClusterConnection(service, props, 0);
            }
        } else {
            for (int i = 0; i < connectionPoolSize; ++i) {
                createClusterConnection(service, props, nodeIds.get(i));
            }
        }
        // get the smart value handler factory for this connection; it will be the same for all connections
        if (pooledConnections.size() != 0) {
            smartValueHandlerFactory = pooledConnections.get(0).getSmartValueHandlerFactory();
        }
    }

    protected ClusterConnection createClusterConnection(
            ClusterConnectionService service, Map<?, ?> props, int nodeId) {
        int[] byteBufferPoolSizes = getByteBufferPoolSizes(props);
        ClusterConnection result = null;
        try {
            result = service.create(CLUSTER_CONNECT_STRING, nodeId, CLUSTER_CONNECT_TIMEOUT_MGM);
            result.setByteBufferPoolSizes(CLUSTER_BYTE_BUFFER_POOL_SIZES);
            result.connect(CLUSTER_CONNECT_RETRIES, CLUSTER_CONNECT_DELAY,true);
            result.waitUntilReady(CLUSTER_CONNECT_TIMEOUT_BEFORE,CLUSTER_CONNECT_TIMEOUT_AFTER);
        } catch (Exception ex) {
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
        return getSession(null);
    }

    /** Get a session to use with the cluster, overriding some properties.
     * Properties PROPERTY_CLUSTER_CONNECTSTRING, PROPERTY_CLUSTER_DATABASE,
     * and PROPERTY_CLUSTER_MAX_TRANSACTIONS may not be overridden.
     * @param properties overriding some properties for this session
     * @return the session
     */
    public Session getSession(Map properties) {
        ClusterConnection clusterConnection = getClusterConnectionFromPool();
        try {
            Db db = null;
            synchronized(this) {
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
        if (connectionPoolSize <= 1) {
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
    public static <T> DomainTypeHandler<T> getDomainTypeHandler(Class<T> cls) {
        // synchronize here because the map is not synchronized
        synchronized(typeToHandlerMap) {
            @SuppressWarnings( "unchecked" )
            DomainTypeHandler<T> domainTypeHandler = (DomainTypeHandler<T>) typeToHandlerMap.get(cls);
            return domainTypeHandler;
        }
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
                Class<?> proxyClass = domainTypeHandler.getProxyClass();
                if (proxyClass != null) {
                    proxyClassToDomainClass.put(proxyClass, cls);
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
        DomainTypeHandler<T> result = getDomainTypeHandler(cls);
        if (result != null) {
            return result;
        } else {
            return getDomainTypeHandler(cls, dictionary);
        }
    }

    @SuppressWarnings("unchecked")
    protected static <T> Class<T> getClassForProxy(T object) {
        Class cls = object.getClass();
        if (java.lang.reflect.Proxy.isProxyClass(cls)) {
            cls = proxyClassToDomainClass.get(cls);
        }
        return cls;        
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
        // we have to close all of the cluster connections
        for (ClusterConnection clusterConnection: pooledConnections) {
            clusterConnection.close();
        }
        pooledConnections.clear();
        synchronized(sessionFactoryMap) {
            // now remove this from the map
            sessionFactoryMap.remove(key);
        }
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
                }
            }
            for (ClusterConnection clusterConnection: pooledConnections) {
                clusterConnection.unloadSchema(tableName);
            }
            return tableName;
        }
    }

}
