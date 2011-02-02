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

package com.mysql.clusterj.core.metadata;

import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.DynamicObjectDelegate;

import com.mysql.clusterj.annotation.PersistenceCapable;

import com.mysql.clusterj.core.CacheManager;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Operation;



import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** This instance manages a persistence-capable type.
 * Currently, only interfaces can be persistence-capable. Persistent
 * properties consist of a pair of bean-pattern methods for which the
 * get method returns the same type as the parameter of the 
 * similarly-named set method.
 * @param T the class of the persistence-capable type
 */
public class DomainTypeHandlerImpl<T> extends AbstractDomainTypeHandlerImpl<T> {

    /** The domain class. */
    Class<T> cls;

    /** Dynamic class indicator */
    boolean dynamic = false;

    /** The methods of the properties. */
    private Map<String, Method> unmatchedGetMethods = new HashMap<String, Method>();
    private Map<String, Method> unmatchedSetMethods = new HashMap<String, Method>();

    /** The Proxy class for the Domain Class. */
    protected Class<T> proxyClass;

    /** The constructor for the Proxy class. */
    Constructor<T> ctor;

    /** The PersistenceCapable annotation for this class. */
    PersistenceCapable persistenceCapable;

    /** Helper parameter for constructor. */
    protected static final Class<?>[] invocationHandlerClassArray = 
            new Class[]{InvocationHandler.class};

