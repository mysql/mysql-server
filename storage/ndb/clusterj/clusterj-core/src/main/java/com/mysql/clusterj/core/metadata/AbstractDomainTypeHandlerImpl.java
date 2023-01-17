/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

import java.util.ArrayList;
import java.util.BitSet;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.CacheManager;
import com.mysql.clusterj.core.query.CandidateIndexImpl;
import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Db;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.IndexOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.Table;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/** Abstract class implementing DomainTypeHandler. This class implements common
 * behavior to manage persistent representations of tables, including field
 * handlers for persistent field values. Subclasses will implement behavior
 * specific to the actual representations of persistence.
 */
public abstract class AbstractDomainTypeHandlerImpl<T> implements DomainTypeHandler<T> {

    /** My message translator */
    protected static final I18NHelper local = I18NHelper.getInstance(AbstractDomainTypeHandlerImpl.class);

    /** My logger */
    protected static final Logger logger = LoggerFactoryService.getFactory().getInstance(AbstractDomainTypeHandlerImpl.class);

    /** The name of the class. */
    protected String name;

    /** The table for the class. */
    protected String tableName;

    /** The table key for the class, which might include projection information. */
    protected String tableKey;

    /** The NDB table for the class. */
    protected Table table;

    /** The number of id fields for the class. */
    protected int numberOfIdFields;

    /** The field numbers of the id fields. */
    protected int[] idFieldNumbers;

    /** The id field(s) for the class, mapped to primary key columns */
    protected DomainFieldHandler[] idFieldHandlers;

    /** The field(s) for the class */
    protected DomainFieldHandler[] fieldHandlers;

    /** The PrimaryKey column names. */
    protected String[] primaryKeyColumnNames;

    /** The number of partition key columns */
    protected int numberOfPartitionKeyColumns = 0;

    /** The partition key fields */
    protected DomainFieldHandler[] partitionKeyFieldHandlers;

    /** The names of the partition key columns */
    protected String[] partitionKeyColumnNames;

    /** The number of fields. Dynamically created as fields are added. */
    protected int numberOfFields = 0;

    /** Persistent fields. */
    protected List<DomainFieldHandler> persistentFieldHandlers = new ArrayList<DomainFieldHandler>();

    /** Non PK fields. */
    protected List<DomainFieldHandler> nonPKFieldHandlers = new ArrayList<DomainFieldHandler>();

    /** Primitive fields. */
    protected List<DomainFieldHandler> primitiveFieldHandlers = new ArrayList<DomainFieldHandler>();

    /** Map of field names to field numbers. */
    protected Map<String, Integer> fieldNameToNumber = new HashMap<String, Integer>();

    /** Field names */
    protected String[] fieldNames;

    /** All index handlers defined for the mapped class. The position in this
     * array is significant. Each DomainFieldHandlerImpl contains the index into
     * this array and the index into the fields array within the
     * IndexHandlerImpl.
     */
    protected List<IndexHandlerImpl> indexHandlerImpls = new ArrayList<IndexHandlerImpl>();

    /** Set of index names to check for duplicates. */
    protected Set<String> indexNames = new HashSet<String>();

    /** Errors reported during construction; see getUnsupported(), setUnsupported(String) */
    private StringBuilder reasons = null;

    /** Register a primary key column field. This is used to associate
     * primary key and partition key column names with field handlers.
     * This method is called by the DomainFieldHandlerImpl constructor
     * after the mapped column name is known. It is only called by fields
     * that are mapped to primary key columns.
     * @param fmd the field handler instance calling us
     * @param columnName the name of the column
     */
    public void registerPrimaryKeyColumn(DomainFieldHandler fmd, String columnName) {
        // find the primary key column that matches the primary key column
        for (int i = 0; i < primaryKeyColumnNames.length; ++i) {
            if (primaryKeyColumnNames[i].equals(columnName)) {
                idFieldHandlers[i] = fmd;
                idFieldNumbers[i] = fmd.getFieldNumber();
                if (logger.isDetailEnabled()) logger.detail("registerPrimaryKeyColumn registered primary key " +
                        columnName);
            }
        }
        // find the partition key column that matches the primary key column
        for (int j = 0; j < partitionKeyColumnNames.length; ++j) {
            if (partitionKeyColumnNames[j].equals(columnName)) {
                partitionKeyFieldHandlers[j] = fmd;
                if (logger.isDetailEnabled()) logger.detail("registerPrimaryKeyColumn registered partition key " +
                        columnName);
            }
        }
        return;
    }

