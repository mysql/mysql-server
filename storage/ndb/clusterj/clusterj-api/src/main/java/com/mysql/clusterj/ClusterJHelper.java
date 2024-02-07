/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

package com.mysql.clusterj;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.InvocationTargetException;
import java.net.URL;

import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.Map;

/**
 * ClusterJHelper provides helper methods to bridge between the API and
 * the implementation.
 */
public class ClusterJHelper {

    /** My class loader */
    static ClassLoader CLUSTERJ_HELPER_CLASS_LOADER = ClusterJHelper.class.getClassLoader();

    /** Return a new Dbug instance.
     * 
     * @return a new Dbug instance
     */
    public static Dbug newDbug() {
        return getServiceInstance(Dbug.class);
    }

    /** Locate a SessionFactory implementation by services lookup. The class loader
     * used is the thread's context class loader.
     *
     * @param props properties of the session factory
     * @return the session factory
     * @throws ClusterFatalUserException if the connection to the cluster cannot be made
     */
    public static SessionFactory getSessionFactory(Map props) {
        return getSessionFactory(props, CLUSTERJ_HELPER_CLASS_LOADER);
    }

    /** Locate a SessionFactory implementation by services lookup of
     * a specific class loader. The properties are a Map that might contain
     * implementation-specific properties plus standard properties.
     *
     * @param props the properties for the factory
     * @param loader the class loader for the factory implementation
     * @return the session factory
     * @throws ClusterFatalUserException if the connection to the cluster cannot be made
     */
    public static SessionFactory getSessionFactory(Map props, ClassLoader loader) {
        SessionFactoryService service =
                getServiceInstance(SessionFactoryService.class, loader);
        SessionFactory factory = service.getSessionFactory(props);
        return factory;
    }

    /** Locate a service implementation by services lookup of
     * the context class loader.
     *
     * @param cls the class of the factory
     * @return the service instance
     */
    public static <T> T getServiceInstance(Class<T> cls) {
        return getServiceInstance(cls, CLUSTERJ_HELPER_CLASS_LOADER);
    }

    /** Locate all service implementations by services lookup of
     * a specific class loader. Implementations in the services file
     * are instantiated and returned. Failed instantiations are remembered
     * in the errorMessages buffer.
     *
     * @param cls the class of the factory
     * @param loader the class loader for the factory implementation
     * @param errorMessages a buffer used to hold the error messages
     * @return the service instance
     */
    @SuppressWarnings("unchecked") // Class.forName
    public static <T> List<T> getServiceInstances(Class<T> cls, ClassLoader loader,
            StringBuffer errorMessages) {
        // find all service implementations of the class in the class loader.
        List<T> result = new ArrayList<T>();
        String factoryName = cls.getName();
        String serviceName = "META-INF/services/" + factoryName;
        Enumeration<URL> urls = null;
        try {
            urls = loader.getResources(serviceName);
        } catch (IOException ex) {
            throw new ClusterJFatalUserException(ex);
        }
        while (urls.hasMoreElements()) {
            InputStream inputStream = null;
            InputStreamReader inputStreamReader = null;
            BufferedReader bufferedReader = null;
            try {
                URL url = urls.nextElement();
                inputStream = url.openStream();
                inputStreamReader = new InputStreamReader(inputStream);
                bufferedReader = new BufferedReader(inputStreamReader);
                factoryName = bufferedReader.readLine();
                Class<T> serviceClass = (Class<T>)Class.forName(factoryName, true, loader);
                T service = serviceClass.getConstructor().newInstance();
                if (service != null) {
                    result.add(service);
                }
            } catch (IOException ex) {
                errorMessages.append(ex.toString());
            } catch (ClassNotFoundException ex) {
                errorMessages.append(ex.toString());
            } catch (InstantiationException ex) {
                errorMessages.append(ex.toString());
            } catch (IllegalAccessException ex) {
                errorMessages.append(ex.toString());
            } catch (InvocationTargetException e) {
                errorMessages.append(e.toString());
            } catch (NoSuchMethodException e) {
                errorMessages.append(e.toString());
            } finally {
                try {
                    if (inputStream != null) {
                        inputStream.close();
                    }
                } catch (IOException ioex) {
                    // nothing to do here
                }
            }
        }
        return result;
    }

