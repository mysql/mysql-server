/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

import java.lang.reflect.Modifier;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.BitSet;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.apache.openjpa.jdbc.kernel.JDBCFetchConfiguration;
import org.apache.openjpa.jdbc.meta.ClassMapping;
import org.apache.openjpa.jdbc.meta.FieldMapping;
import org.apache.openjpa.kernel.OpenJPAStateManager;
import org.apache.openjpa.kernel.PCState;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.core.CacheManager;
import com.mysql.clusterj.core.metadata.IndexHandlerImpl;
import com.mysql.clusterj.core.metadata.KeyValueHandlerImpl;
import com.mysql.clusterj.core.query.CandidateIndexImpl;
import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.Table;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 *
 */
public class NdbOpenJPADomainTypeHandlerImpl<T> implements DomainTypeHandler<T> {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(NdbOpenJPADomainTypeHandlerImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(NdbOpenJPADomainTypeHandlerImpl.class);

    private String typeName;
    private Class<T> describedType;
    private Class<?> oidClass;
    private Table storeTable;
    private String tableName;
    private List<NdbOpenJPADomainFieldHandlerImpl> primaryKeyFields =
            new ArrayList<NdbOpenJPADomainFieldHandlerImpl>();
    private int[] primaryKeyFieldNumbers;
    private List<NdbOpenJPADomainFieldHandlerImpl> fields =
            new ArrayList<NdbOpenJPADomainFieldHandlerImpl>();
    private ClassMapping classMapping;

    /** The field handlers for this persistent type*/
    private NdbOpenJPADomainFieldHandlerImpl[] fieldHandlers;

    /** All columns in the mapped table. */
    private Set<com.mysql.clusterj.core.store.Column> allStoreColumns = 
        new HashSet<com.mysql.clusterj.core.store.Column>();

    /** The indexes defined for this domain type */
    private List<IndexHandlerImpl> indexHandlerImpls = new ArrayList<IndexHandlerImpl>();

    private String[] partitionKeyColumnNames;

    private int numberOfPartitionKeyColumns;

    private NdbOpenJPADomainFieldHandlerImpl[] partitionKeyFieldHandlers;

    private NdbOpenJPAConfigurationImpl domainTypeHandlerFactory;

    private Dictionary dictionary;

    private Set<NdbOpenJPADomainTypeHandlerImpl<?>> dependencies =
        new HashSet<NdbOpenJPADomainTypeHandlerImpl<?>>();

    private Status status = Status.IN_PROCESS;

    /** Reasons why this class is not supported by clusterjpa */
    private List<String> reasons = new ArrayList<String>();

    @SuppressWarnings("unchecked")
    NdbOpenJPADomainTypeHandlerImpl(
            Dictionary dictionary, ClassMapping classMapping, 
            NdbOpenJPAConfigurationImpl domainTypeHandlerFactory) {
        String message = null;
        this.dictionary = dictionary;
        this.domainTypeHandlerFactory = domainTypeHandlerFactory;
        this.classMapping = classMapping;
        classMapping.resolve(0xffffffff);
        oidClass = classMapping.getObjectIdType();
        this.describedType = (Class<T>)classMapping.getDescribedType();
        if (classMapping.getPCSuperclass() != null) {
            // persistent subclasses are not supported
            message = local.message("ERR_Subclass", this.describedType);
            setUnsupported(message);
        }
        if (classMapping.getPCSubclasses() != null && classMapping.getPCSubclasses().length > 0) {
            // persistent superclasses are not supported
            message = local.message("ERR_Superclass", this.describedType);
            setUnsupported(message);
        }
        int modifiers = describedType.getClass().getModifiers();
        if (Modifier.isAbstract(modifiers)) {
            // abstract classes are not supported
            message = local.message("ERR_Abstract_Class", describedType.getClass().getName());
            setUnsupported(message);
        }
        this.typeName = describedType.getName();
        org.apache.openjpa.jdbc.schema.Table table = classMapping.getTable();
        if (table != null) {
            this.tableName = table.getFullName();
            this.storeTable = dictionary.getTable(tableName);
            if (storeTable == null) {
                // classes with mapped tables not in cluster are not supported
                message = local.message("ERR_No_Table", describedType.getClass().getName(), tableName);
                logger.info(message);
                setUnsupported(message);
            } else {
                if (logger.isTraceEnabled()) {
                    logger.trace("initialize for class: " + typeName
                            + " mapped to table: " + storeTable.getName());
                }
                // set up the partition keys
                // the partition key field handlers will be initialized via registerPrimaryKeyColumn
                partitionKeyColumnNames = storeTable.getPartitionKeyColumnNames();
                numberOfPartitionKeyColumns = partitionKeyColumnNames.length;
                partitionKeyFieldHandlers = new NdbOpenJPADomainFieldHandlerImpl[numberOfPartitionKeyColumns];
                if (logger.isDetailEnabled()) {
                    logger.detail("partition key columns for class: "+ typeName
                            + " partition key columns: " + numberOfPartitionKeyColumns
                            + " names: " + Arrays.toString(partitionKeyColumnNames));
                }
            }
        } else {
            // classes without mapped tables are not supported
            message = local.message("ERR_No_Mapped_Table", describedType.getClass().getName());
            logger.info(message);
            setUnsupported(message);
        }
        // set up the fields
        List<FieldMapping> allFieldMappings = new ArrayList<FieldMapping>();
        allFieldMappings.addAll(Arrays.asList(classMapping.getFieldMappings()));
        FieldMapping versionFieldMapping = classMapping.getVersionFieldMapping();
        if (versionFieldMapping != null) {
            allFieldMappings.add(versionFieldMapping);
        }
        for (FieldMapping fm: allFieldMappings) {
            if (logger.isDetailEnabled()) logger.detail("field name: " + fm.getName() + " of type: " + fm.getTypeCode());
            NdbOpenJPADomainFieldHandlerImpl fmd = new NdbOpenJPADomainFieldHandlerImpl(
                    dictionary, this, domainTypeHandlerFactory, fm);
            fields.add(fmd);
            if (!fmd.isSupported()) {
                setUnsupported(fmd.getReason());
            } else {
                // add column names to allColumnNames
                for (com.mysql.clusterj.core.store.Column column: fmd.getStoreColumns()) {
                    allStoreColumns.add(column);
                }
                if (fmd.isPrimaryKey()) {
                    primaryKeyFields.add(fmd);
                }
            }
        }
        primaryKeyFieldNumbers = new int[primaryKeyFields.size()];
        int i = 0;
        for (NdbOpenJPADomainFieldHandlerImpl fmd: primaryKeyFields) {
            primaryKeyFieldNumbers[i++] = fmd.getFieldNumber();
        }
        fieldHandlers = fields.toArray(new NdbOpenJPADomainFieldHandlerImpl[fields.size()]);
        // check to make sure all partition keys have registered
        for (int j = 0; j < numberOfPartitionKeyColumns; ++j) {
            if (partitionKeyFieldHandlers[j] == null) {
                setUnsupported();
                reasons.add("Unmapped partition key " + partitionKeyColumnNames[j]);
            }
        }
    }

    /** Register a primary key column field. This is used to associate
     * primary key and partition key column names with field handlers.
     * This method is called by the NdbOpenJPADomainFieldHandlerImpl constructor
     * after the mapped column name is known. It is only called by fields
     * that are mapped to primary key columns.
     * @param fmd the field handler instance calling us
     * @param columnName the name of the column
     */
    protected void registerPrimaryKeyColumn(NdbOpenJPADomainFieldHandlerImpl fmd,
            String columnName) {
        // find the partition key column that matches the primary key column
        for (int j = 0; j < partitionKeyColumnNames.length; ++j) {
            if (partitionKeyColumnNames[j].equals(columnName)) {
                partitionKeyFieldHandlers[j] = fmd;
            } else {
                if (logger.isDetailEnabled()) logger.detail(
                        "NdbOpenJPADomainTypeHandlerImpl.registerPrimaryKeyColumn "
                        + "mismatch between partition key column name: " + partitionKeyColumnNames[j]
                        + " and primary key column name: " + columnName);
            }
        }
        return;
    }

    public String getName() {
        return typeName;
    }

    public Class<?> getOidClass() {
        return oidClass;
    }

    public String getTableName() {
        return tableName;
    }

    public DomainFieldHandler getFieldHandler(String fieldName) {
        if (logger.isDetailEnabled()) logger.detail("In " + getName() + " looking for " + fieldName);
        for (NdbOpenJPADomainFieldHandlerImpl domainFieldHandler: fields) {
            if (fieldName.equals(domainFieldHandler.getName())) {
                return domainFieldHandler;
            }
        }
        throw new ClusterJFatalInternalException(
                local.message("ERR_Unknown_Field_Name", fieldName, this.getName()));
    }

    public Class<T> getProxyClass() {
        return null;
    }

    public T newInstance() {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void objectMarkModified(ValueHandler handler, String fieldName) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void objectSetKeys(Object keys, Object instance) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void objectSetValues(ResultData rs, ValueHandler handler) {
        for (NdbOpenJPADomainFieldHandlerImpl fmd: fields) {
            fmd.objectSetValue(rs, handler);
        }
    }

    public void objectSetValuesExcept(ResultData rs, ValueHandler handler, String indexName) {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void objectSetCacheManager(CacheManager cm, Object instance) {
    }

    public void objectResetModified(ValueHandler handler) {
    }

    public void operationGetValues(Operation op) {
        for (NdbOpenJPADomainFieldHandlerImpl fmd: fields) {
            fmd.operationGetValue(op);
        }
    }

    /** Specify that the key columns are to be returned in the result.
     * @param op the operation
     */
    public void operationGetKeys(Operation op) {
        for (NdbOpenJPADomainFieldHandlerImpl fmd: primaryKeyFields) {
            fmd.operationGetValue(op);
        }
    }

    /** Specify the fields to be returned in the result.
     * @param op the operation
     * @param fields the fields to be returned by the operation
     */
    public void operationGetValues(Operation op, BitSet fields) {
        if (fields == null) {
            operationGetValues(op);
        } else {
            int i = 0;
            for (NdbOpenJPADomainFieldHandlerImpl fmd: this.fields) {
                if (fields.get(i++)) {
                    fmd.operationGetValue(op);
                }
            }
        }
    }

    public void operationSetKeys(ValueHandler handler, Operation op) {
        for (NdbOpenJPADomainFieldHandlerImpl fmd: primaryKeyFields) {
            if (logger.isDetailEnabled()) {
                logger.detail("Class: " + typeName
                        + " Primary Key Field: " + fmd.getName() + handler.pkToString(this));
            }
            fmd.operationSetValue(handler, op);
        }
    }

    public void operationSetModifiedValues(ValueHandler handler, Operation op) {
        for (NdbOpenJPADomainFieldHandlerImpl fmd: fields) {
            fmd.operationSetModifiedValue(handler, op);
        }
    }

    public void operationSetValuesExcept(ValueHandler handler, Operation op, String index) {
        try {
            for (NdbOpenJPADomainFieldHandlerImpl fmd : fields) {
                if (!fmd.includedInIndex(index)) {
                    fmd.operationSetValue(handler, op);
                }
            }
        } catch (Exception exception) {
            exception.printStackTrace();
            throw new RuntimeException("NdbOpenJPADomainTypeHandlerImpl.operationSetValuesExcept caught exception", exception);
        }
    }

    public void operationSetModifiedNonPKValues(ValueHandler handler, Operation op) {
        try {
            for (NdbOpenJPADomainFieldHandlerImpl fmd : fields) {
                if (!fmd.isPrimaryKey()) {
                    fmd.operationSetModifiedValue(handler, op);
                }
            }
        } catch (Exception exception) {
            exception.printStackTrace();
            throw new RuntimeException("NdbOpenJPADomainTypeHandlerImpl.operationSetModifiedValuesExcept caught " + exception);
        }
    }

    @SuppressWarnings("unchecked")
    public T getInstance(ValueHandler handler) {
        OpenJPAStateManager sm = ((NdbOpenJPAValueHandler)handler).getStateManager();
        sm.initialize(describedType, PCState.PNONTRANS);
        Object instance= sm.getManagedInstance();
        // System.out.println("NdbOpenJPADomainTypeHandlerImpl.getInstance returned " + instance.getClass() + " " + instance);
        return (T) instance;
    }

    public ValueHandler createKeyValueHandler(Object keys) {

        FieldMapping[] primaryKeyFieldMappings =
                classMapping.getPrimaryKeyFieldMappings();
        int numberOfFields = classMapping.getFieldMappings().length;
        Object[] keyValues = new Object[numberOfFields];
        boolean nullKeyValue = false;
        if (primaryKeyFieldMappings.length != 1) {
        // for each key field, use the field value accessor to get the
        // value from the Oid and put it into the proper place in keyValues
            for (NdbOpenJPADomainFieldHandlerImpl fmd: primaryKeyFields) {
                // store the key value from the oid into the keyValues array
                // this can be improved with a smarter KeyValueHandlerImpl
                Object keyValue = fmd.getKeyValue(keys);
                keyValues[fmd.getFieldNumber()] = keyValue;
                if (keyValue == null) {
                    nullKeyValue = true;
                }
            }
        } else {
            keyValues[primaryKeyFieldMappings[0].getIndex()] = keys;
        }
        KeyValueHandlerImpl keyHandler = new KeyValueHandlerImpl(keyValues);
        return nullKeyValue?null:keyHandler;
    }

    public ValueHandler getValueHandler(Object instance) {
        if (instance instanceof ValueHandler) {
            return (ValueHandler)instance;
        } else {
            OpenJPAStateManager sm = (OpenJPAStateManager)instance;
            return new NdbOpenJPAValueHandler(sm);
        }
    }

    public ValueHandler getValueHandler(OpenJPAStateManager sm, NdbOpenJPAStoreManager store) {
        return new NdbOpenJPAValueHandler(sm, store);
    }

    public int[] getKeyFieldNumbers() {
        return primaryKeyFieldNumbers;
    }

    public void newInstance(OpenJPAStateManager sm) {
        // TODO: accommodate subclasses based on results of select
        sm.initialize(describedType, PCState.PNONTRANS);
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

    /** Load the fields for the persistent instance owned by the sm.
     * @param sm the StateManager
     * @param store the StoreManager
     * @param fields the fields to load
     * @param fetch the FetchConfiguration
     * @return true if any field was loaded
     */
    public boolean load(OpenJPAStateManager sm, NdbOpenJPAStoreManager store, 
            BitSet fields, JDBCFetchConfiguration fetch, Object context) throws SQLException {
        if (logger.isDetailEnabled()) {
            StringBuilder buffer = new StringBuilder("load for ");
            buffer.append(typeName);
            buffer.append(" for fields ");
            buffer.append(NdbOpenJPAUtility.printBitSet(sm, fields));
            logger.detail(buffer.toString());
        }
        boolean loadedAny = false;
        List<NdbOpenJPADomainFieldHandlerImpl> requestedFields = new ArrayList<NdbOpenJPADomainFieldHandlerImpl>();
        for (int i=fields.nextSetBit(0); i>=0; i=fields.nextSetBit(i+1)) {
            if (!sm.getLoaded().get(i)) {
                // this field is not loaded
                NdbOpenJPADomainFieldHandlerImpl fieldHandler = fieldHandlers[i];
                if (logger.isDebugEnabled()) logger.debug(
                        "loading field " + fieldHandler.getName()
                        + " for column " + fieldHandler.getColumnName());
                if (fieldHandler.isRelation()) {
                    // if relation, load each field individually with its own query
                    fieldHandler.load(sm, store, fetch);
                    loadedAny = true;
                } else {
                    // if not relation, get a list of all fields to load with one lookup
                    requestedFields.add(fieldHandler);
                }
            }
        }
        if (requestedFields.size() != 0) {
            // load fields via primary key lookup
            NdbOpenJPAResult result = store.lookup(sm, this, requestedFields);
            for (NdbOpenJPADomainFieldHandlerImpl fieldHandler: fieldHandlers) {
                loadedAny = true;
                fieldHandler.load(sm, store, fetch, result);
            }
        }
        return loadedAny;
    }

    /** Load the fields for the persistent instance owned by the sm.
     * This is the normal way for fields to be loaded into the instance from a result.
     * @param sm the StateManager
     * @param store the StoreManager
     * @param fetch the FetchConfiguration
     */
    public void load(OpenJPAStateManager sm, NdbOpenJPAStoreManager store, 
            JDBCFetchConfiguration fetch, NdbOpenJPAResult result) throws SQLException {
        for (NdbOpenJPADomainFieldHandlerImpl fieldHandler: fieldHandlers) {
            if (logger.isDebugEnabled()) logger.debug("loading field " + fieldHandler.getName()
                    + " for column " + fieldHandler.getColumnName() + " from result data.");
            fieldHandler.load(sm, store, fetch, result);
        }
    }

    /** Get StoreColumns for fields identified by the BitSet
     * 
     */
    public Set<com.mysql.clusterj.core.store.Column> getStoreColumns(BitSet fields) {
        // iterate the fields and save the columns in a Set
        if (fields == null) {
            return allStoreColumns;
        }
        Set<com.mysql.clusterj.core.store.Column> result = 
            new HashSet<com.mysql.clusterj.core.store.Column>();
        for (int i=fields.nextSetBit(0); i>=0; i=fields.nextSetBit(i+1)) {
            NdbOpenJPADomainFieldHandlerImpl fieldHandler = fieldHandlers[i];
            for (com.mysql.clusterj.core.store.Column column: fieldHandler.getStoreColumns()) {
                result.add(column);
            }
        }
        return result;
    }

    /** Get the DomainFieldHandler for the related field.
     * 
     * @param fm the related field mapping
     * @return the DomainFieldHandler
     */
    public NdbOpenJPADomainFieldHandlerImpl getDomainFieldHandler(FieldMapping fm) {
        for (NdbOpenJPADomainFieldHandlerImpl domainFieldHandler: fieldHandlers) {
            if (fm.equals(domainFieldHandler.getFieldMapping())) {
                return domainFieldHandler;
            }
        }
        throw new ClusterJFatalInternalException(
                local.message("ERR_Unknown_Field_Mapping",
                        this.getName(), fm.getName()));
    }

    /** Register an index from a field and return a special int[][] that
     * contains all indexHandlerImpls in which the
     * field participates. One index can be defined in the field annotation
     * and this index is identified here. This method is called by the
     * FieldHandler constructor after the Index annotation is processed
     * and the mapped column name is known.
     * TODO: Currently, only one index is supported per field.
     * @param fieldHandler the FieldHandler
     * @param indexName the index name
     * @param dictionary the dictionary used to validate index metadata
     * @return the array of array identifying the indexes into the IndexHandler
     * list and columns in the IndexHandler corresponding to the field
     */
    public int[][] createIndexHandler( NdbOpenJPADomainFieldHandlerImpl fieldHandler,
            Dictionary dictionary, String indexName) {
        IndexHandlerImpl indexHandler = new IndexHandlerImpl(this, dictionary, indexName, fieldHandler);
        int currentIndex = indexHandlerImpls.size();
        indexHandlerImpls.add(indexHandler);
        int[][] result = new int[1][2];
        result[0][0] = currentIndex;
        result[0][1] = 0;
        return result;
    }

    public com.mysql.clusterj.core.store.Table getTable() {
        return storeTable;
    }

    public Table getStoreTable() {
        return storeTable;
    }

    /** Create a partition key for any operation that knows the primary key. 
     * @param handler the handler that contains the values of the primary key
     */
    public PartitionKey createPartitionKey(ValueHandler handler) {
        // create the partition key based on the mapped table
        PartitionKey result = storeTable.createPartitionKey();
        // add partition key part value for each partition key field
        for (NdbOpenJPADomainFieldHandlerImpl fmd: partitionKeyFieldHandlers) {
            if (logger.isDetailEnabled()) logger.detail(
                    "Field number " + fmd.getFieldNumber()
                    + " column name " + fmd.getColumnName() + " field name " + fmd.getName());
            fmd.partitionKeySetPart(result, handler);
        }
        return result;
    }

    /** Register a dependency on another class. If the class is not already known,
     * add it to the list of dependencies.
     */
    public NdbOpenJPADomainTypeHandlerImpl<?> registerDependency(ClassMapping mapping) {
        NdbOpenJPADomainTypeHandlerImpl<?> domainTypeHandler = 
            domainTypeHandlerFactory.getDomainTypeHandler(mapping, dictionary);
        dependencies.add(domainTypeHandler);
        return domainTypeHandler;
    }

    /** Status of this class with regard to usability with clusterjpa.
     * 
     */
    private enum Status {
        IN_PROCESS,
        GOOD,
        BAD;
    }

    private Status supportStatus() {
        return status;
    }

    public void initializeRelations() {
        for (NdbOpenJPADomainFieldHandlerImpl fieldHandler: fieldHandlers) {
            fieldHandler.initializeRelations();
        }

        // iterate the dependencies and see if any are not supported
        boolean supported = status != Status.BAD;
        boolean workToDo = !supported;
        // iterate a copy of dependencies to avoid concurrent modification of dependencies
        List<NdbOpenJPADomainTypeHandlerImpl<?>> copyOfDependencies =
                new ArrayList<NdbOpenJPADomainTypeHandlerImpl<?>>(dependencies);
        for (NdbOpenJPADomainTypeHandlerImpl<?> dependent: copyOfDependencies) {
            // top level class will have recursive list of dependencies including itself
            this.dependencies.addAll(dependent.getDependencies());
            switch (dependent.supportStatus()) {
                case IN_PROCESS:
                    workToDo = true;
                    break;
                case GOOD:
                    break;
                case BAD:
                    setUnsupported();
                    supported = false;
                    workToDo = true;
                    String message = local.message("ERR_Bad_Dependency", dependent.typeName);
                    reasons.add(message);
                    if (logger.isDebugEnabled()) logger.debug(message);
            }
            String message = "Processing class " + typeName + " found dependency " + dependent.typeName
            + " support status is: " + dependent.supportStatus();
            if (logger.isDetailEnabled()) logger.detail(message);
        }
        if (workToDo) {
            if (!supported) {
                // found an unsupported class in the dependency list
                for (NdbOpenJPADomainTypeHandlerImpl<?> dependent: dependencies) {
                    dependent.setUnsupported();
                }
            } else {
                for (NdbOpenJPADomainTypeHandlerImpl<?> dependent: dependencies) {
                    dependent.setSupported();
                }
            }
        } else {
            this.setSupported();
        }
        if (logger.isDetailEnabled()) logger.detail("Processing class " + typeName + " has dependencies " + dependencies);
        if (logger.isDetailEnabled()) logger.detail("Processing class " + typeName + " has dependencies " + dependencies);
    }

    private Set<NdbOpenJPADomainTypeHandlerImpl<?>> getDependencies() {
        return dependencies;
    }

    private void setUnsupported() {
        if (status != Status.BAD) {
            if (logger.isDetailEnabled()) logger.detail("Class " + typeName + " marked as BAD.");
            status = Status.BAD;
        }
    }

    private void setUnsupported(String reason) {
        if (status != Status.BAD) {
            if (logger.isDetailEnabled()) logger.detail("Class " + typeName + " marked as BAD.");
            status = Status.BAD;
        }
        reasons.add(reason);
    }

    private void setSupported() {
        if (status != Status.GOOD) {
            if (logger.isDetailEnabled()) logger.detail("Class " + typeName + " marked as GOOD.");
            status = Status.GOOD;
        }
    }

    public boolean isSupportedType() {
        return Status.GOOD == status;
    }

    public String getReasons() {
        if (reasons.size() == 0) {
            return null;
        }
        StringBuilder result = new StringBuilder(
                local.message("MSG_Unsupported_Class", getName()));
        for (String reason:reasons) {
            result.append('\n');
            result.append(reason);
            result.append(';');
        }
        result.append('\n');
        return result.toString();
    }

    @Override
    public String toString() {
        return "NdbOpenJPADomainTypeHandlerImpl:" + typeName;
    }

    public String[] getFieldNames() {
        throw new ClusterJFatalInternalException(
                local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void operationSetValues(ValueHandler valueHandler, Operation op) {
        try {
            for (NdbOpenJPADomainFieldHandlerImpl fmd : fields) {
                fmd.operationSetValue(valueHandler, op);
            }
        } catch (Exception exception) {
            exception.printStackTrace();
            throw new RuntimeException("NdbOpenJPADomainTypeHandlerImpl.operationSetValues caught exception", exception);
        }
    }

    public void operationSetNonPKValues(ValueHandler valueHandler, Operation op) {
        try {
            for (NdbOpenJPADomainFieldHandlerImpl fmd : fields) {
                if (!fmd.isPrimaryKey()) {
                    fmd.operationSetValue(valueHandler, op);
                }
            }
        } catch (Exception exception) {
            exception.printStackTrace();
            throw new RuntimeException("NdbOpenJPADomainTypeHandlerImpl.operationSetNonPKValues caught exception", exception);
        }
    }

}
