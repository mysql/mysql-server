/*
   Copyright (C) 2009-2010 Sun Microsystems Inc.
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

package com.mysql.clusterj.core.metadata;

import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJUserException;

import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.Column;
import com.mysql.clusterj.annotation.Indices;

import com.mysql.clusterj.core.CacheManager;

import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.IndexOperation;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.query.CandidateIndexImpl;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

import java.util.ArrayList;
import java.util.BitSet;
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
public class DomainTypeHandlerImpl<T> implements DomainTypeHandler<T> {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(DomainTypeHandlerImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(DomainTypeHandlerImpl.class);

    /** The domain class. */
    Class<T> cls;

    /** The name of the class. */
    String className;

    /** The table for the class. */
    String tableName;

    /** The NDB table for the class. */
    Table table;

    /** The NDB table for the class. */
    Dictionary dictionary;

    /** The PrimaryKey column names. */
    String[] primaryKeyColumnNames;

    /** The id field(s) for the class, mapped to primary key columns */
    DomainFieldHandlerImpl[] idFieldHandlers;

    /** The names of the partition key columns */
    private String[] partitionKeyColumnNames;

    /** The number of partition key columns */
    int numberOfPartitionKeyColumns = 0;

    /** The partition key fields */
    private DomainFieldHandlerImpl[] partitionKeyFieldHandlers;

    /** The field numbers of the id fields. */
    private int[] idFieldNumbers;

    /** The number of id fields for the class. */
    private int numberOfPrimaryKeyColumns;

    /** The types of the properties. */
    private Map<String, Method> unmatchedGetMethods = new HashMap<String, Method>();
    private Map<String, Method> unmatchedSetMethods = new HashMap<String, Method>();

    /** The number of fields. Dynamically created as fields are added. */
    int numberOfFields = 0;

    /** Persistent fields. */
    private List<DomainFieldHandlerImpl> persistentFieldHandlers =
            new ArrayList<DomainFieldHandlerImpl>();

    /** Primitive fields. */
    private List<DomainFieldHandlerImpl> primitiveFieldHandlers =
            new ArrayList<DomainFieldHandlerImpl>();

    /** Map of field names to field numbers. */
    private Map<String, Integer> fieldNameToNumber = new HashMap<String, Integer>();

    /** The Proxy class for the Domain Class. */
    protected Class<T> proxyClass;

    /** The constructor for the Proxy class. */
    Constructor<T> ctor;

    /** The PersistenceCapable annotation for this class. */
    PersistenceCapable persistenceCapable;

    /** Helper parameter for constructor. */
    protected static final Class<?>[] invocationHandlerClassArray = 
            new Class[]{InvocationHandler.class};

    /** All index handlers defined for the mapped class. The position in this
     * array is significant. Each DomainFieldHandlerImpl contains the index into
     * this array and the index into the fields array within the
     * IndexHandlerImpl.
     */
    protected List<IndexHandlerImpl> indexHandlerImpls =
            new ArrayList<IndexHandlerImpl>();

    /** Index names to check for duplicates. */
    protected Set<String> indexNames = new HashSet<String>();

    /** Initialize DomainTypeHandler for a class.
     * 
     * @param cls the domain class (this is the only class 
     * known to the rest of the implementation)
     * @param dictionary NdbDictionary instance used for metadata access
     */
    @SuppressWarnings( "unchecked" )
    public DomainTypeHandlerImpl(Class<T> cls, Dictionary dictionary) {
        if (logger.isDebugEnabled()) logger.debug("New DomainTypeHandlerImpl for class " + cls.getName());
        this.cls = cls;
        this.className = cls.getName();
        // Create a proxy class for the domain class
        proxyClass = (Class<T>)Proxy.getProxyClass(
                cls.getClassLoader(), new Class[]{cls});
        ctor = getConstructorForInvocationHandler (proxyClass);
        persistenceCapable = cls.getAnnotation(PersistenceCapable.class);
        if (persistenceCapable == null) {
            throw new ClusterJFatalUserException(local.message(
                    "ERR_No_Persistence_Capable_Annotation", className));
        }
        tableName = persistenceCapable.table();
        this.dictionary = dictionary;
        table = getTable(dictionary);
        logger.debug("Found Table for " + tableName);

        primaryKeyColumnNames = table.getPrimaryKeyColumnNames();

        // the id field handlers will be initialized via registerPrimaryKeyColumn
        numberOfPrimaryKeyColumns  = primaryKeyColumnNames.length;
        idFieldHandlers = new DomainFieldHandlerImpl[numberOfPrimaryKeyColumns];
        idFieldNumbers = new int[numberOfPrimaryKeyColumns];

        if (logger.isDebugEnabled()) logger.debug(toString());
        // TODO handle the case where there is only a hash primary key
        // First two entries in indexHandlerImpls represent the primary key.
        IndexHandlerImpl primaryKeyIndexHandler =
                new IndexHandlerImpl(this, dictionary, "PRIMARY", primaryKeyColumnNames);
        indexHandlerImpls.add(primaryKeyIndexHandler);
        // now add the twinned unique primary key index
        indexHandlerImpls.add(new IndexHandlerImpl(dictionary, primaryKeyIndexHandler));

        // set up the partition keys
        // the partition key field handlers will be initialized via registerPrimaryKeyColumn
        partitionKeyColumnNames = table.getPartitionKeyColumnNames();
        numberOfPartitionKeyColumns = partitionKeyColumnNames.length;
        partitionKeyFieldHandlers = new DomainFieldHandlerImpl[numberOfPartitionKeyColumns];

        // Process index annotation on the class. There might not be a field associated with the index.
        Indices indicesAnnotation = cls.getAnnotation(Indices.class);
        if (indicesAnnotation != null) {
            com.mysql.clusterj.annotation.Index[] indexList = indicesAnnotation.value();
            for (com.mysql.clusterj.annotation.Index indexAnnotation : indexList) {
                String indexName = indexAnnotation.name();
                if (indexNames.contains(indexName)) {
                    throw new ClusterJUserException(
                        local.message("ERR_Duplicate_Index", className, indexName));
                } else {
                    indexNames.add(indexName);
                }
                String[] columnNames = getColumnNames(indexName, indexAnnotation.columns());
                IndexHandlerImpl imd = new IndexHandlerImpl(this, dictionary, indexName, columnNames);
                indexHandlerImpls.add(imd);
                // for unique indexes, add the twin
                if (imd.isTwinned(dictionary)) {
                    IndexHandlerImpl twin = new IndexHandlerImpl(dictionary, imd);
                    indexHandlerImpls.add(twin);
                }
            }
        }

        // Now iterate the fields in the class
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
                fieldNameToNumber.put(domainFieldHandler.getName(), domainFieldHandler.getFieldNumber());
                // put field into either persistent or not persistent list
                if (domainFieldHandler.isPersistent()) {
                    persistentFieldHandlers.add(domainFieldHandler);
                }
                if (domainFieldHandler.isPrimitive()) {
                    primitiveFieldHandlers.add(domainFieldHandler);
                }
            }
        }
        // done with methods; if anything in unmatched we have a problem
        if ((!unmatchedGetMethods.isEmpty()) || (!unmatchedSetMethods.isEmpty())) {
            throw new ClusterJUserException(
                    local.message("ERR_Unmatched_Methods", 
                    unmatchedGetMethods, unmatchedSetMethods));
        }

        // Check that all index columnNames have corresponding fields
        for (IndexHandlerImpl indexHandler:indexHandlerImpls) {
            indexHandler.assertAllColumnsHaveFields();
        }
        logger.debug("DomainTypeHandlerImpl " + className +
                "Indices " + indexHandlerImpls);
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

    /** Register a primary key column field. This is used to associate
     * primary key and partition key column names with field handlers.
     * This method is called by the DomainFieldHandlerImpl constructor
     * after the mapped column name is known. It is only called by fields
     * that are mapped to primary key columns.
     * @param fmd the field handler instance calling us
     * @param columnName the name of the column
     */
    protected void registerPrimaryKeyColumn(DomainFieldHandlerImpl fmd,
            String columnName) {
        // find the primary key column that matches the primary key column
        for (int i = 0; i < primaryKeyColumnNames.length; ++i) {
            if (primaryKeyColumnNames[i].equals(columnName)) {
                idFieldHandlers[i] = fmd;
                idFieldNumbers[i] = fmd.fieldNumber;
            }
        }
        // find the partition key column that matches the primary key column
        for (int j = 0; j < partitionKeyColumnNames.length; ++j) {
            if (partitionKeyColumnNames[j].equals(columnName)) {
                partitionKeyFieldHandlers[j] = fmd;
            }
        }
        return;
    }

    /** Create and register an index from a field and return a special int[][] that
     * contains all indexHandlerImpls in which the
     * field participates. One index can be defined in the field annotation
     * and this index is identified here. This method is called by the
     * FieldHandler constructor after the Index annotation is processed
     * and the mapped column name is known.
     * @see AbstractDomainFieldHandlerImpl#indices
     * @param fmd the FieldHandler
     * @param columnName the column name mapped to the field
     * @param indexName the index name
     * or null if no index annotation defined on the field
     * @return the array of array identifying the indexes into the IndexHandler
     * list and columns in the IndexHandler corresponding to the field
     */
    protected int[][] registerIndices(
            AbstractDomainFieldHandlerImpl fmd, String columnName, String indexName) {
        if (indexName != null) {
            IndexHandlerImpl indexHandler =
                    new IndexHandlerImpl(this, dictionary, indexName, fmd);
            // Add the index to the list of indexes on the class
            indexHandlerImpls.add(indexHandler);
            if (indexHandler.isTwinned(dictionary)) {
                indexHandlerImpls.add(new IndexHandlerImpl(dictionary, indexHandler));
            }
        }

        // Find all the indexes that this field belongs to, by iterating
        // the list of indexes and comparing column names.
        List<int[]> result =new ArrayList<int[]>();
        for (int i = 0; i < indexHandlerImpls.size(); ++i) {
            IndexHandlerImpl indexHandler = indexHandlerImpls.get(i);
            String[] columns = indexHandler.getColumnNames();
            for (int j = 0; j < columns.length; ++j) {
                if (fmd.getColumnName().equals(columns[j])) {
                    if (logger.isDetailEnabled()) logger.detail("Found field " + fmd.getName() + " column " + fmd.getColumnName() + " matching " + indexHandler.indexName);
                    indexHandler.setDomainFieldHandlerFor(j, fmd);
                    result.add(new int[]{i,j});
                }
            }
        }

        if (logger.isDebugEnabled()) logger.debug("Found " + result.size() + " indexes for " + columnName);
        return result.toArray(new int[result.size()][]);
    }

    /** Extract the column names from a Columns annotation. This is used for
     * Index annotations and PrimaryKey annotations.
     * 
     * @param indexName the index name (for error messages)
     * @param columns the Columns from the annotation
     * @return an array of column names
     */
    protected String[] getColumnNames(String indexName, Column[] columns) {
        Set<String> columnNames = new HashSet<String>();
        for (Column column : columns) {
            String columnName = column.name();
            if (columnNames.contains(columnName)) {
                // error: the column name is duplicated
                throw new ClusterJUserException(
                        local.message("ERR_Duplicate_Column",
                        className, indexName, columnName));
            }
            columnNames.add(columnName);
        }
        return columnNames.toArray(new String[columnNames.size()]);
    }

    /** Return the list of index names corresponding to the parameter indexHandlerImpls.
     * This method is called by the DomainFieldHandlerImpl constructor after the
     * registerIndices method is called.
     * @param indexArray the result of registerIndices
     * @return all index names for the corresponding indexHandlerImpls
     */
    public Set<String> getIndexNames(int[][] indexArray) {
        Set<String> result = new HashSet<String>();
        for (int[] index: indexArray) {
            result.add(indexHandlerImpls.get(index[0]).indexName);
        }
        return result;
    }

    /** Create a list of candidate indexes to evaluate query terms and
     * decide what type of scan to use.
     * @return a new array of CandidateIndexImpl
     */
    public CandidateIndexImpl[] createCandidateIndexes() {
        CandidateIndexImpl[] result = new CandidateIndexImpl[indexHandlerImpls.size()];
        int i = 0;
        for (IndexHandlerImpl indexHandler: indexHandlerImpls) {
            result[i++] = indexHandler.toCandidateIndexImpl();
        }
        return result;
    }

    public String getName() {
        return className;
    }

    public String getTableName() {
        return tableName;
    }

    public int getNumberOfFields() {
        return numberOfFields;
    }

    public Class<T> getProxyClass() {
        return proxyClass;
    }

    public Class<T> getDomainClass() {
        return cls;
    }

    public DomainFieldHandlerImpl[] getIdFieldHandlers() {
        return idFieldHandlers;
    }

    public DomainFieldHandlerImpl getFieldHandler(String fieldName) {
        for (DomainFieldHandlerImpl fmd: persistentFieldHandlers) {
            if (fmd.getName().equals(fieldName)) {
                return fmd;
            }
        }
        throw new ClusterJUserException(
                local.message("ERR_Not_A_Member", fieldName, cls.getName()));
    }

    public int getFieldNumber(String fieldName) {
        Integer fieldNumber = fieldNameToNumber.get(fieldName);
        if (fieldNumber == null) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_No_Field_Number", fieldName, cls.getName()));
        }
        return fieldNumber.intValue();
    }

    public void operationSetValues(Object instance, Operation op) {
        ValueHandler handler = getValueHandler(instance);
        for (DomainFieldHandlerImpl fmd: persistentFieldHandlers) {
            fmd.operationSetValue(handler, op);
        }
    }

    public void operationSetValuesExcept(ValueHandler handler, Operation op, String index) {
        for (DomainFieldHandlerImpl fmd: persistentFieldHandlers) {
            if (!fmd.includedInIndex(index)) {
                if (logger.isDetailEnabled()) logger.detail("operationSetValuesExcept field: " + fmd.name + " is not included in index: " + index);
                fmd.operationSetValue(handler, op);
            }
        }
    }

    public void operationSetModifiedValuesExcept(ValueHandler handler, Operation op, String index) {
        for (DomainFieldHandlerImpl fmd: persistentFieldHandlers) {
            if (!fmd.includedInIndex(index)) {
                if (logger.isDetailEnabled()) logger.detail("operationSetModifiedValuesExcept index: " + index);
                fmd.operationSetModifiedValue(handler, op);
            }
        }
    }

    public void operationSetModifiedValues(ValueHandler handler, Operation op) {
        for (DomainFieldHandlerImpl fmd: persistentFieldHandlers) {
            fmd.operationSetModifiedValue(handler, op);
        }
    }

    public void operationSetKeys(ValueHandler handler, Operation op) {
        for (DomainFieldHandlerImpl fmd: idFieldHandlers) {
            fmd.operationSetValue(handler, op);
        }        
    }

    public void operationGetValues(Operation op) {
        for (DomainFieldHandlerImpl fmd: persistentFieldHandlers) {
            fmd.operationGetValue(op);
        }
    }

    public void operationGetValues(Operation op, BitSet fields) {
        if (fields == null) {
            operationGetValues(op);
        } else {
            int i = 0;
            for (DomainFieldHandlerImpl fmd: persistentFieldHandlers) {
                if (fields.get(i++)) {
                    fmd.operationGetValue(op);
                }
            }
        }
    }

    public void operationGetValuesExcept(IndexOperation op, String index) {
        for (DomainFieldHandlerImpl fmd: persistentFieldHandlers) {
            if (!fmd.includedInIndex(index)) {
                if (logger.isDetailEnabled()) logger.detail("operationGetValuesExcept index: " + index);
                fmd.operationGetValue(op);
            }
        }
    }

    public void objectSetValues(ResultData rs, ValueHandler handler) {
        for (DomainFieldHandlerImpl fmd: persistentFieldHandlers) {
            fmd.objectSetValue(rs, handler);
        }
    }

    public void objectSetValuesExcept(
            ResultData rs, ValueHandler handler, String indexName) {
        for (DomainFieldHandlerImpl fmd: persistentFieldHandlers) {
            fmd.objectSetValueExceptIndex(rs, handler, indexName);
        }
    }

    public void objectSetKeys(Object keys, Object instance) {
        ValueHandler handler = getValueHandler(instance);
        int size = idFieldHandlers.length;
        if (size == 1) {
            // single primary key; store value in key field
            for (DomainFieldHandlerImpl fmd: idFieldHandlers) {
                fmd.objectSetValue(keys, handler);
            }
        } else if (keys instanceof java.lang.Object[]) {
            if (logger.isDetailEnabled()) logger.detail(keys.toString());
            // composite primary key; store values in key fields
            for (int i = 0; i < idFieldHandlers.length; ++i) {
                idFieldHandlers[i].objectSetValue(((Object[])keys)[i], handler);
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
        try {
            InvocationHandlerImpl<T> handler = new InvocationHandlerImpl<T>(this);
            T proxy = ctor.newInstance(new Object[] {handler});
            handler.setProxy(proxy);
            return proxy;
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
        for (DomainFieldHandlerImpl fmd:primitiveFieldHandlers) {
            fmd.objectSetDefaultValue(handler);
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

    private Table getTable(Dictionary dictionary) {
        Table result;
        try {
            result = dictionary.getTable(tableName);
        } catch (Exception ex) {
            throw new ClusterJException(
                    local.message("ERR_Get_NdbTable", className, tableName), ex);
        }
        return result;
    }

    @SuppressWarnings( "unchecked" )
    public T getInstance(ValueHandler handler) {
        return (T)((InvocationHandlerImpl)handler).getProxy();
    }

    protected Index getIndex(String indexName) {
        Index result;
        String uniqueIndexName = indexName + "$unique";
        try {
            result = dictionary.getIndex(indexName, tableName, indexName);
        } catch (Exception ex1) {
            // index name failed; try unique index name
            try {
                result = dictionary.getIndex(uniqueIndexName, tableName, indexName);
            } catch (Exception ex2) {
                // fall through
            }
            throw new ClusterJException(
                    local.message("ERR_Get_NdbIndex", 
                    tableName, indexName, uniqueIndexName), ex1);
        }
        return result;
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
        if (numberOfPrimaryKeyColumns == 1) {
            Class<?> keyType = idFieldHandlers[0].getType();
            DomainFieldHandlerImpl fmd = idFieldHandlers[0];
            checkKeyType(fmd.getName(), keyType, keys);
            int keyFieldNumber = fmd.getFieldNumber();
            keyValues[keyFieldNumber] = keys;
        } else {
            if (!(keys.getClass().isArray())) {
                throw new ClusterJUserException(
                        local.message("ERR_Key_Must_Be_An_Object_Array",
                        numberOfPrimaryKeyColumns));
            }
            Object[]keyObjects = (Object[])keys;
            for (int i = 0; i < numberOfPrimaryKeyColumns; ++i) {
                DomainFieldHandlerImpl fmd = idFieldHandlers[i];
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

    public int[] getKeyFieldNumbers() {
        return idFieldNumbers;
    }

    public Class<?> getOidClass() {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public Set<String> getColumnNames(BitSet fields) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public Set<com.mysql.clusterj.core.store.Column> getStoreColumns(BitSet fields) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public Table getStoreTable() {
        return table;
    }

    /** Create a partition key for a find by primary key. 
     * @param clusterTransaction the transaction
     * @param handler the handler that contains the values of the primary key
     */
    public PartitionKey createPartitionKey(ValueHandler handler) {
        // create the partition key based on the mapped table
        PartitionKey result = table.createPartitionKey();
        // add partition key part value for each partition key field
        for (DomainFieldHandlerImpl fmd: partitionKeyFieldHandlers) {
            if (logger.isDetailEnabled()) logger.detail(
                        "Field number " + fmd.getFieldNumber()
                        + " column name " + fmd.getColumnName() + " field name " + fmd.getName());
            fmd.partitionKeySetPart(result, handler);
        }
        return result;
    }

}
