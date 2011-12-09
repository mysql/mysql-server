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

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.ColumnType;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.query.CandidateIndexImpl;
import com.mysql.clusterj.core.query.InPredicateImpl;
import com.mysql.clusterj.core.query.PredicateImpl;
import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.store.Blob;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import java.lang.reflect.Proxy;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.HashSet;
import java.util.Set;

/**
 *
 */
public abstract class AbstractDomainFieldHandlerImpl implements DomainFieldHandler, ColumnMetadata {

    public AbstractDomainFieldHandlerImpl() {}

    /** Empty byte[] to set the initial value of the byte array before execute. */
    public static final byte[] emptyByteArray = new byte[0];
    static final I18NHelper local = I18NHelper.getInstance(AbstractDomainFieldHandlerImpl.class);
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(AbstractDomainFieldHandlerImpl.class);

    /** The domain type handler for this field */
    protected DomainTypeHandler<?> domainTypeHandler;

    /** true if the mapped column allows null values */
    protected boolean nullable;

    /** The default value for the column if the field is null */
    protected String columnDefaultValue = null;

    /** The column name if only one column for this field */
    protected String columnName = "";

    /** The Column metadata for the database column. */
    protected com.mysql.clusterj.core.store.Column storeColumn;

    /** The Charset name for the column. */
    protected String charsetName = null;

    /** The precision of the column in the database */
    protected int precision;

    /** The scale of the column in the database */
    protected int scale;

    /** The length of the column in the database */
    protected int maximumLength;

    /** true if the column is part of the partition key */
    protected boolean partitionKey;

    /** The Store Type for the column. */
    protected ColumnType storeColumnType = null;

    /** Column names in the case of a field mapped to multiple columns, e.g. foreign keys */
    protected String[] columnNames;

    /** The default value for this field */
    protected Object defaultValue;

    /** Error messages while constructing the field */
    protected StringBuffer errorMessages;

    /** The number of the field. This is the index into many arrays in the implementation,
     * in particular the array of fields in the domain type handler, and the array of
     * values in the InvocationHandler. */
    protected int fieldNumber;

    /** index names for this field */
    protected Set<String> indexNames = new HashSet<String>();
    /** The index handlers in which this field participates.
     * The first dimension indexes the index handlers in the DomainTypeHandler;
     * the second dimension indexes the IndexHandler fields array.
     * For example, a value of {{1,2}, {2,3}} means that this field is
     * at position 2 of the IndexHandler at position 1 and
     * at position 3 of the IndexHandler at position 2 in the
     * DomainTypeHandler.
     */
    protected int[][] indices = new int[0][0];

    /** The name of the field (property). */
    protected String name;

    /** The type of the field (property). */
    protected Class<?> type;

    /** If there is an ordered index on this field */
    protected boolean orderedIndex = false;

    /** If there is a unique index on this field */
    protected boolean uniqueIndex = false;

    /** If this is a primary key column */
    protected boolean primaryKey = false;

    /** The composite domain field handlers */
    public AbstractDomainFieldHandlerImpl[] compositeDomainFieldHandlers = null;

    /**
     * The type-specific anonymous class with methods
     * to set a value into an Operation or a managed Object.
     */
    protected ObjectOperationHandler objectOperationHandlerDelegate;

    /** Provide a reason for a field not being able to be persistent. The reason
     * is added to the existing list of reasons.
     * @param message the reason
     */
    protected void error(String message) {
        if (errorMessages == null) {
            errorMessages = new StringBuffer(local.message("ERR_Field_Not_Valid", domainTypeHandler.getName(), name));
        }
        errorMessages.append(message);
        errorMessages.append('\n');
    }

    public void filterCompareValue(Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
        if (value == null) {
            if (ScanFilter.BinaryCondition.COND_EQ.equals(condition)) {
                filter.isNull(storeColumn);
                return;
            } else {
                throw new ClusterJUserException(
                        local.message("ERR_Null_Values_Can_Only_Be_Filtered_Equal",
                                domainTypeHandler.getName(), name, condition));
            }
        }
        try {
            objectOperationHandlerDelegate.filterCompareValue(this, value, condition, filter);
        } catch (Exception ex) {
            throw new ClusterJDatastoreException(local.message("ERR_Filter_Value", name, columnName, objectOperationHandlerDelegate.handler(), value), ex);
        }
    }

    public String getColumnName() {
        return columnName;
    }

    public String[] getColumnNames() {
        return columnNames;
    }

    public int getFieldNumber() {
        return fieldNumber;
    }

    public Class<?> getType() {
        return type;
    }

    public String getTypeName() {
        return (type==null)?"unknown":printableName(type);
    }

    protected String printableName(Class<?> cls) {
        if (cls.isArray()) {
            return printableName(cls.getComponentType()) + "[] ";
        } else {
            return cls.getName();
        }
    }

    public String getName() {
        return name;
    }

    public boolean includedInIndex(String index) {
        return indexNames.contains(index);
    }

    public boolean isPrimitive() {
        return objectOperationHandlerDelegate.isPrimitive();
    }

    public boolean isPrimaryKey() {
        return primaryKey;
    }

    public com.mysql.clusterj.core.store.Column getStoreColumn() {
        return storeColumn;
    }

    public void markEqualBounds(CandidateIndexImpl[] candidateIndexImpls, PredicateImpl predicate) {
        for (int[] indexBounds : indices) {
            candidateIndexImpls[indexBounds[0]].markEqualBound(indexBounds[1], predicate);
        }
    }

    public void markInBounds(CandidateIndexImpl[] candidateIndexImpls, InPredicateImpl predicate) {
        for (int[] indexBounds : indices) {
            candidateIndexImpls[indexBounds[0]].markInBound(indexBounds[1], predicate);
        }
    }

    public void markLowerBounds(CandidateIndexImpl[] candidateIndexImpls, PredicateImpl predicate, boolean strict) {
        for (int[] indexBounds : indices) {
            candidateIndexImpls[indexBounds[0]].markLowerBound(indexBounds[1], predicate, strict);
        }
    }