    /** Create and register an index from a field and return a special int[][] that
     * contains all indexHandlerImpls in which the
     * field participates. The int[][] is used by the query optimizer to determine
     * which if any index can be used. This method is called by the
     * DomainFieldHandlerImpl constructor after the mapped column name is known.
     * @see AbstractDomainFieldHandlerImpl#indices
     * @param fmd the FieldHandler
     * @param columnName the column name mapped to the field
     * @return the array of array identifying the indexes into the IndexHandler
     * list and columns in the IndexHandler corresponding to the field
     */
    public int[][] registerIndices(AbstractDomainFieldHandlerImpl fmd, String columnName) {
        // Find all the indexes that this field belongs to, by iterating
        // the list of indexes and comparing column names.
        List<int[]> result =new ArrayList<int[]>();
        for (int i = 0; i < indexHandlerImpls.size(); ++i) {
            IndexHandlerImpl indexHandler = indexHandlerImpls.get(i);
            String[] columns = indexHandler.getColumnNames();
            for (int j = 0; j < columns.length; ++j) {
                if (fmd.getColumnName().equals(columns[j])) {
                    if (logger.isDetailEnabled()) logger.detail("Found field " + fmd.getName()
                            + " column " + fmd.getColumnName() + " matching " + indexHandler.getIndexName());
                    indexHandler.setDomainFieldHandlerFor(j, fmd);
                    result.add(new int[]{i,j});
                }
            }
        }
    
        if (logger.isDebugEnabled()) logger.debug("Found " + result.size() + " indexes for " + columnName);
        return result.toArray(new int[result.size()][]);
    }

    /** Return the list of index names corresponding to the array of indexes.
     * This method is called by the DomainFieldHandlerImpl constructor after the
     * registerIndices method is called.
     * @param indexArray the result of registerIndices
     * @return all index names for the corresponding indexes
     */
    public Set<String> getIndexNames(int[][] indexArray) {
        Set<String> result = new HashSet<String>();
        for (int[] index: indexArray) {
            result.add(indexHandlerImpls.get(index[0]).getIndexName());
        }
        return result;
    }

    /** Extract the column names from store Columns.
     * 
     * @param indexName the index name (for error messages)
     * @param columns the store Column instances
     * @return an array of column names
     */
    protected String[] getColumnNames(String indexName, Column[] columns) {
        Set<String> columnNames = new HashSet<String>();
        for (Column column : columns) {
            String columnName = column.getName();
            if (columnNames.contains(columnName)) {
                // error: the column name is duplicated
                throw new ClusterJUserException(
                        local.message("ERR_Duplicate_Column",
                        name, indexName, columnName));
            }
            columnNames.add(columnName);
        }
        return columnNames.toArray(new String[columnNames.size()]);
    }