    /** Initialize DomainTypeHandler for a class.
     * 
     * @param cls the domain class (this is the only class 
     * known to the rest of the implementation)
     * @param dictionary NdbDictionary instance used for metadata access
     */
    @SuppressWarnings( "unchecked" )
    public DomainTypeHandlerImpl(Class<T> cls, Dictionary dictionary) {
        this.cls = cls;
        this.name = cls.getName();
        this.dynamic = DynamicObject.class.isAssignableFrom(cls);
        if (dynamic) {
            // Dynamic object has a handler but no proxy
            this.tableName = getTableNameForDynamicObject((Class<DynamicObject>)cls);
        } else {
            // Create a proxy class for the domain class
            proxyClass = (Class<T>)Proxy.getProxyClass(
                    cls.getClassLoader(), new Class[]{cls});
            ctor = getConstructorForInvocationHandler (proxyClass);
            persistenceCapable = cls.getAnnotation(PersistenceCapable.class);
            if (persistenceCapable == null) {
                throw new ClusterJUserException(local.message(
                        "ERR_No_Persistence_Capable_Annotation", name));
            }
            this.tableName = persistenceCapable.table();
        }
        this.table = getTable(dictionary);
        if (table == null) {
            throw new ClusterJUserException(local.message("ERR_Get_NdbTable", name, tableName));
        }
        if (logger.isDebugEnabled()) logger.debug("Found Table for " + tableName);

        // the id field handlers will be initialized via registerPrimaryKeyColumn
        this.primaryKeyColumnNames = table.getPrimaryKeyColumnNames();
        this.numberOfIdFields  = primaryKeyColumnNames.length;
        this.idFieldHandlers = new DomainFieldHandlerImpl[numberOfIdFields];
        this.idFieldNumbers = new int[numberOfIdFields];

        // the partition key field handlers will be initialized via registerPrimaryKeyColumn
        this.partitionKeyColumnNames = table.getPartitionKeyColumnNames();
        this.numberOfPartitionKeyColumns = partitionKeyColumnNames.length;
        this.partitionKeyFieldHandlers = new DomainFieldHandlerImpl[numberOfPartitionKeyColumns];

        // Process indexes for the table. There might not be a field associated with the index.
        // The first entry in indexHandlerImpls is for the mandatory hash primary key,
        // which is not really an index but is treated as an index by query.
        Index primaryIndex = dictionary.getIndex("PRIMARY$KEY", tableName, "PRIMARY");
        IndexHandlerImpl primaryIndexHandler =
            new IndexHandlerImpl(this, dictionary, primaryIndex, primaryKeyColumnNames);
        indexHandlerImpls.add(primaryIndexHandler);

        String[] indexNames = table.getIndexNames();
        for (String indexName: indexNames) {
            // the index alias is the name as known by the user (without the $unique suffix)
            String indexAlias = removeUniqueSuffix(indexName);
            Index index = dictionary.getIndex(indexName, tableName, indexAlias);
            String[] columnNames = index.getColumnNames();
            IndexHandlerImpl imd = new IndexHandlerImpl(this, dictionary, index, columnNames);
            indexHandlerImpls.add(imd);
        }

        if (dynamic) {
            // for each column in the database, create a field
            List<String> fieldNameList = new ArrayList<String>();
            for (String columnName: table.getColumnNames()) {
                Column storeColumn = table.getColumn(columnName);
                DomainFieldHandlerImpl domainFieldHandler = null;
                domainFieldHandler = 
                    new DomainFieldHandlerImpl(this, table, numberOfFields++, storeColumn);
                String fieldName = domainFieldHandler.getName();
                fieldNameList.add(fieldName);
                fieldNameToNumber.put(domainFieldHandler.getName(), domainFieldHandler.getFieldNumber());
                persistentFieldHandlers.add(domainFieldHandler);
                if (!storeColumn.isPrimaryKey()) {
                    nonPKFieldHandlers.add(domainFieldHandler);
                }
            }
            fieldNames = fieldNameList.toArray(new String[fieldNameList.size()]);
        } else {
            // Iterate the fields (names and types based on get/set methods) in the class
            List<String> fieldNameList = new ArrayList<String>();
            Method[] methods = cls.getMethods();
            for (Method method: methods) {
                // remember get methods
                String methodName = method.getName();
                String name = convertMethodName(methodName);
                Class type = getType(method);
                DomainFieldHandlerImpl domainFieldHandler = null;
                if (methodName.startsWith("get")) {
                    Method unmatched = unmatchedSetMethods.get(name);
                    if (unmatched == null) {
                        // get is first of the pair; put it into the unmatched map
                        unmatchedGetMethods.put(name, method);
                    } else {
                        // found the potential match
                        if (getType(unmatched).equals(type)) {
                            // method names and types match
                            unmatchedSetMethods.remove(name);
                            domainFieldHandler = new DomainFieldHandlerImpl(this, table,
                                    numberOfFields++, name, type, method, unmatched);
                        } else {
                            // both unmatched because of type mismatch
                            unmatchedGetMethods.put(name, method);
                        }
                    }
                } else if (methodName.startsWith("set")) {
                    Method unmatched = unmatchedGetMethods.get(name);
                    if (unmatched == null) {
                        // set is first of the pair; put it into the unmatched map
                        unmatchedSetMethods.put(name, method);
                    } else {
                        // found the potential match
                        if (getType(unmatched).equals(type)) {
                            // method names and types match
                            unmatchedGetMethods.remove(name);
                            domainFieldHandler = new DomainFieldHandlerImpl(this, table,
                                    numberOfFields++, name, type, unmatched, method);
                        } else {
                            // both unmatched because of type mismatch
                            unmatchedSetMethods.put(name, method);
                        }
                    }
                }
                if (domainFieldHandler != null) {
                    // found matching methods
                    // set up field name to number map
                    String fieldName = domainFieldHandler.getName();
                    fieldNameList.add(fieldName);
                    fieldNameToNumber.put(domainFieldHandler.getName(), domainFieldHandler.getFieldNumber());
                    // put field into either persistent or not persistent list
                    if (domainFieldHandler.isPersistent()) {
                        persistentFieldHandlers.add(domainFieldHandler);
                        if (!domainFieldHandler.isPrimaryKey()) {
                            nonPKFieldHandlers.add(domainFieldHandler);
                        }
                    }
                    if (domainFieldHandler.isPrimitive()) {
                        primitiveFieldHandlers.add(domainFieldHandler);
                    }
                }
            }
            fieldNames = fieldNameList.toArray(new String[fieldNameList.size()]);
            // done with methods; if anything in unmatched we have a problem
            if ((!unmatchedGetMethods.isEmpty()) || (!unmatchedSetMethods.isEmpty())) {
                throw new ClusterJUserException(
                        local.message("ERR_Unmatched_Methods", 
                        unmatchedGetMethods, unmatchedSetMethods));
            }

        }
        // Check that all index columnNames have corresponding fields
        // indexes without fields will be unusable for query
        for (IndexHandlerImpl indexHandler:indexHandlerImpls) {
            indexHandler.assertAllColumnsHaveFields();
        }

        if (logger.isDebugEnabled()) {
            logger.debug(toString());
            logger.debug("DomainTypeHandlerImpl " + name + "Indices " + indexHandlerImpls);
        }
    }

