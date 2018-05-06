/*
   Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.core.metadata;

import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.spi.SmartValueHandler;
import com.mysql.clusterj.core.spi.ValueHandlerFactory;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.DynamicObjectDelegate;

import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.Projection;

import com.mysql.clusterj.core.CacheManager;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Db;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ResultData;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** This instance manages a persistence-capable type.
 * Currently, only interfaces or subclasses of DynamicObject can be persistence-capable.
 * Persistent properties consist of a pair of bean-pattern methods for which the
 * get method returns the same type as the parameter of the 
 * similarly-named set method.
 * @param T the class of the persistence-capable type
 */
public class DomainTypeHandlerImpl<T> extends AbstractDomainTypeHandlerImpl<T> {

    protected interface Finalizable {
        void finalize() throws Throwable;
    }

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

    /** The smart value handler factory */
    ValueHandlerFactory valueHandlerFactory;

    /* The column number is the entry indexed by field number */
    private int[] fieldNumberToColumnNumberMap;

    /* The number of transient (non-persistent) fields in the domain model */
    private int numberOfTransientFields;

    /* The field handlers for transient fields */
    private DomainFieldHandlerImpl[] transientFieldHandlers;

    /** Helper parameter for constructor. */
    protected static final Class<?>[] invocationHandlerClassArray = 
            new Class[]{InvocationHandler.class};

    /** Initialize DomainTypeHandler for a class.
     * 
     * @param cls the domain class (this is the only class known to the rest of the implementation)
     * @param dictionary NdbDictionary instance used for metadata access
     */
    public DomainTypeHandlerImpl(Class<T> cls, Dictionary dictionary) {
        this(cls, dictionary, null);
    }