    /** Create a list of candidate indexes to evaluate query terms and
     * decide what type of operation to use. The result must correspond
     * one to one with the indexHandlerImpls.
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

    public String getTableName() {
        return tableName;
    }

    public int getNumberOfFields() {
        return numberOfFields;
    }

    public DomainFieldHandler[] getIdFieldHandlers() {
        return idFieldHandlers;
    }

    public DomainFieldHandler[] getFieldHandlers() {
        return fieldHandlers;
    }

    public DomainFieldHandler getFieldHandler(String fieldName) {
        for (DomainFieldHandler fmd: persistentFieldHandlers) {
            if (fmd.getName().equals(fieldName)) {
                return fmd;
            }
        }
        throw new ClusterJUserException(
                local.message("ERR_Not_A_Member", fieldName, name));
    }

    public int getFieldNumber(String fieldName) {
        Integer fieldNumber = fieldNameToNumber.get(fieldName);
        if (fieldNumber == null) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_No_Field_Number", fieldName, name));
        }
        return fieldNumber.intValue();
    }

    public void operationSetNonPKValues(ValueHandler handler, Operation op) {
        for (DomainFieldHandler fmd: nonPKFieldHandlers) {
            if (handler.isModified(fmd.getFieldNumber())) {
                fmd.operationSetValue(handler, op);
            }
        }
    }

    public void operationSetValues(ValueHandler handler, Operation op) {
        for (DomainFieldHandler fmd: persistentFieldHandlers) {
            if (logger.isDetailEnabled()) logger.detail("operationSetValues field: " + fmd.getName());
            fmd.operationSetValue(handler, op);
        }
    }

    public void operationSetModifiedNonPKValues(ValueHandler handler, Operation op) {
        for (DomainFieldHandler fmd: nonPKFieldHandlers) {
            fmd.operationSetModifiedValue(handler, op);
        }
            }

    public void operationSetModifiedValues(ValueHandler handler, Operation op) {
        for (DomainFieldHandler fmd: persistentFieldHandlers) {
            fmd.operationSetModifiedValue(handler, op);
        }
    }

    public void operationSetKeys(ValueHandler handler, Operation op) {
        for (DomainFieldHandler fmd: idFieldHandlers) {
            fmd.operationSetValue(handler, op);
        }        
    }

    public void operationGetValues(Operation op) {
        for (DomainFieldHandler fmd: persistentFieldHandlers) {
            fmd.operationGetValue(op);
        }
    }

    public void operationGetValues(Operation op, BitSet fields) {
        if (fields == null) {
            operationGetValues(op);
        } else {
            int i = 0;
            for (DomainFieldHandler fmd: persistentFieldHandlers) {
                if (fields.get(i++)) {
                    fmd.operationGetValue(op);
                }
            }
        }
    }

    public void operationGetValuesExcept(IndexOperation op, String index) {
        for (DomainFieldHandler fmd: persistentFieldHandlers) {
            if (!fmd.includedInIndex(index)) {
                if (logger.isDetailEnabled()) logger.detail("operationGetValuesExcept index: " + index);
                fmd.operationGetValue(op);
            }
        }
    }

    public void objectSetValues(ResultData rs, ValueHandler handler) {
        for (DomainFieldHandler fmd: persistentFieldHandlers) {
            fmd.objectSetValue(rs, handler);
        }
    }

    public void objectSetValuesExcept(ResultData rs, ValueHandler handler, String indexName) {
        for (DomainFieldHandler fmd: persistentFieldHandlers) {
            fmd.objectSetValueExceptIndex(rs, handler, indexName);
        }
    }

    protected Table getTable(Dictionary dictionary) {
        Table result;
        try {
            result = dictionary.getTable(tableName);
        } catch (Exception ex) {
            throw new ClusterJException(
                    local.message("ERR_Get_NdbTable", name, tableName), ex);
        }
        return result;
    }

    public int[] getKeyFieldNumbers() {
        return idFieldNumbers;
    }

    public Table getStoreTable() {
        return table;
    }

    /** Create a partition key for a find by primary key. 
     * @param handler the handler that contains the values of the primary key
     */
    public PartitionKey createPartitionKey(ValueHandler handler) {
        // create the partition key based on the mapped table
        PartitionKey result = table.createPartitionKey();
        // add partition key part value for each partition key field
        for (DomainFieldHandler fmd: partitionKeyFieldHandlers) {
            if (logger.isDetailEnabled()) logger.detail(
                        "Field number " + fmd.getFieldNumber()
                        + " column name " + fmd.getName() + " field name " + fmd.getName());
            fmd.partitionKeySetPart(result, handler);
        }
        return result;
    }

    public String getName() {
        return name;
    }

    public Set<String> getColumnNames(BitSet fields) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public Set<com.mysql.clusterj.core.store.Column> getStoreColumns(BitSet fields) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public ValueHandler createKeyValueHandler(Object keys, Db db) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public T getInstance(ValueHandler handler) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public Class<?> getOidClass() {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public Class<?>[] getProxyInterfaces() {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public ValueHandler getValueHandler(Object instance) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public boolean isSupportedType() {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public T newInstance(Db db) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public T newInstance(ResultData resultData, Db db) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void objectMarkModified(ValueHandler handler, String fieldName) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void objectResetModified(ValueHandler handler) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    public void objectSetCacheManager(CacheManager cm, Object instance) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

    protected String removeUniqueSuffix(String indexName) {
        int beginIndex = indexName.lastIndexOf("$unique");
        if (beginIndex < 0) {
            // there's no $unique suffix
            return indexName;
        }
        String result = indexName.substring(0, beginIndex);
        return result;
    }

    public String[] getFieldNames() {
        return fieldNames;
    }

    public void setUnsupported(String reason) {
        if (reasons == null) {
            reasons = new StringBuilder();
        }
        reasons.append(reason);
    }

    public String getUnsupported() {
        return reasons == null?null:reasons.toString();
    }

    public T newInstance(ValueHandler valueHandler) {
        throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
    }

}