    protected <O extends DynamicObject> String getTableNameForDynamicObject(Class<O> cls) {
        DynamicObject dynamicObject;
        PersistenceCapable persistenceCapable = cls.getAnnotation(PersistenceCapable.class);
        String tableName = null;
        try {
            dynamicObject = cls.newInstance();
            tableName = dynamicObject.table();
            if (tableName == null  && persistenceCapable != null) {
                tableName = persistenceCapable.table();
            }
        } catch (InstantiationException e) {
            throw new ClusterJUserException(local.message("ERR_Dynamic_Object_Instantiation", cls.getName()), e);
        } catch (IllegalAccessException e) {
            throw new ClusterJUserException(local.message("ERR_Dynamic_Object_Illegal_Access", cls.getName()), e);
        }
        if (tableName == null) {
            throw new ClusterJUserException(local.message("ERR_Dynamic_Object_Null_Table_Name",
                    cls.getName()));
        }
        return tableName;
    }

    /** Is this type supported? */
    public boolean isSupportedType() {
        // if unsupported, throw an exception
        return true;
    }

    public ValueHandler getValueHandler(Object instance)
            throws IllegalArgumentException {
        if (instance instanceof ValueHandler) {
            return (ValueHandler)instance;
        } else if (instance instanceof DynamicObject) {
            return (ValueHandler)((DynamicObject)instance).delegate();
        } else {
            ValueHandler handler = (ValueHandler)
                    Proxy.getInvocationHandler(instance);
            return handler;
        }
    }

    public void objectMarkModified(ValueHandler handler, String fieldName) {
        int fieldNumber = fieldNameToNumber.get(fieldName);
        handler.markModified(fieldNumber);
    }

    public Class<T> getProxyClass() {
        return proxyClass;
    }

    public Class<T> getDomainClass() {
        return cls;
    }

    public void operationSetValues(Object instance, Operation op) {
        ValueHandler handler = getValueHandler(instance);
        for (DomainFieldHandler fmd: persistentFieldHandlers) {
            fmd.operationSetValue(handler, op);
        }
    }

    public void objectSetKeys(Object keys, Object instance) {
        ValueHandler handler = getValueHandler(instance);
        int size = idFieldHandlers.length;
        if (size == 1) {
            // single primary key; store value in key field
            for (DomainFieldHandler fmd: idFieldHandlers) {
                fmd.objectSetKeyValue(keys, handler);
            }
        } else if (keys instanceof java.lang.Object[]) {
            if (logger.isDetailEnabled()) logger.detail(keys.toString());
            // composite primary key; store values in key fields
            for (int i = 0; i < idFieldHandlers.length; ++i) {
                idFieldHandlers[i].objectSetKeyValue(((Object[])keys)[i], handler);
            }
        } else {
                // composite key but parameter is not Object[]
                throw new ClusterJUserException(
                        local.message("ERR_Composite_Key_Parameter"));
        }
    }

    public void objectResetModified(ValueHandler handler) {
        handler.resetModified();
    }

    @SuppressWarnings("unchecked")
    public void objectSetCacheManager(CacheManager cm, Object instance) {
        InvocationHandlerImpl<T> handler =
                (InvocationHandlerImpl<T>)getValueHandler(instance);
        handler.setCacheManager(cm);
    }

    public T newInstance() {
        T instance;
        try {
            InvocationHandlerImpl<T> handler = new InvocationHandlerImpl<T>(this);
            if (dynamic) {
                instance = cls.newInstance();
                ((DynamicObject)instance).delegate((DynamicObjectDelegate)handler);
            } else {
                instance = ctor.newInstance(new Object[] {handler});
                handler.setProxy(instance);
            }
            return instance;
        } catch (InstantiationException ex) {
            throw new ClusterJException(
                    local.message("ERR_Create_Instance", cls.getName()), ex);
        } catch (IllegalAccessException ex) {
            throw new ClusterJException(
                    local.message("ERR_Create_Instance", cls.getName()), ex);
        } catch (IllegalArgumentException ex) {
            throw new ClusterJException(
                    local.message("ERR_Create_Instance", cls.getName()), ex);
        } catch (InvocationTargetException ex) {
            throw new ClusterJException(
                    local.message("ERR_Create_Instance", cls.getName()), ex);
        } catch (SecurityException ex) {
            throw new ClusterJException(
                    local.message("ERR_Create_Instance", cls.getName()), ex);
        }
    }


