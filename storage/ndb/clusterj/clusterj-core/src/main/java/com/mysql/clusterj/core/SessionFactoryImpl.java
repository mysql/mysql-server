/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.clusterj.core;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;

import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandlerFactory;
import com.mysql.clusterj.core.metadata.DomainTypeHandlerFactoryImpl;

import com.mysql.clusterj.core.store.Db;
import com.mysql.clusterj.core.store.ClusterConnection;
import com.mysql.clusterj.core.store.ClusterConnectionService;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.util.HashMap;
import java.util.Map;

public class SessionFactoryImpl implements SessionFactory, Constants {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(SessionFactoryImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(SessionFactoryImpl.class);

    /** The properties */
    protected Map props;

    /** NdbCluster connect properties */
    String CLUSTER_CONNECTION_SERVICE;
    String CLUSTER_CONNECT_STRING;
    int CLUSTER_CONNECT_RETRIES;
    int CLUSTER_CONNECT_DELAY;
    int CLUSTER_CONNECT_VERBOSE;
    int CLUSTER_CONNECT_TIMEOUT_BEFORE;
    int CLUSTER_CONNECT_TIMEOUT_AFTER;
    String CLUSTER_DATABASE;
    int CLUSTER_MAX_TRANSACTIONS;

    /** Ndb_cluster_connection: one per factory. */
    protected ClusterConnection clusterConnection;

    /** Map of Proxy to Class */
    // TODO make this non-static
    static private Map<Class, Class> proxyClassToDomainClass = new HashMap<Class, Class>();

    /** Map of Domain Class to DomainTypeHandler. */
    // TODO make this non-static
    static final protected Map<Class, DomainTypeHandler> typeToHandlerMap =
            new HashMap<Class, DomainTypeHandler>();

    /** DomainTypeHandlerFactory for this session factory. */
    DomainTypeHandlerFactory domainTypeHandlerFactory = new DomainTypeHandlerFactoryImpl();


    /** The tables. */
    // TODO make this non-static
    static final protected Map<String,Table> Tables = new HashMap<String,Table>();

    /** The session factories. */
    static final protected Map<String, SessionFactoryImpl> sessionFactoryMap =
            new HashMap<String, SessionFactoryImpl>();

    /** Get a session factory. If there is already a session factory
     * with the same connect string and database, return it, regardless of whether other
     * properties of the factory are the same as specified in the Map.
     * @param props properties of the session factory
     * @return the session factory
     */
    static public synchronized SessionFactoryImpl getSessionFactory(Map<String, String> props) {
        String clusterConnectString = 
                getRequiredStringProperty(props, PROPERTY_CLUSTER_CONNECTSTRING);
        String clusterDatabase = getStringProperty(props, PROPERTY_CLUSTER_DATABASE,
                Constants.DEFAULT_PROPERTY_CLUSTER_DATABASE);
        String sessionFactoryKey = clusterConnectString + "+" + clusterDatabase;
        SessionFactoryImpl result = sessionFactoryMap.get(sessionFactoryKey);
        if (result == null) {
            logger.info("SessionFactoryImpl creating new SessionFactory with key " + sessionFactoryKey);
            result = new SessionFactoryImpl(props);
            sessionFactoryMap.put(sessionFactoryKey, result);
        }
        return result;
    }

    /** Create a new SessionFactoryImpl from the properties in the Map, and
     * connect to the ndb cluster.
     *
     * @param props the properties for the factory
     */
    protected SessionFactoryImpl(Map<String, String> props) {
        this.props = props;
        CLUSTER_CONNECT_STRING = getRequiredStringProperty(props, PROPERTY_CLUSTER_CONNECTSTRING);
        CLUSTER_CONNECT_RETRIES = getIntProperty(props, PROPERTY_CLUSTER_CONNECT_RETRIES,
                Constants.DEFAULT_PROPERTY_CLUSTER_CONNECT_RETRIES);
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
        CLUSTER_CONNECTION_SERVICE = getStringProperty(props, PROPERTY_CLUSTER_CONNECTION_SERVICE);
        try {
            ClusterConnectionService service =
                    ClusterJHelper.getServiceInstance(ClusterConnectionService.class,
                            CLUSTER_CONNECTION_SERVICE);
            clusterConnection = service.create(CLUSTER_CONNECT_STRING);
            clusterConnection.connect(CLUSTER_CONNECT_RETRIES, CLUSTER_CONNECT_DELAY,true);
            clusterConnection.waitUntilReady(CLUSTER_CONNECT_TIMEOUT_BEFORE,CLUSTER_CONNECT_TIMEOUT_AFTER);
        } catch (Exception Exception) {
            throw new ClusterJFatalException(
                    local.message("ERR_Connecting", props), Exception);
        }
        // now get a Session and complete a transaction to make sure that the cluster is ready
        try {
            Session session = getSession(null);
            session.currentTransaction().begin();
            session.currentTransaction().commit();
            session.close();
        } catch (Exception e) {
            if (e instanceof ClusterJException) {
                System.out.println("SessionFactoryImpl<init> failed to complete transaction.");
                throw (ClusterJException)e;
            }
        }
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
        try {
            Db db = clusterConnection.createDb(CLUSTER_DATABASE, CLUSTER_MAX_TRANSACTIONS);
            Dictionary dictionary = db.getDictionary();
            return new SessionImpl(this, properties, db, dictionary);
        } catch (Exception ex) {
            throw new ClusterJFatalException(
                    local.message("ERR_Create_Ndb"), ex);
        }
    }

    /** Get the DomainTypeHandler for a class. If the handler is not already
     * available, null is returned. 
     * @param cls the Class for which to get domain type handler
     * @return the DomainTypeHandler or null if not available
     */
    @SuppressWarnings( "unchecked" )
    public static <T> DomainTypeHandler<T> getDomainTypeHandler(Class<T> cls) {
        DomainTypeHandler<T> domainTypeHandler;
        // synchronize here because the map is not synchronized
        synchronized(typeToHandlerMap) {
            domainTypeHandler = typeToHandlerMap.get(cls);
        }
        return domainTypeHandler;
    }

    /** Create or get the DomainTypeHandler for a class.
     * Use the dictionary to validate against schema.
     * @param cls the Class for which to get domain type handler
     * @param dictionary the dictionary to validate against
     * @return the type handler
     */
    @SuppressWarnings( "unchecked" )
    public <T> DomainTypeHandler<T> getDomainTypeHandler(Class<T> cls,
            Dictionary dictionary) {
        DomainTypeHandler<T> domainTypeHandler;
        // synchronize here because the map is not synchronized
        synchronized(typeToHandlerMap) {
            domainTypeHandler = typeToHandlerMap.get(cls);
            if (logger.isDetailEnabled()) logger.detail("DomainTypeToHandler for "
                    + cls.getName() + "(" + cls
                    + ") returned " + domainTypeHandler);
            if (domainTypeHandler == null) {
                domainTypeHandler = domainTypeHandlerFactory.createDomainTypeHandler(cls,
                        dictionary);
                if (logger.isDetailEnabled()) logger.detail("createDomainTypeHandler for "
                        + cls.getName() + "(" + cls
                        + ") returned " + domainTypeHandler);
                typeToHandlerMap.put(cls, domainTypeHandler);
                Class<?> proxyClass = domainTypeHandler.getProxyClass();
                if (proxyClass != null) {
                    proxyClassToDomainClass.put(proxyClass, cls);
                }
            }
        }
        return domainTypeHandler;
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
        if (cls.getName().startsWith("$Proxy")) {
            cls = proxyClassToDomainClass.get(cls);
        }
        return cls;        
    }

    public <T> T newInstance(Class<T> cls, Dictionary dictionary) {
        DomainTypeHandler<T> domainTypeHandler = getDomainTypeHandler(cls, dictionary);
        return domainTypeHandler.newInstance();
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
    protected static String getStringProperty(Map<String, String> props, String propertyName) {
        return (String)props.get(propertyName);
    }

    /** Get the property from the properties map as a String. If the user has not
     * provided a value in the props, use the supplied default value.
     * @param props the properties
     * @param propertyName the name of the property
     * @param defaultValue the value to return if there is no property by that name
     * @return the value from the properties or the default value
     */
    protected static String getStringProperty(Map<String, String> props, String propertyName, String defaultValue) {
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
    protected static String getRequiredStringProperty(Map<String, String> props, String propertyName) {
        String result = (String)props.get(propertyName);
        if (result == null) {
                throw new ClusterJFatalUserException(
                        local.message("ERR_NullProperty", propertyName));                            
        }
        return result;
    }

    /** Get the property from the properties map as an int.
     * @param props the properties
     * @param propertyName the name of the property
     * @return the value from the properties or the default value
     * 
     */
    protected static int getIntProperty(Map<String, String> props, String propertyName) {
        String property = getStringProperty(props, propertyName);
        try {
            int result = Integer.parseInt(property);
            return result;
        } catch (NumberFormatException ex) {
            throw new ClusterJFatalUserException(
                    local.message("ERR_NumericFormat", propertyName, property));
        }
    }

    /** Get the property from the properties map as an int. If the user has not
     * provided a value in the props, use the supplied default value.
     * @param props the properties
     * @param propertyName the name of the property
     * @param defaultValue the value to return if there is no property by that name
     * @return the value from the properties or the default value
     */
    protected static int getIntProperty(Map<String, String> props, String propertyName, int defaultValue) {
        String property = getStringProperty(props, propertyName);
        if (property == null) {
            return defaultValue;
        }
        try {
            int result = Integer.parseInt(property);
            return result;
        } catch (NumberFormatException ex) {
            throw new ClusterJFatalUserException(
                    local.message("ERR_NumericFormat", propertyName, property));
        }
    }

    public void close() {
        // TODO: What should this do?
    }

    public void setDomainTypeHandlerFactory(DomainTypeHandlerFactory domainTypeHandlerFactory) {
        this.domainTypeHandlerFactory = domainTypeHandlerFactory;
    }

    public DomainTypeHandlerFactory getDomainTypeHandlerFactory() {
        return domainTypeHandlerFactory;
    }

}