    public void markUpperBounds(CandidateIndexImpl[] candidateIndexImpls, PredicateImpl predicate, boolean strict) {
        for (int[] indexBounds : indices) {
            candidateIndexImpls[indexBounds[0]].markUpperBound(indexBounds[1], predicate, strict);
        }
    }

    public Object getValue(QueryExecutionContext context, String index) {
        return objectOperationHandlerDelegate.getValue(context, index);
    }

    void objectSetDefaultValue(ValueHandler handler) {
        objectOperationHandlerDelegate.objectInitializeJavaDefaultValue(this, handler);
    }

    public void objectSetKeyValue(Object key, ValueHandler handler) {
        if (logger.isDetailEnabled()) {
            logger.detail("Setting value " + key + ".");
        }
        handler.setObject(fieldNumber, key);
    }

    public void objectSetValue(ResultData rs, ValueHandler handler) {
        try {
            objectOperationHandlerDelegate.objectSetValue(this, rs, handler);
        } catch (Exception ex) {
            throw new ClusterJDatastoreException(local.message("ERR_Value_Delegate", name, columnName, objectOperationHandlerDelegate.handler(), "objectSetValue"), ex);
        }
    }

    public void objectSetValueExceptIndex(ResultData rs, ValueHandler handler, String indexName) {
        try {
            if (!includedInIndex(indexName)) {
                objectOperationHandlerDelegate.objectSetValue(this, rs, handler);
            }
        } catch (Exception ex) {
            throw new ClusterJDatastoreException(local.message("ERR_Value_Delegate", name, columnName, objectOperationHandlerDelegate.handler(), "objectSetValueExcept"), ex);
        }
    }

    public void objectSetValueFor(Object value, Object row, String indexName) {
        if (includedInIndex(indexName)) {
            ValueHandler handler = (ValueHandler) Proxy.getInvocationHandler(row);
            handler.setObject(fieldNumber, value);
        }
    }

    public void operationEqual(Object value, Operation op) {
        try {
            objectOperationHandlerDelegate.operationEqual(this, value, op);
        } catch (Exception ex) {
            ex.printStackTrace();
            throw new ClusterJDatastoreException(local.message("ERR_Value_Delegate", name, columnName, objectOperationHandlerDelegate.handler(), "operationEqual"), ex);
        }
    }

    public void operationEqualForIndex(Object parameterValue, Operation op, String indexName) {
        throw new UnsupportedOperationException("Not yet implemented");
    }

    public void operationGetValue(Operation op) {
        if (logger.isDetailEnabled()) {
            logger.detail("Column " + columnName + ".");
        }
        try {
            objectOperationHandlerDelegate.operationGetValue(this, op);
        } catch (Exception ex) {
            throw new ClusterJDatastoreException(local.message("ERR_Value_Delegate", name, columnName, objectOperationHandlerDelegate.handler(), "operationGetValue"), ex);
        }
    }

    public void operationSetBounds(Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
        if (logger.isDetailEnabled()) {
            logger.detail("Column: " + columnName + " type: " + type + " value: " + value);
        }
        try {
            objectOperationHandlerDelegate.operationSetBounds(this, value, type, op);
        } catch (Exception ex) {
            throw new ClusterJDatastoreException(local.message("ERR_Value_Delegate", name, columnName, objectOperationHandlerDelegate.handler(), "operationSetBounds"), ex);
        }
    }

    public void operationSetModifiedValue(ValueHandler handler, Operation op) {
        if (handler.isModified(fieldNumber)) {
            // delegate to operationSetValue to get NullValue behavior
            operationSetValue(handler, op);
        }
    }

    public void operationSetValue(ValueHandler handler, Operation op) {
        if (logger.isDetailEnabled()) {
            logger.detail("Column: " + columnName + " field: " + name + " type: " + type + " delegate " + objectOperationHandlerDelegate.handler());
        }
        try {
            objectOperationHandlerDelegate.operationSetValue(this, handler, op);
        } catch (ClusterJDatastoreException ex) {
            throw new ClusterJDatastoreException(local.message("ERR_Value_Delegate", name, columnName, objectOperationHandlerDelegate.handler(), "operationSetValue"), ex);
        }
    }

    public void operationSetValue(Object value, Operation op) {
        if (logger.isDetailEnabled()) {
            logger.detail("Column: " + columnName + " field: " + name + " type: " + type + " delegate " + objectOperationHandlerDelegate.handler());
        }
        try {
            objectOperationHandlerDelegate.operationSetValue(this, value, op);
        } catch (ClusterJDatastoreException ex) {
            throw new ClusterJDatastoreException(local.message("ERR_Value_Delegate", name, columnName, objectOperationHandlerDelegate.handler(), "operationSetValue"), ex);
        }
    }