    /** Locate a service implementation for a service by services lookup of
     * a specific class loader. The first service instance found is returned.
     *
     * @param cls the class of the factory
     * @param loader the class loader for the factory implementation
     * @return the service instance
     */
    public static <T> T getServiceInstance(Class<T> cls, ClassLoader loader) {
        StringBuffer errorMessages = new StringBuffer();
        List<T> services = getServiceInstances(cls, loader, errorMessages);
        if (services.size() != 0) {
            return services.get(0);
        } else {
            String factoryName = cls.getName();
            String serviceName = "META-INF/services/" + factoryName;
            throw new ClusterJFatalUserException(
                    "No instance for service " + factoryName +
                    " could be found. " +
                    "Make sure that there is a file " + serviceName +
                    " in your class path naming the factory class." +
                    errorMessages);
        }
    }

    /** Locate a service implementation for a service.
     * If the implementation name is not null, use it instead of
     * looking up. If the implementation class is not loadable or does not
     * implement the interface, throw an exception.
     * @param cls
     * @param implementationClassName name of implementation class to load
     * @param loader the ClassLoader to use to find the service
     * @return the implementation instance for a service
     */
    @SuppressWarnings("unchecked") // (Class<T>)clazz
    public static <T> T getServiceInstance(Class<T> cls, String implementationClassName, ClassLoader loader) {
        if (implementationClassName == null) {
            return getServiceInstance(cls, loader);
        } else {
            try {
                Class<?> clazz = Class.forName(implementationClassName, true, loader);
                Class<T> serviceClass = null;
                if (!(cls.isAssignableFrom(clazz))) {
                   throw new ClassCastException(cls.getName() + " " + implementationClassName);
                }
                serviceClass = (Class<T>)clazz;
                T service = serviceClass.getConstructor().newInstance();
                return service;
            } catch (ClassNotFoundException e) {
                throw new ClusterJFatalUserException(implementationClassName, e);
            } catch (ClassCastException e) {
                throw new ClusterJFatalUserException(implementationClassName, e);
            } catch (InstantiationException e) {
                throw new ClusterJFatalUserException(implementationClassName, e);
            } catch (IllegalAccessException e) {
                throw new ClusterJFatalUserException(implementationClassName, e);
            } catch (InvocationTargetException e) {
                throw new ClusterJFatalUserException(implementationClassName, e);
            } catch (NoSuchMethodException e) {
                throw new ClusterJFatalUserException(implementationClassName, e);
            }
        }
    }

    /** Locate a service implementation for a service.
     * If the implementation name is not null, use it instead of
     * looking up. If the implementation class is not loadable or does not
     * implement the interface, throw an exception.
     * Use the ClusterJHelper class loader to find the service.
     * @param cls
     * @param implementationClassName
     * @return the implementation instance for a service
     */
    public static <T> T getServiceInstance(Class<T> cls, String implementationClassName) {
        return getServiceInstance(cls, implementationClassName, CLUSTERJ_HELPER_CLASS_LOADER);
    }

    /** Get the named boolean property from either the environment or system properties.
     * If the property is not 'true' then return false.
     * @param propertyName the name of the property
     * @param def the default if the property is not set 
     * @return the system property if it is set via -D or the system environment
     */
    public static boolean getBooleanProperty(String propertyName, String def) {
        String propertyFromEnvironment = System.getenv(propertyName);
        // system property overrides environment variable
        String propertyFromSystem = System.getProperty(propertyName,
                propertyFromEnvironment==null?def:propertyFromEnvironment);
        boolean result = propertyFromSystem.equals("true")?true:false;
        return result;
    }

    /** Get the named String property from either the environment or system properties.
     * @param propertyName the name of the property
     * @param def the default if the property is not set 
     * @return the system property if it is set via -D or the system environment
     */
    public static String getStringProperty(String propertyName, String def) {
        String propertyFromEnvironment = System.getenv(propertyName);
        // system property overrides environment variable
        String result = System.getProperty(propertyName,
                propertyFromEnvironment==null?def:propertyFromEnvironment);
        return result;
    }

}