    @SuppressWarnings( "unchecked" )
    public DomainTypeHandlerImpl(Class<T> cls, Dictionary dictionary,
            ValueHandlerFactory smartValueHandlerFactory) {
        this.valueHandlerFactory = smartValueHandlerFactory!=null?
                smartValueHandlerFactory:
                defaultInvocationHandlerFactory;
        this.cls = cls;
        this.name = cls.getName();
        if (DynamicObject.class.isAssignableFrom(cls)) {
            this.dynamic = true;
            // Dynamic object has a handler but no proxy
            this.tableName = getTableNameForDynamicObject((Class<DynamicObject>)cls);
        } else {
            // Create a proxy class for the domain class
            // Invoke the handler's finalizer method when the proxy is finalized
            proxyClass = (Class<T>)Proxy.getProxyClass(
                    cls.getClassLoader(), new Class[]{cls, Finalizable.class});
            ctor = getConstructorForInvocationHandler (proxyClass);
            persistenceCapable = cls.getAnnotation(PersistenceCapable.class);
            if (persistenceCapable == null) {
                throw new ClusterJUserException(local.message(
                        "ERR_No_Persistence_Capable_Annotation", name));
            }
            this.tableName = persistenceCapable.table();
            if (tableName.length() == 0) {
                throw new ClusterJUserException(local.message(
                        "ERR_No_TableAnnotation", name));
            }
        }
        List<String> columnNamesUsed = new ArrayList<String>();
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
        List<DomainFieldHandler> fieldHandlerList = new ArrayList<DomainFieldHandler>();

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
                fieldHandlerList.add(domainFieldHandler);
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
                    fieldHandlerList.add(domainFieldHandler);
                }
            }
            fieldNames = fieldNameList.toArray(new String[fieldNameList.size()]);
            // done with methods; if anything in unmatched we have a problem
            if ((!unmatchedGetMethods.isEmpty()) || (!unmatchedSetMethods.isEmpty())) {
                setUnsupported(
                        local.message("ERR_Unmatched_Methods", 
                        unmatchedGetMethods, unmatchedSetMethods));
            }

        }

        // Check that no errors were reported during field analysis
        String reasons = getUnsupported();
        if (reasons != null) {
            throw new ClusterJUserException(
                    local.message("ERR_Field_Construction", name, reasons.toString()));
        }

        // Check that all index columnNames have corresponding fields
        // indexes without fields will be unusable for query
        for (IndexHandlerImpl indexHandler:indexHandlerImpls) {
            indexHandler.assertAllColumnsHaveFields();
        }

        // Make sure that the PRIMARY index is usable
        // If not, this table has no primary key or there is no primary key field
        if (!indexHandlerImpls.get(0).isUsable()) {
            String reason = local.message("ERR_Primary_Field_Missing", name);
            logger.warn(reason);
            throw new ClusterJUserException(reason);
        }

        if (logger.isDebugEnabled()) {
            logger.debug(toString());
            logger.debug("DomainTypeHandlerImpl " + name + "Indices " + indexHandlerImpls);
        }

        // Compute the column for each field handler. 
        // If no column for this field, increment the transient field counter.
        // For persistent fields, column number is positive.
        // For transient fields, column number is negative (index into transient field handler).
        fieldNumberToColumnNumberMap = new int[numberOfFields];
        String[] columnNames = table.getColumnNames();
        this.fieldHandlers = fieldHandlerList.toArray(new DomainFieldHandlerImpl[numberOfFields]);

        int transientFieldNumber = 0;
        List<DomainFieldHandlerImpl> transientFieldHandlerList = new ArrayList<DomainFieldHandlerImpl>();
        for (int fieldNumber = 0; fieldNumber < numberOfFields; ++fieldNumber) {
            DomainFieldHandler fieldHandler = fieldHandlerList.get(fieldNumber);
            // find the column name for the field
            String columnName = fieldHandler.getColumnName();
            boolean found = false;
            for (int columnNumber = 0; columnNumber < columnNames.length; ++columnNumber) {
                if (columnNames[columnNumber].equals(columnName)) {
                    fieldNumberToColumnNumberMap[fieldNumber] = columnNumber;
                    found = true;
                    columnNamesUsed.add(columnNames[columnNumber]);
                    break;
                }
            }
            if (!found) {
                // no column for this field; it is a transient field
                fieldNumberToColumnNumberMap[fieldNumber] = --transientFieldNumber;
                transientFieldHandlerList.add((DomainFieldHandlerImpl) fieldHandler);
            }
        }
        // if projection, get list of projected fields and give them to the Table instance
        if (cls.getAnnotation(Projection.class) != null) {
            String[] projectedColumnNames = new String[columnNamesUsed.size()];
            columnNamesUsed.toArray(projectedColumnNames);
            table.setProjectedColumnNames(projectedColumnNames);
        }
        numberOfTransientFields = 0 - transientFieldNumber;
        transientFieldHandlers = 
            transientFieldHandlerList.toArray(new DomainFieldHandlerImpl[transientFieldHandlerList.size()]);
    }

    /** Get the table name mapped to the domain class.
     * @param cls the domain class
     * @return the table name for the domain class
     */
    @SuppressWarnings("unchecked")
    protected static String getTableName(Class<?> cls) {
        String tableName = null;
        if (DynamicObject.class.isAssignableFrom(cls)) {
            tableName = getTableNameForDynamicObject((Class<DynamicObject>)cls);
        } else {
            PersistenceCapable persistenceCapable = cls.getAnnotation(PersistenceCapable.class);
            if (persistenceCapable != null) {
                tableName = persistenceCapable.table();            
            }
        }
        return tableName;
    }

    /** Get the table name for a dynamic object. The table name is available either from
     * the PersistenceCapable annotation or via the table() method.
     * @param cls the dynamic object class
     * @return the table name for the dynamic object class
     */
    protected static <O extends DynamicObject> String getTableNameForDynamicObject(Class<O> cls) {
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
        ValueHandler handler = null;
        if (instance instanceof ValueHandler) {
            handler = (ValueHandler)instance;
        } else if (instance instanceof DynamicObject) {
            DynamicObject dynamicObject = (DynamicObject)instance;
            handler = (ValueHandler)dynamicObject.delegate();
        } else {
            handler = (ValueHandler)Proxy.getInvocationHandler(instance);
        }
        // make sure the value handler has not been released
        if (handler.wasReleased()) {
            throw new ClusterJUserException(local.message("ERR_Cannot_Access_Object_After_Release"));
        }
        return handler;
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
        objectSetKeys(keys, handler);
    }

    public void objectSetKeys(Object keys, ValueHandler handler) {
        int size = idFieldHandlers.length;
        if (size == 1) {
            // single primary key; store value in key field
            idFieldHandlers[0].objectSetKeyValue(keys, handler);
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

    public void objectSetCacheManager(CacheManager cm, Object instance) {
        getValueHandler(instance).setCacheManager(cm);
    }

    @Override
    public T newInstance(Db db) {
        ValueHandler valueHandler = valueHandlerFactory.getValueHandler(this, db);
        return newInstance(valueHandler);
    }

    /** Create a new domain type instance from the result.
     * @param resultData the results from a database query
     * @param db the Db
     * @return the domain type instance
     */
    public T newInstance(ResultData resultData, Db db) {
        ValueHandler valueHandler = valueHandlerFactory.getValueHandler(this, db, resultData);
        T result = newInstance(valueHandler);
        return result;
    }

    @Override
    public T newInstance(ValueHandler valueHandler) {
        T instance;
        try {
            if (dynamic) {
                instance = cls.newInstance();
                ((DynamicObject)instance).delegate((DynamicObjectDelegate)valueHandler);
            } else {
                instance = ctor.newInstance(new Object[] {valueHandler});
                // TODO is setProxy really needed?
                valueHandler.setProxy(instance);
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


    public void initializePrimitiveFields(ValueHandler handler) {
        for (DomainFieldHandler fmd:primitiveFieldHandlers) {
            ((AbstractDomainFieldHandlerImpl) fmd).objectSetDefaultValue(handler);
        }
        handler.resetModified();
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
    public T getInstance(ValueHandler valueHandler) {
        T instance = (T)valueHandler.getProxy();
        return instance;
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

    /** Create a key value handler from the key(s). The keys are as given by the user.
     * For domain classes with a single key field, the key is the object wrapper for the
     * primitive key value. For domain classes with compound primary keys, the key is
     * an Object[] in which the values correspond to the order of keys in the table
     * definition.
     * The key is validated for proper types, and if a key component is part of a partition key,
     * it must not be null.
     * @param keys the key(s)
     * @param db the Db
     * @return the key value handler
     */
    public ValueHandler createKeyValueHandler(Object keys, Db db) {
        if (keys == null) {
            throw new ClusterJUserException(
                    local.message("ERR_Key_Must_Not_Be_Null", getName(), "unknown"));
        }
        // check the cardinality of the keys with the number of key fields
        if (numberOfIdFields == 1) {
            Class<?> keyType = idFieldHandlers[0].getType();
            DomainFieldHandler fmd = idFieldHandlers[0];
            checkKeyType(fmd.getName(), keyType, keys);
        } else {
            if (!(keys.getClass().isArray())) {
                throw new ClusterJUserException(
                        local.message("ERR_Key_Must_Be_An_Object_Array",
                        numberOfIdFields));
            }
            for (int i = 0; i < numberOfIdFields; ++i) {
                DomainFieldHandler fmd = idFieldHandlers[i];
                Object keyObject = ((Object[])keys)[i];
                Class<?> keyType = fmd.getType();
                checkKeyType(fmd.getName(), keyType, keyObject);
                if (keyObject == null && fmd.isPartitionKey()) {
                    // partition keys must not be null
                    throw new ClusterJUserException(local.message("ERR_Key_Must_Not_Be_Null", getName(), 
                            fieldHandlers[i].getName()));
                }
            }
        }
        return valueHandlerFactory.getKeyValueHandler(this, db, keys);
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
                (keyType == Integer.class && valueType == int.class) ||
                (keyType == Long.class && valueType == long.class) ||
                (keyType == long.class && valueType == Long.class) ||
                (keyType == short.class && valueType == Short.class) ||
                (keyType == Short.class && valueType == short.class) ||
                (keyType == byte.class && valueType == Byte.class) ||
                (keyType == Byte.class && valueType == byte.class)) {
            return;
        } else {
                throw new ClusterJUserException(
                    local.message("ERR_Incorrect_Key_Type",
                    name, valueType.getName(), keyType.getName()));
        }
    }

    /** Expand the given Object or Object[] into an Object[] containing key values
     * in the proper position. The parameter is as given by the user. 
     * For domain classes with a single key field, the key is the object wrapper for the
     * primitive key value. For domain classes with compound primary keys, the key is
     * an Object[] in which the values correspond to the order of keys in the table
     * definition.
     * 
     * @param keys an object or Object[] containing all primary keys
     * @return an Object[] of length numberOfFields in which key values are in their proper position
     */
    public Object[] expandKeyValues(Object keys) {
        Object[] keyValues;
        if (keys instanceof Object[]) {
            keyValues = (Object[])keys;
        } else {
            keyValues = new Object[] {keys};
        }
        Object[] result = new Object[numberOfFields];
        int i = 0;
        for (Integer idFieldNumber: idFieldNumbers) {
            result[idFieldNumber] = keyValues[i++];
        }
        return result;
    }

    public Class<?> getOidClass() {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public ColumnMetadata[] columnMetadata() {
        ColumnMetadata[] result = new ColumnMetadata[numberOfFields];
        return persistentFieldHandlers.toArray(result);
    }

    /** Factory for default InvocationHandlerImpl */
    protected ValueHandlerFactory defaultInvocationHandlerFactory = new ValueHandlerFactory()  {

        public <V> ValueHandler getValueHandler(DomainTypeHandlerImpl<V> domainTypeHandler, Db db) {
            return new InvocationHandlerImpl<V>(domainTypeHandler);
        }

        public <V> ValueHandler getKeyValueHandler(
                DomainTypeHandlerImpl<V> domainTypeHandler, Db db, Object keyValues) {
            Object[] expandedKeyValues = expandKeyValues(keyValues);
            return new KeyValueHandlerImpl(expandedKeyValues);
        }

        public <V> ValueHandler getValueHandler(
                DomainTypeHandlerImpl<V> domainTypeHandler, Db db, ResultData resultData) {
            ValueHandler result = new InvocationHandlerImpl<V>(domainTypeHandler);
            objectSetValues(resultData, result);
            return result;
        }
    };

    public int getNumberOfTransientFields() {
        return numberOfTransientFields;
    }

    public int[] getFieldNumberToColumnNumberMap() {
        return fieldNumberToColumnNumberMap;
    }

    /** Create a new object array containing default values for all transient fields.
     * 
     * @return default transient field values
     */
    public Object[] newTransientValues() {
        Object[] result = new Object[numberOfTransientFields];
        int i = 0;
        for (DomainFieldHandlerImpl transientFieldHandler: transientFieldHandlers) {
            Object value = transientFieldHandler.getDefaultValue();
            result[i++] = value;
        }
        return result;
    }

}