    protected void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
        try {
            objectOperationHandlerDelegate.operationSetValue(fmd, value, op);
        } catch (Exception ex) {
            throw new ClusterJDatastoreException(local.message("ERR_Value_Delegate", name, columnName, objectOperationHandlerDelegate.handler(), "operationSetValue"), ex);
        }
    }

    public void partitionKeySetPart(PartitionKey result, ValueHandler handler) {
        try {
            objectOperationHandlerDelegate.partitionKeySetPart(this, result, handler);
        } catch (NullPointerException npe) {
            throw new ClusterJUserException(
                    local.message("ERR_Key_Must_Not_Be_Null",
                            domainTypeHandler.getName(), getName()));
        }
    }

    protected static String formatBytes(int length, byte[] data) {
        int bytesToFormat = Math.min(length, data.length);
        StringBuffer buffer = new StringBuffer(":");
        for (int i = 0; i < bytesToFormat; ++i) {
            buffer.append("[");
            buffer.append(data[i]);
            buffer.append("]");
        }
        if (bytesToFormat < data.length) {
            buffer.append("...");
        }
        return buffer.toString();
    }

    protected static java.util.Date parse(String dateString) {
        try {
            return new SimpleDateFormat().parse(dateString);
        } catch (ParseException ex) {
            throw new ClusterJUserException(local.message("ERR_Parse_Exception", dateString));
        }
    }

    protected String printIndices() {
        StringBuffer buffer = new StringBuffer();
        buffer.append("indices[");
        buffer.append(indices.length);
        buffer.append("][]\n");
        for (int[] row : indices) {
            buffer.append(" row size ");
            buffer.append(row == null ? "null" : row.length);
            buffer.append(": ");
            buffer.append(row == null ? "" : row[0]);
            buffer.append(" ");
            buffer.append(row == null ? "" : row[1]);
            buffer.append("\n");
        }
        return buffer.toString();
    }

    protected void reportErrors() {
        if (errorMessages != null) {
            throw new ClusterJUserException(errorMessages.toString());
        }
    }

    @Override
    public String toString() {
        return name;
    }

    public void validateIndexType(String indexName, boolean hash) {
        if (objectOperationHandlerDelegate == null || !(objectOperationHandlerDelegate.isValidIndexType(this, hash))) {
            error (local.message("ERR_Invalid_Index_For_Type", 
                    domainTypeHandler.getName(), name, columnName, indexName, hash?"hash":"btree"));
        }
    }

    protected static interface ObjectOperationHandler {

        boolean isPrimitive();

        Object getValue(QueryExecutionContext context, String index);

        void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler);

        void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op);

        Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue);

        void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op);

        void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op);

        String handler();

        void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler);

        void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op);

        void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter);

        void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op);

        boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered);

        void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd, PartitionKey partitionKey, ValueHandler keyValueHandler);
    }

    protected static ObjectOperationHandler objectOperationHandlerByte = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return true;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setByte(fmd.fieldNumber, (byte) 0);
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Byte) (columnDefaultValue == null ? Byte.valueOf((byte)0) : Byte.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setByte(fmd.storeColumn, (Byte) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("Column " + fmd.columnName + " set to value " + handler.getByte(fmd.fieldNumber));
            }
            op.setByte(fmd.storeColumn, handler.getObjectByte(fmd.fieldNumber).byteValue());
        }

        public String handler() {
            return "setByte";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setByte(fmd.fieldNumber, rs.getByte(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundByte(fmd.storeColumn, type, ((Number)value).byteValue());
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpByte(condition, fmd.storeColumn, ((Number) value).byteValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setInt.setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalByte(fmd.storeColumn, ((Number) value).byteValue());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getByte(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerBoolean = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return true;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setBoolean(fmd.fieldNumber, false);
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Boolean) (columnDefaultValue == null ? Boolean.FALSE : Boolean.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setBoolean(fmd.storeColumn, (Boolean)value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            op.setBoolean(fmd.storeColumn, handler.getBoolean(fmd.fieldNumber));
        }

        public String handler() {
            return "setBoolean";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setBoolean(fmd.fieldNumber, rs.getBoolean(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            throw new ClusterJFatalInternalException(local.message("ERR_NotImplemented"));
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpBoolean(condition, fmd.storeColumn, ((Boolean) value).booleanValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalBoolean(fmd.storeColumn, ((Boolean)value).booleanValue());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return false;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getBoolean(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerObjectBoolean = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setBoolean(fmd.fieldNumber, false);
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Boolean)(columnDefaultValue == null ? Boolean.FALSE : Boolean.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setBoolean(fmd.storeColumn, (Boolean)value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
            op.setBoolean(fmd.storeColumn, handler.getBoolean(fmd.fieldNumber));
            }
        }

        public String handler() {
            return "setObjectBoolean";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setObjectBoolean(fmd.fieldNumber, rs.getObjectBoolean(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            throw new ClusterJFatalInternalException(local.message("ERR_NotImplemented"));
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpBoolean(condition, fmd.storeColumn, ((Boolean) value).booleanValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalBoolean(fmd.storeColumn, ((Boolean)value).booleanValue());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return false;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getBoolean(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerBytes = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            if (columnDefaultValue == null) {
                return new byte[]{};
            } else {
                throw new UnsupportedOperationException(local.message("ERR_Convert_String_To_Value", columnDefaultValue, "byte[]"));
            }
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (value == null) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setBytes(fmd.storeColumn, (byte[]) value);
            }
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            byte[] value = handler.getBytes(fmd.fieldNumber);
            if (value == null) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setBytes(fmd.storeColumn, value);
            }
        }

        public String handler() {
            return "setBytes";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setBytes(fmd.fieldNumber, rs.getBytes(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundBytes(fmd.storeColumn, type, (byte[]) value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpBytes(condition, fmd.storeColumn, (byte[]) value);
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setBytes.setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalBytes(fmd.storeColumn, (byte[]) value);
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getBytes(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerKeyBytes = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            if (columnDefaultValue == null) {
                return emptyByteArray;
            } else {
                throw new UnsupportedOperationException(local.message("ERR_Convert_String_To_Value", columnDefaultValue, "byte[]"));
            }
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalBytes(fmd.storeColumn, (byte[]) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            byte[] value = handler.getBytes(fmd.fieldNumber);
            op.equalBytes(fmd.storeColumn, value);
        }

        public String handler() {
            return "setKeyBytes";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setBytes(fmd.fieldNumber, rs.getBytes(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundBytes(fmd.storeColumn, type, (byte[]) value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpBytes(condition, fmd.storeColumn, (byte[]) value);
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setBytes.setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalBytes(fmd.storeColumn, (byte[]) value);
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            partitionKey.addBytesKey(fmd.storeColumn, keyValueHandler.getBytes(fmd.fieldNumber));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getBytes(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerBytesLob = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getBlob(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            if (columnDefaultValue == null) {
                return new byte[]{};
            } else {
                throw new UnsupportedOperationException(local.message("ERR_Convert_String_To_Value", columnDefaultValue, "byte[]"));
            }
        }

        public void operationSetValue(final AbstractDomainFieldHandlerImpl fmd, final Object value, final Operation op) {
            Blob blob = op.getBlobHandle(fmd.storeColumn);
            if (value == null) {
                blob.setNull();
            } else {
                // set an empty blob first, and later replace it with the real value
                blob.setValue(emptyByteArray);
                Runnable callback = new Runnable() {

                    public void run() {
                        Blob blob = op.getBlobHandle(fmd.storeColumn);
                        byte[] data = (byte[]) value;
                        int length = data.length;
                        if (logger.isDetailEnabled()) {
                            logger.detail("Value to operation set blob value for field " + fmd.name + " for column " + fmd.columnName + " wrote length " + length + formatBytes(16, data));
                        }
                        blob.writeData(data);
                    }
                };
                op.postExecuteCallback(callback);
            }
        }

        public void operationSetValue(final AbstractDomainFieldHandlerImpl fmd, final ValueHandler handler, final Operation op) {
            Blob blob = op.getBlobHandle(fmd.storeColumn);
            if (handler.isNull(fmd.fieldNumber)) {
                blob.setNull();
            } else {
                // set an empty blob first, and later replace it with the real value
                blob.setValue(emptyByteArray);
                Runnable callback = new Runnable() {

                    public void run() {
                        Blob blob = op.getBlobHandle(fmd.storeColumn);
                        byte[] data = handler.getBytes(fmd.fieldNumber);
                        int length = data.length;
                        if (logger.isDetailEnabled()) {
                            logger.detail("Value to operation set blob value for field " + fmd.name + " for column " + fmd.columnName + " wrote length " + length + formatBytes(16, data));
                        }
                        blob.writeData(data);
                    }
                };
                op.postExecuteCallback(callback);
            }
        }

        public String handler() {
            return "setBytesLob";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            Blob blob = rs.getBlob(fmd.storeColumn);
            int length = blob.getLength().intValue();
            byte[] data = new byte[length];
            blob.readData(data, length);
            if (logger.isDetailEnabled()) {
                logger.detail("ResultSet get blob value for field " + fmd.name + " for column " + fmd.columnName + " returned length " + length + formatBytes(16, data));
            }
            blob.close();
            handler.setBytes(fmd.fieldNumber, data);
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return false;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getBoolean(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerStringLob = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getBlob(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            if (columnDefaultValue == null) {
                return "";
            } else {
                return columnDefaultValue;
            }
        }

        public void operationSetValue(final AbstractDomainFieldHandlerImpl fmd, final Object value, final Operation op) {
            Blob blob = op.getBlobHandle(fmd.storeColumn);
            if (value == null) {
                blob.setNull();
            } else {
                // set an empty blob first, and later replace it with the real value
                blob.setValue(emptyByteArray);
                Runnable callback = new Runnable() {

                    public void run() {
                        Blob blob = op.getBlobHandle(fmd.storeColumn);
                        byte[] data = fmd.storeColumn.encode((String)value);
                        int length = data.length;
                        if (logger.isDetailEnabled()) {
                            logger.detail("Value to operation set text value for field " + fmd.name + " for column " + fmd.columnName + " wrote length " + length + formatBytes(16, data));
                        }
                        blob.writeData(data);
                    }
                };
                op.postExecuteCallback(callback);
            }
        }

        public void operationSetValue(final AbstractDomainFieldHandlerImpl fmd, final ValueHandler handler, final Operation op) {
            Blob blob = op.getBlobHandle(fmd.storeColumn);
            if (handler.isNull(fmd.fieldNumber)) {
                blob.setNull();
            } else {
                // set an empty blob first, and later replace it with the real value
                blob.setValue(emptyByteArray);
                Runnable callback = new Runnable() {

                    public void run() {
                        Blob blob = op.getBlobHandle(fmd.storeColumn);
                        byte[] data = fmd.storeColumn.encode(handler.getString(fmd.fieldNumber));
                        int length = data.length;
                        if (logger.isDetailEnabled()) {
                            logger.detail("Value to operation set text value for field " + fmd.name + " for column " + fmd.columnName + " wrote length " + length + formatBytes(16, data));
                        }
                        blob.writeData(data);
                    }
                };
                op.postExecuteCallback(callback);
            }
        }

        public String handler() {
            return "setStringLob";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            Blob blob = rs.getBlob(fmd.storeColumn);
            int length = blob.getLength().intValue();
            byte[] data = new byte[length];
            blob.readData(data, length);
            if (logger.isDetailEnabled()) {
                logger.detail("ResultSet get text value for field " + fmd.name + " for column " + fmd.columnName + " returned length " + length + formatBytes(16, data));
            }
            blob.close();
            handler.setString(fmd.fieldNumber, fmd.storeColumn.decode(data));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return false;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getString(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerDecimal = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (BigDecimal) (columnDefaultValue == null ? BigDecimal.ZERO : new BigDecimal(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setDecimal(fmd.storeColumn, (BigDecimal) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setDecimal(fmd.storeColumn, handler.getBigDecimal(fmd.fieldNumber));
            }
        }

        public String handler() {
            return "object BigDecimal";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setBigDecimal(fmd.fieldNumber, rs.getDecimal(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundDecimal(fmd.storeColumn, type, (BigDecimal) value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpDecimal(condition, fmd.storeColumn, (BigDecimal) value);
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setDecimal.setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalDecimal(fmd.storeColumn, (BigDecimal) value);
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getBigDecimal(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerBigInteger = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (BigInteger)(columnDefaultValue == null ? BigInteger.ZERO : new BigInteger(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setBigInteger(fmd.storeColumn, (BigInteger)value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setBigInteger(fmd.storeColumn, handler.getBigInteger(fmd.fieldNumber));
            }
        }

        public String handler() {
            return "object BigInteger";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setBigInteger(fmd.fieldNumber, rs.getBigInteger(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundBigInteger(fmd.storeColumn, type, (BigInteger)value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpBigInteger(condition, fmd.storeColumn, (BigInteger)value);
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setDecimal.setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalBigInteger(fmd.storeColumn, (BigInteger)value);
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getBigInteger(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerDouble = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return true;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setDouble(fmd.fieldNumber, 0.0D);
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Double) (columnDefaultValue == null ? Double.valueOf("0") : Double.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setDouble(fmd.storeColumn, (Double) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            op.setDouble(fmd.storeColumn, handler.getDouble(fmd.fieldNumber));
        }

        public String handler() {
            return "primitive double";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setDouble(fmd.fieldNumber, rs.getDouble(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundDouble(fmd.storeColumn, type, (Double) value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpDouble(condition, fmd.storeColumn, ((Double) value).doubleValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalDouble(fmd.storeColumn, (Double)value);
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getDouble(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerFloat = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return true;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setFloat(fmd.fieldNumber, 0.0F);
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Float) (columnDefaultValue == null ? Float.valueOf("0") : Float.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setFloat(fmd.storeColumn, (Float) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            op.setFloat(fmd.storeColumn, handler.getFloat(fmd.fieldNumber));
        }

        public String handler() {
            return "primitive float";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setFloat(fmd.fieldNumber, rs.getFloat(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundFloat(fmd.storeColumn, type, (Float) value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpFloat(condition, fmd.storeColumn, ((Float) value).floatValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalFloat(fmd.storeColumn, (Float)value);
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getFloat(index);
        }

    };

    protected abstract static class ObjectOperationHandlerInt implements ObjectOperationHandler {

        public boolean isPrimitive() {
            return true;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setInt(fmd.fieldNumber, 0);
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Integer) (columnDefaultValue == null ? Integer.valueOf(0) : Integer.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setInt(fmd.storeColumn, (Integer) value);
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            int value = rs.getInt(fmd.storeColumn);
            if (logger.isDetailEnabled()) {
                logger.detail("Field " + fmd.name + " from column " + fmd.columnName + " set to value " + value);
            }
            handler.setInt(fmd.fieldNumber, value);
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundInt(fmd.storeColumn, type, (Integer) value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpInt(condition, fmd.storeColumn, ((Integer) value).intValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalInt(fmd.storeColumn, ((Integer) value).intValue());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getInt(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerInt = new ObjectOperationHandlerInt() {

        public String handler() {
            return "primitive int";
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("Column " + fmd.columnName + " set to value " + handler.getInt(fmd.fieldNumber));
            }
            op.setInt(fmd.storeColumn, handler.getInt(fmd.fieldNumber));
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerKeyInt = new ObjectOperationHandlerInt() {

        public String handler() {
            return "primitive key int";
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("Key field " + fmd.name + " set equal to value " + handler.getInt(fmd.getFieldNumber()));
            }
            op.equalInt(fmd.storeColumn, handler.getInt(fmd.fieldNumber));
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            partitionKey.addIntKey(fmd.storeColumn, keyValueHandler.getInt(fmd.fieldNumber));
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerJavaSqlDate = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            if (columnDefaultValue == null) {
                return new Date(new java.util.Date().getTime());
            } else {
                // string is converted using SQL date handler
                return Date.valueOf(columnDefaultValue);
            }
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setLong(fmd.storeColumn, ((Date)value).getTime());
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setLong(fmd.storeColumn, (handler.getJavaSqlDate(fmd.fieldNumber)).getTime());
            }
        }

        public String handler() {
            return "object java.sql.Date";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            try {
                handler.setJavaSqlDate(fmd.fieldNumber, new Date(rs.getLong(fmd.storeColumn)));
            } catch (Exception ex) {
                throw new ClusterJDatastoreException(local.message("ERR_Set_Value", fmd.objectOperationHandlerDelegate.handler()), ex);
            }
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundLong(fmd.storeColumn, type, ((Date)value).getTime());
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpLong(condition, fmd.storeColumn, ((Date)value).getTime());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalLong(fmd.storeColumn, ((Date)value).getTime());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getJavaSqlDate(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerJavaSqlTime = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            if (columnDefaultValue == null) {
                return new Time(new java.util.Date().getTime());
            } else {
                // string is converted using SQL time handler
                return Time.valueOf(columnDefaultValue);
            }
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setLong(fmd.storeColumn, ((Time)value).getTime());
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setLong(fmd.storeColumn, (handler.getJavaSqlTime(fmd.fieldNumber)).getTime());
            }
        }

        public String handler() {
            return "object java.sql.Time";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setJavaSqlTime(fmd.fieldNumber, new Time(rs.getLong(fmd.storeColumn)));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundLong(fmd.storeColumn, type, ((Time)value).getTime());
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpLong(condition, fmd.storeColumn, ((Time)value).getTime());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalLong(fmd.storeColumn, ((Time)value).getTime());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getJavaSqlTime(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerJavaSqlTimestamp = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            if (columnDefaultValue == null) {
                return new Timestamp(new java.util.Date().getTime());
            } else {
                // string is converted using SQL timestamp handler
                return Timestamp.valueOf(columnDefaultValue);
            }
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setLong(fmd.storeColumn, ((Timestamp)value).getTime());
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setLong(fmd.storeColumn, (handler.getJavaSqlTimestamp(fmd.fieldNumber).getTime()));
            }
        }

        public String handler() {
            return "object java.sql.Timestamp";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setJavaSqlTimestamp(fmd.fieldNumber, new Timestamp(rs.getLong(fmd.storeColumn)));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundLong(fmd.storeColumn, type, ((Timestamp)value).getTime());
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpLong(condition, fmd.storeColumn, ((Timestamp)value).getTime());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalLong(fmd.storeColumn, ((Timestamp)value).getTime());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getJavaSqlTimestamp(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerJavaUtilDate = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            if (columnDefaultValue == null) {
                return new java.util.Date();
            } else {
                // any other string is converted using SQL timestamp handler
                return parse(columnDefaultValue);
            }
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setLong(fmd.storeColumn, ((java.util.Date)value).getTime());
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setLong(fmd.storeColumn, (handler.getJavaUtilDate(fmd.fieldNumber)).getTime());
            }
        }

        public String handler() {
            return "object java.util.Date";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setJavaUtilDate(fmd.fieldNumber, new java.util.Date(rs.getLong(fmd.storeColumn)));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundLong(fmd.storeColumn, type, ((java.util.Date)value).getTime());
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpLong(condition, fmd.storeColumn, ((java.util.Date)value).getTime());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalLong(fmd.storeColumn, ((java.util.Date)value).getTime());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getJavaUtilDate(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerKeyString = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (String) (columnDefaultValue == null ? "" : columnDefaultValue);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setString(fmd.storeColumn, (String) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            op.equalString(fmd.storeColumn, handler.getString(fmd.fieldNumber));
        }

        public String handler() {
            return "key String";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setString(fmd.fieldNumber, rs.getString(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundString(fmd.storeColumn, type, (String) value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpString(condition, fmd.storeColumn, (String)value);
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setString.setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalString(fmd.storeColumn, (String)value);
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            partitionKey.addStringKey(fmd.storeColumn, keyValueHandler.getString(fmd.fieldNumber));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getString(index);
        }

    };

    public abstract static class ObjectOperationHandlerLong implements ObjectOperationHandler {

        public boolean isPrimitive() {
            return true;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setLong(fmd.fieldNumber, 0L);
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Long) (columnDefaultValue == null ? Long.valueOf(0) : Long.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setLong(fmd.storeColumn, ((Number) value).longValue());
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setLong(fmd.fieldNumber, rs.getLong(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundLong(fmd.storeColumn, type, ((Number) value).longValue());
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpLong(condition, fmd.storeColumn, ((Number) value).longValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setLong.setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalLong(fmd.storeColumn, ((Number) value).longValue());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getLong(index);
        }

    }

    protected static ObjectOperationHandler objectOperationHandlerLong = new ObjectOperationHandlerLong() {

        public String handler() {
            return "primitive long";
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("Column " + fmd.columnName + " set to value " + handler.getLong(fmd.fieldNumber));
            }
            op.setLong(fmd.storeColumn, handler.getLong(fmd.fieldNumber));
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerKeyLong = new ObjectOperationHandlerLong() {

        public String handler() {
            return "key primitive long";
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("Column " + fmd.columnName + " set to value " + handler.getLong(fmd.fieldNumber));
            }
            op.equalLong(fmd.storeColumn, handler.getLong(fmd.fieldNumber));
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            partitionKey.addLongKey(fmd.storeColumn, keyValueHandler.getLong(fmd.fieldNumber));
        }

    };
    protected static ObjectOperationHandler objectOperationHandlerObjectByte = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Byte) (columnDefaultValue == null ? Byte.valueOf((byte)0) : Byte.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setByte(fmd.storeColumn, (Byte) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setByte(fmd.storeColumn, handler.getObjectByte(fmd.fieldNumber).byteValue());
            }
        }

        public String handler() {
            return "object Byte";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setObjectByte(fmd.fieldNumber, rs.getObjectByte(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundByte(fmd.storeColumn, type, ((Number)value).byteValue());
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpByte(condition, fmd.storeColumn, ((Number) value).byteValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setObjectByte.setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalByte(fmd.storeColumn, ((Number) value).byteValue());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getByte(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerObjectDouble = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Double) (columnDefaultValue == null ? Double.valueOf("0") : Double.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setDouble(fmd.storeColumn, (Double) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setDouble(fmd.storeColumn, handler.getObjectDouble(fmd.fieldNumber));
            }
        }

        public String handler() {
            return "object Double";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setObjectDouble(fmd.fieldNumber, rs.getObjectDouble(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundDouble(fmd.storeColumn, type, (Double) value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpDouble(condition, fmd.storeColumn, ((Double) value).doubleValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalDouble(fmd.storeColumn, (Double)value);
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getDouble(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerObjectFloat = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Float) (columnDefaultValue == null ? Float.valueOf("0") : Float.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setFloat(fmd.storeColumn, (Float) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setFloat(fmd.storeColumn, handler.getObjectFloat(fmd.fieldNumber));
            }
        }

        public String handler() {
            return "object Float";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setObjectFloat(fmd.fieldNumber, rs.getObjectFloat(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundFloat(fmd.storeColumn, type, (Float) value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpFloat(condition, fmd.storeColumn, ((Float) value).floatValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalFloat(fmd.storeColumn, (Float)value);
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getFloat(index);
        }

    };

    protected abstract static class ObjectOperationHandlerInteger implements ObjectOperationHandler {
        
        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Integer) (columnDefaultValue == null ? Integer.valueOf(0) : Integer.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setInt(fmd.storeColumn, (Integer) value);
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setObjectInt(fmd.fieldNumber, rs.getObjectInteger(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundInt(fmd.storeColumn, type, (Integer) value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpInt(condition, fmd.storeColumn, ((Integer) value).intValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setObjectInteger.setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalInt(fmd.storeColumn, ((Integer) value).intValue());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getInt(index);
        }

    }

    protected static ObjectOperationHandler objectOperationHandlerObjectInteger = new ObjectOperationHandlerInteger() {

        public String handler() {
            return "object Integer";
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setInt(fmd.storeColumn, handler.getObjectInt(fmd.fieldNumber));
            }
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerKeyObjectInteger = new ObjectOperationHandlerInteger() {

        public String handler() {
            return "key object Integer";
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.equalInt(fmd.storeColumn, handler.getObjectInt(fmd.fieldNumber));
            }
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            partitionKey.addIntKey(fmd.storeColumn, keyValueHandler.getObjectInt(fmd.fieldNumber));
        }

    };

    public abstract static class ObjectOperationHandlerObjectLong implements ObjectOperationHandler {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Long) (columnDefaultValue == null ? Long.valueOf(0) : Long.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setLong(fmd.storeColumn, ((Number) value).longValue());
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setObjectLong(fmd.fieldNumber, rs.getObjectLong(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundLong(fmd.storeColumn, type, ((Number) value).longValue());
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpLong(condition, fmd.storeColumn, ((Number) value).longValue());
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("setObjectLong.setEqual " + fmd.columnName + " to value " + value);
            }
            op.equalLong(fmd.storeColumn, ((Number) value).longValue());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getLong(index);
        }

    }

    protected static ObjectOperationHandler objectOperationHandlerObjectLong = new ObjectOperationHandlerObjectLong() {

        public String handler() {
            return "object Long";
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setLong(fmd.storeColumn, handler.getObjectLong(fmd.fieldNumber));
            }
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerKeyObjectLong = new ObjectOperationHandlerObjectLong() {

        public String handler() {
            return "key object Long";
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.equalLong(fmd.storeColumn, handler.getObjectLong(fmd.fieldNumber));
            }
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            partitionKey.addLongKey(fmd.storeColumn, keyValueHandler.getObjectLong(fmd.fieldNumber));
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerObjectShort = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Short) (columnDefaultValue == null ? Short.valueOf((short) 0) : Short.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setShort(fmd.storeColumn, (Short) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setShort(fmd.storeColumn, handler.getObjectShort(fmd.fieldNumber));
            }
        }

        public String handler() {
            return "object Short";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setObjectShort(fmd.fieldNumber, rs.getObjectShort(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            try {
                op.setBoundShort(fmd.storeColumn, type, ((Number) value).shortValue());
            } catch (ClassCastException ex) {
                throw new ClusterJUserException(local.message("ERR_Parameter_Type", "Number", value.getClass().getName()));
            }
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            try {
                filter.cmpShort(condition, fmd.storeColumn, ((Number) value).shortValue());
            } catch (ClassCastException ex) {
                throw new ClusterJUserException(local.message("ERR_Parameter_Type", "Number", value.getClass().getName()));
            }
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalShort(fmd.storeColumn, ((Number) value).shortValue());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getShort(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerShort = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return true;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setShort(fmd.fieldNumber, (short) 0);
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Short) (columnDefaultValue == null ? Short.valueOf((short) 0) : Short.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setShort(fmd.storeColumn, (Short) value);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (logger.isDetailEnabled()) {
                logger.detail("Column " + fmd.columnName + " set to value " + handler.getShort(fmd.fieldNumber));
            }
            op.setShort(fmd.storeColumn, handler.getShort(fmd.fieldNumber));
        }

        public String handler() {
            return "primitive short";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setShort(fmd.fieldNumber, rs.getShort(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            try {
                op.setBoundShort(fmd.storeColumn, type, ((Number) value).shortValue());
            } catch (ClassCastException ex) {
                throw new ClusterJUserException(local.message("ERR_Parameter_Type", "Number", value.getClass().getName()));
            }
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            try {
                filter.cmpShort(condition, fmd.storeColumn, ((Number) value).shortValue());
            } catch (ClassCastException ex) {
                throw new ClusterJUserException(local.message("ERR_Parameter_Type", "Number", value.getClass().getName()));
            }
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalShort(fmd.storeColumn, ((Number) value).shortValue());
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getShort(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerShortYear = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return true;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setShort(fmd.fieldNumber, (short) 1900);
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Short) (columnDefaultValue == null ? Short.valueOf((short) 1900) : Short.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setByte(fmd.storeColumn, (byte)((Short)value - 1900));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            op.setByte(fmd.storeColumn, (byte)(handler.getShort(fmd.fieldNumber) - 1900));
        }

        public String handler() {
            return "primitive short year";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setShort(fmd.fieldNumber, (short)(rs.getByte(fmd.storeColumn) + 1900));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            try {
                op.setBoundByte(fmd.storeColumn, type, (byte)(((Number) value).shortValue() - 1900));
            } catch (ClassCastException ex) {
                throw new ClusterJUserException(local.message("ERR_Parameter_Type", "Number", value.getClass().getName()));
            }
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            try {
                filter.cmpByte(condition, fmd.storeColumn, (byte)(((Number) value).shortValue() - 1900));
            } catch (ClassCastException ex) {
                throw new ClusterJUserException(local.message("ERR_Parameter_Type", "Number", value.getClass().getName()));
            }
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalByte(fmd.storeColumn, (byte)(((Number) value).shortValue() - 1900));
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getShort(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerObjectShortYear = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setShort(fmd.fieldNumber, (short) 1900);
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (Short) (columnDefaultValue == null ? Short.valueOf((short) 1900) : Short.valueOf(columnDefaultValue));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.setByte(fmd.storeColumn, (byte)((Short)value - 1900));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setByte(fmd.storeColumn, (byte)(handler.getShort(fmd.fieldNumber) - 1900));
            }
        }

        public String handler() {
            return "object short year";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            handler.setShort(fmd.fieldNumber, (short)(rs.getByte(fmd.storeColumn) + 1900));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            try {
                op.setBoundByte(fmd.storeColumn, type, (byte)(((Number) value).shortValue() - 1900));
            } catch (ClassCastException ex) {
                throw new ClusterJUserException(local.message("ERR_Parameter_Type", "Number", value.getClass().getName()));
            }
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            try {
                filter.cmpByte(condition, fmd.storeColumn, (byte)(((Number) value).shortValue() - 1900));
            } catch (ClassCastException ex) {
                throw new ClusterJUserException(local.message("ERR_Parameter_Type", "Number", value.getClass().getName()));
            }
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalByte(fmd.storeColumn, (byte)(((Number) value).shortValue() - 1900));
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getShort(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerString = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.storeColumn);
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return (String) (columnDefaultValue == null ? "" : columnDefaultValue);
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (value == null) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setString(fmd.storeColumn, (String)value);
            }
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            if (handler.isNull(fmd.fieldNumber)) {
                op.setNull(fmd.storeColumn);
            } else {
                op.setString(fmd.storeColumn, handler.getString(fmd.fieldNumber));
            }
        }

        public String handler() {
            return "object String";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            if (logger.isDetailEnabled()) {
                logger.detail("field " + fmd.name + " set to value " + rs.getString(fmd.storeColumn));
            }
            handler.setString(fmd.fieldNumber, rs.getString(fmd.storeColumn));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            op.setBoundString(fmd.storeColumn, type, (String)value);
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            filter.cmpString(condition, fmd.storeColumn, (String)value);
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            op.equalString(fmd.storeColumn, (String)value);
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return true;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getString(index);
        }

    };

    protected static ObjectOperationHandler objectOperationHandlerUnsupportedType = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            throw new ClusterJUserException(local.message("ERR_Unsupported_Field_Type", fmd.getTypeName(), fmd.getName()));
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            throw new ClusterJUserException(local.message("ERR_Unsupported_Field_Type", fmd.getTypeName(), fmd.getName()));
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            return null;
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            throw new ClusterJUserException(local.message("ERR_Unsupported_Field_Type", fmd.getTypeName(), fmd.getName()));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            throw new ClusterJUserException(local.message("ERR_Unsupported_Field_Type", fmd.getTypeName(), fmd.getName()));
        }

        public String handler() {
            return "unsupported Type";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            throw new ClusterJUserException(local.message("ERR_Unsupported_Field_Type", fmd.getTypeName(), fmd.getName()));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            throw new ClusterJUserException(local.message("ERR_Unsupported_Field_Type", fmd.getTypeName(), fmd.getName()));
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            throw new ClusterJUserException(local.message("ERR_Unsupported_Field_Type", fmd.getTypeName(), fmd.getName()));
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            throw new ClusterJUserException(local.message("ERR_Unsupported_Field_Type", fmd.getTypeName(), fmd.getName()));
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            return false;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd, PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJUserException(local.message("ERR_Unsupported_Field_Type", fmd.getTypeName(), fmd.getName()));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
       }

    };

    /** This operation handler is a no-op for getting and setting values that don't
     * have columns in the table that the field is mapped in, i.e. fields
     * that are mapped to foreign keys in other tables.
     *
     */
    protected static ObjectOperationHandler objectOperationHandlerVirtualType = new ObjectOperationHandler() {

        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            return;
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            return;
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            return;
        }

        public String handler() {
            return "Virtual Type (field with no columns)";
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            return;
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
       }

    };

    protected static abstract class ObjectOperationHandlerNotPersistent implements ObjectOperationHandler {

        public boolean isPrimitive() {
            return true;
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            // this value is never used
            return null;
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, IndexScanOperation.BoundType type, IndexScanOperation op) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, ScanFilter.BinaryCondition condition, ScanFilter filter) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            throw new ClusterJFatalInternalException(local.message("ERR_Implementation_Should_Not_Occur"));
       }

    }

    protected static ObjectOperationHandler objectOperationHandlerNotPersistentByte = new ObjectOperationHandlerNotPersistent() {

        public String handler() {
            return "not persistent primitive byte";
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setByte(fmd.fieldNumber, (byte) 0);
        }
    };
    protected static ObjectOperationHandler objectOperationHandlerNotPersistentDouble = new ObjectOperationHandlerNotPersistent() {

        public String handler() {
            return "not persistent primitive double";
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setDouble(fmd.fieldNumber, 0.0D);
        }
    };
    protected static ObjectOperationHandler objectOperationHandlerNotPersistentFloat = new ObjectOperationHandlerNotPersistent() {

        public String handler() {
            return "not persistent primitive float";
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setFloat(fmd.fieldNumber, 0.0F);
        }
    };
    protected static ObjectOperationHandler objectOperationHandlerNotPersistentInt = new ObjectOperationHandlerNotPersistent() {

        public String handler() {
            return "not persistent primitive int";
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setInt(fmd.fieldNumber, 0);
        }
    };
    protected static ObjectOperationHandler objectOperationHandlerNotPersistentLong = new ObjectOperationHandlerNotPersistent() {

        public String handler() {
            return "not persistent primitive long";
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setLong(fmd.fieldNumber, 0L);
        }
    };
    protected static ObjectOperationHandler objectOperationHandlerNotPersistentObject = new ObjectOperationHandlerNotPersistent() {

        public String handler() {
            return "not persistent Object";
        }

        @Override
        public boolean isPrimitive() {
            return false;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
        }
    };
    protected static ObjectOperationHandler objectOperationHandlerNotPersistentShort = new ObjectOperationHandlerNotPersistent() {

        public String handler() {
            return "not persistent primitive short";
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            handler.setShort(fmd.fieldNumber, (short) 0);
        }
    };

    /* These methods implement ColumnMetadata
     */

    protected void initializeColumnMetadata(com.mysql.clusterj.core.store.Column storeColumn) {
        this.columnName = storeColumn.getName();;
        this.storeColumnType = storeColumn.getType();
        this.charsetName = storeColumn.getCharsetName();
        this.primaryKey = storeColumn.isPrimaryKey();
        this.partitionKey = storeColumn.isPartitionKey();
        this.precision = storeColumn.getPrecision();
        this.scale = storeColumn.getScale();
        this.maximumLength = storeColumn.getLength();
        this.nullable = storeColumn.getNullable();
    }

    public boolean isPartitionKey() {
        return partitionKey;
    }

    public int maximumLength() {
        return maximumLength;
    }

    public String name() {
        return name;
    }

    public int number() {
        return fieldNumber;
    }

    public int precision() {
        return precision;
    }

    public int scale() {
        return scale;
    }

    public ColumnType columnType() {
        return this.storeColumnType;
    }

    public boolean nullable() {
        return nullable;
    }

    public Class<?> javaType() {
        return this.type;
    }

    public String charsetName() {
        return this.charsetName;
    }

}