    public void initializeNotPersistentFields(InvocationHandlerImpl<T> handler) {
        for (DomainFieldHandler fmd:primitiveFieldHandlers) {
            ((AbstractDomainFieldHandlerImpl) fmd).objectSetDefaultValue(handler);
        }
    }

    /** Convert a method name to a javabeans property name.
     * This is done by removing the leading "get" or "set" and upper-casing the
     * result.
     * @param methodName the name of a get or set method
     * @return the property name
     */
    private String convertMethodName(String methodName) {
        String head = methodName.substring(3, 4).toLowerCase();
        String tail = methodName.substring(4);
        return head + tail;
    }

    @SuppressWarnings( "unchecked" )
    public T getInstance(ValueHandler handler) {
        return (T)((InvocationHandlerImpl)handler).getProxy();
    }

    private Class<?> getType(Method method) {
        Class<?> result = null;
        if (method.getName().startsWith("get")) {
            result = method.getReturnType();
        } else if (method.getName().startsWith("set")) {
            Class<?>[] types = method.getParameterTypes();
            if (types.length != 1) {
                throw new ClusterJUserException(
                        local.message("ERR_Set_Method_Parameters",
                        method.getName(), types.length));
            }
            result = types[0];
        } else {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Method_Name", method.getName()));
        }
        if (result == null) {
            throw new ClusterJUserException(
                    local.message("ERR_Unmatched_Method" + method.getName()));
        }
        return result;
    }

    /** TODO: Protect with doPrivileged. */
    protected Constructor<T> getConstructorForInvocationHandler(
            Class<T> cls) {
        try {
            return cls.getConstructor(invocationHandlerClassArray);
        } catch (NoSuchMethodException ex) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Get_Constructor", cls), ex);
        } catch (SecurityException ex) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Get_Constructor", cls), ex);
        }
    }

    public ValueHandler createKeyValueHandler(Object keys) {
        if (keys == null) {
            throw new ClusterJUserException(
                    local.message("ERR_Key_Must_Not_Be_Null", getName(), "unknown"));
        }
        Object[] keyValues = new Object[numberOfFields];
        // check the cardinality of the keys with the number of key fields
        if (numberOfIdFields == 1) {
            Class<?> keyType = idFieldHandlers[0].getType();
            DomainFieldHandler fmd = idFieldHandlers[0];
            checkKeyType(fmd.getName(), keyType, keys);
            int keyFieldNumber = fmd.getFieldNumber();
            keyValues[keyFieldNumber] = keys;
        } else {
            if (!(keys.getClass().isArray())) {
                throw new ClusterJUserException(
                        local.message("ERR_Key_Must_Be_An_Object_Array",
                        numberOfIdFields));
            }
            Object[]keyObjects = (Object[])keys;
            for (int i = 0; i < numberOfIdFields; ++i) {
                DomainFieldHandler fmd = idFieldHandlers[i];
                int index = fmd.getFieldNumber();
                Object keyObject = keyObjects[i];
                Class<?> keyType = fmd.getType();
                checkKeyType(fmd.getName(), keyType, keyObject);
                keyValues[index] = keyObjects[i];
            }
        }
        return new KeyValueHandlerImpl(keyValues);
    }

    /** Check that the key value matches the key type. Keys that are part
     * of a compound key can be null as long as they are not part of the
     * partition key.
     *
     * @param name the name of the field
     * @param keyType the type of the key field
     * @param keys the value for the key field
     */
    public void checkKeyType(String name, Class<?> keyType, Object keys)
            throws ClusterJUserException {
        if (keys == null) {
            return;
        }
        Class<?> valueType = keys.getClass();
        if (keyType.isAssignableFrom(valueType) ||
                (keyType == int.class && valueType == Integer.class) ||
                (keyType == Integer.class & valueType == int.class) ||
                (keyType == Long.class & valueType == long.class) ||
                (keyType == long.class & valueType == Long.class)) {
            return;
        } else {
                throw new ClusterJUserException(
                    local.message("ERR_Incorrect_Key_Type",
                    name, valueType.getName(), keyType.getName()));
        }
    }

    public Class<?> getOidClass() {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    protected ColumnMetadata[] columnMetadata() {
        ColumnMetadata[] result = new ColumnMetadata[numberOfFields];
        return persistentFieldHandlers.toArray(result);
    }

}
