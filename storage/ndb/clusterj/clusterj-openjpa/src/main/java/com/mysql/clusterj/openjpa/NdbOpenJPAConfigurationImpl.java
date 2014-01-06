/*
   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.openjpa;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.SessionFactory;

import com.mysql.clusterj.core.SessionFactoryImpl;

import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandlerFactory;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;

import org.apache.openjpa.jdbc.conf.JDBCConfigurationImpl;
import org.apache.openjpa.jdbc.meta.ClassMapping;
import org.apache.openjpa.lib.conf.BooleanValue;
import org.apache.openjpa.lib.conf.IntValue;
import org.apache.openjpa.lib.conf.ProductDerivations;
import org.apache.openjpa.lib.conf.StringValue;

/**
 * Default implementation of the {@link NdbOpenJPAConfiguration} interface.
 * This implementation extends the JDBCConfiguration so both access
 * to MySQLd via JDBC and access to Ndb via ClusterJ are supported.
 * Type safety: The return type Map for getCacheMarshallerInstances()
 * from the type OpenJPAConfigurationImpl needs unchecked conversion
 * to conform to Map<String,CacheMarshaller> from the type OpenJPAConfiguration
 */
@SuppressWarnings("unchecked")
public class NdbOpenJPAConfigurationImpl extends JDBCConfigurationImpl
    implements NdbOpenJPAConfiguration, com.mysql.clusterj.Constants, DomainTypeHandlerFactory {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(NdbOpenJPAConfigurationImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(NdbOpenJPAConfigurationImpl.class);

    public StringValue connectString;
    public IntValue connectRetries;
    public IntValue connectDelay;
    public IntValue connectVerbose;
    public IntValue connectTimeoutBefore;
    public IntValue connectTimeoutAfter;
    public StringValue username;
    public StringValue password;
    public StringValue database;
    public IntValue maxTransactions;
    public BooleanValue failOnJDBCPath;
    public SessionFactoryImpl sessionFactory;
    private final Map<Class<?>, NdbOpenJPADomainTypeHandlerImpl<?>> domainTypeHandlerMap =
            new HashMap<Class<?>, NdbOpenJPADomainTypeHandlerImpl<?>>();

    /**
     * These are to bridge an incompatibility between OpenJPA 1.x and 2.x in the handling of configuration
     * values. In 1.x, the IntValue.get() method returns int and in 2.x it returns Integer.
     * Similarly, in 1.x BooleanValue.get() returns boolean and in 2.x it returns Boolean.
     */
    static private Method intValueMethod;
    static private Method booleanValueMethod;
    static {
        try {
            intValueMethod = IntValue.class.getMethod("get", (Class[])null);
            booleanValueMethod = BooleanValue.class.getMethod("get", (Class[])null);
        } catch (SecurityException e) {
            throw new ClusterJFatalInternalException(e);
        } catch (NoSuchMethodException e) {
            throw new ClusterJFatalInternalException(e);
        }
    }

    /** Return the int value from a configuration IntValue.
     * 
     */
    int getIntValue(IntValue value) {
        try {
            return (Integer)intValueMethod.invoke(value);
        } catch (IllegalArgumentException e) {
            throw new ClusterJFatalInternalException(e);
        } catch (IllegalAccessException e) {
            throw new ClusterJFatalInternalException(e);
        } catch (InvocationTargetException e) {
            throw new ClusterJFatalInternalException(e);
        }
    }
    /** Return the boolean value from a configuration BooleanValue.
     * 
     */
    boolean getBooleanValue(BooleanValue value) {
        try {
            return (Boolean) booleanValueMethod.invoke(value);
        } catch (IllegalArgumentException e) {
            throw new ClusterJFatalInternalException(e);
        } catch (IllegalAccessException e) {
            throw new ClusterJFatalInternalException(e);
        } catch (InvocationTargetException e) {
            throw new ClusterJFatalInternalException(e);
        }
    }
    /**
     * Default constructor. Attempts to load default properties.
     */
    public NdbOpenJPAConfigurationImpl() {
        this(true);
    }

    /**
     * Constructor.
     *
     * @param loadGlobals whether to attempt to load the global properties
     */
    public NdbOpenJPAConfigurationImpl(boolean loadGlobals) {
        this(true, loadGlobals);
    }

    /**
     * Constructor.
     *
     * @param derivations whether to apply product derivations
     * @param loadGlobals whether to attempt to load the global properties
     */
    public NdbOpenJPAConfigurationImpl(boolean derivations, boolean loadGlobals) {
        super(false, false);

        connectString = addString("ndb.connectString");
        connectRetries = addInt("ndb.connectRetries");
        connectRetries.set(4);
        connectDelay = addInt("ndb.connectDelay");
        connectDelay.set(5);
        connectVerbose = addInt("ndb.connectVerbose");
        connectVerbose.set(0);
        connectTimeoutBefore = addInt("ndb.connectTimeoutBefore");
        connectTimeoutBefore.set(30);
        connectTimeoutAfter = addInt("ndb.connectTimeoutAfter");
        connectTimeoutAfter.set(20);
        username = addString("ndb.username");
        password = addString("ndb.password");
        database = addString("ndb.database");
        database.set("test");
        maxTransactions = addInt("ndb.maxTransactions");
        maxTransactions.set(1024);
        failOnJDBCPath = addBoolean("ndb.failOnJDBCPath");
        failOnJDBCPath.set(false);

        sessionFactory = null;

        if (derivations)
            ProductDerivations.beforeConfigurationLoad(this);
        if (loadGlobals)
            loadGlobals();
    }

    /**
     * Copy constructor
     */
    public NdbOpenJPAConfigurationImpl(NdbOpenJPAConfiguration conf) {
        this(true, false);
        if (conf != null)
            fromProperties(conf.toProperties(false));
    }

    public String getConnectString() {
        return connectString.get();
    }

    public void setConnectString(String value) {
        connectString.set(value);
    }

    public int getConnectRetries() {
        return getIntValue(connectRetries);
    }

    public void setConnectRetries(int value) {
        connectRetries.set(value);
    }

    public int getConnectDelay() {
        return getIntValue(connectDelay);
    }

    public void setConnectDelay(int value) {
        connectDelay.set(value);
    }

    public int getConnectVerbose() {
        return getIntValue(connectVerbose);
    }

    public void setConnectVerbose(int value) {
        connectVerbose.set(value);
    }

    public int getConnectTimeoutBefore() {
        return getIntValue(connectTimeoutBefore);
    }

    public void setConnectTimeoutBefore(int value) {
        connectTimeoutBefore.set(value);
    }

    public int getConnectTimeoutAfter() {
        return getIntValue(connectTimeoutAfter);
    }

    public void setConnectTimeoutAfter(int value) {
        connectTimeoutAfter.set(value);
    }

    public String getUsername() {
        return username.getString();
    }

    public void setUsername(String value) {
        username.setString(value);
    }

    public String getPassword() {
        return password.getString();
    }

    public void setPassword(String value) {
        password.setString(value);
    }

    public String getDatabase() {
        return database.getString();
    }

    public void setDatabase(String value) {
        database.setString(value);
    }

    public int getMaxTransactions() {
        return getIntValue(maxTransactions);
    }

    public void setMaxTransactions(int value) {
        maxTransactions.set(value);
    }

    public boolean getFailOnJDBCPath() {
        return getBooleanValue(failOnJDBCPath);
    }

    public void setFailOnJDBCPath(boolean value) {
        failOnJDBCPath.set(value);
    }

    public SessionFactoryImpl getSessionFactory() {
        if (sessionFactory == null) {
            sessionFactory = createSessionFactory();
        }
        return (SessionFactoryImpl) sessionFactory;
    }

    public void setSessionFactory(SessionFactory value) {
        sessionFactory = (SessionFactoryImpl) value;
    }

    public SessionFactoryImpl createSessionFactory() {
        // require connectString to be specified
        if (connectString.get() == null) {
            throw new IllegalStateException(
                    local.message("ERR_Missing_Connect_String"));
        }
        // map OpenJPA properties to ClusterJ properties
        Properties props = new Properties();
        props.put(PROPERTY_CLUSTER_CONNECTSTRING,
                connectString.get());
        props.put(PROPERTY_CLUSTER_CONNECT_RETRIES,
                connectRetries.getString());
        props.put(PROPERTY_CLUSTER_CONNECT_DELAY,
                connectDelay.getString());
        props.put(PROPERTY_CLUSTER_CONNECT_VERBOSE,
                connectVerbose.getString());
        props.put(PROPERTY_CLUSTER_CONNECT_TIMEOUT_BEFORE,
                connectTimeoutBefore.getString());
        props.put(PROPERTY_CLUSTER_CONNECT_TIMEOUT_AFTER,
                connectTimeoutAfter.getString());
        props.put(PROPERTY_CLUSTER_DATABASE,
                database.getString());
        props.put(PROPERTY_CLUSTER_MAX_TRANSACTIONS,
                maxTransactions.getString());
        SessionFactoryImpl factory = (SessionFactoryImpl)
                ClusterJHelper.getSessionFactory(props);
        factory.setDomainTypeHandlerFactory(this);
        return factory;
    }

    /** Get the domain type handler for this class mapping. A cached handler is
     * returned if possible. Synchronize on the class-to-handler map.
     * @param cmd the openjpa class mapping
     * @return the domain type handler
     */
    public NdbOpenJPADomainTypeHandlerImpl<?> getDomainTypeHandler(
            ClassMapping cmd, Dictionary dictionary) {
        NdbOpenJPADomainTypeHandlerImpl<?> result;
        Class<?> domainClass = cmd.getDescribedType();
        synchronized(domainTypeHandlerMap) {
            result = domainTypeHandlerMap.get(domainClass);
            if (result == null) {
                if (logger.isDebugEnabled()) logger.debug("domain class: " + domainClass.getName());
                result = createDomainTypeHandler(cmd, dictionary);
                domainTypeHandlerMap.put(domainClass, result);
                result.initializeRelations();
                logger.info("New domain type " + result.getName()
                        + (result.isSupportedType()?" is supported.":
                            " is not known to be supported because " + result.getReasons()));
            }
        }
        return result;
    }

    /** Create a new domain type handler for the class mapping.
     * 
     * @param classMapping the openjpa class mapping
     * @return the domain type handler
     */
    private NdbOpenJPADomainTypeHandlerImpl<?> createDomainTypeHandler(
            ClassMapping classMapping, Dictionary dictionary) {
        return new NdbOpenJPADomainTypeHandlerImpl<Object>(dictionary, classMapping, this);
    }

    /**
     * Free the data sources.
     */
    @Override
    protected void preClose() {
        if (sessionFactory != null) {
            sessionFactory.close();
        }
        super.preClose();
    }

    @Override
    protected boolean isInvalidProperty(String propName) {
        if (super.isInvalidProperty(propName))
            return true;

        // handle openjpa.ndb.SomeMisspelledProperty, but not
        // openjpa.someotherimplementation.SomeProperty
        String lowerCasePropName = propName.toLowerCase();
        String[] prefixes = ProductDerivations.getConfigurationPrefixes();
        for (int i = 0; i < prefixes.length; i++)
            if (lowerCasePropName.startsWith(prefixes[i] + ".ndb"))
                return true; 
        return false;
    }

    /** Get the domain type handler for the class. This method is called by the query
     * handler when performing a clusterj query for an openjpa entity. The class
     * must have already been registered via the openjpa clusterj path.
     */
    public <T> DomainTypeHandler<T> createDomainTypeHandler(
            Class<T> domainClass, Dictionary dictionary) {
        DomainTypeHandler<T> result = (DomainTypeHandler<T>) domainTypeHandlerMap.get(domainClass);
        if (result == null) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Create_Domain_Type_Handler_First", domainClass.getName()));
        }
        return result;
    }

}
