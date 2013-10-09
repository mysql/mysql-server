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

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;

import com.mysql.clusterj.core.metadata.AbstractDomainFieldHandlerImpl;
import com.mysql.clusterj.core.query.QueryDomainTypeImpl;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.IndexScanOperation.BoundType;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.Table;
import com.mysql.clusterj.core.store.ScanFilter.BinaryCondition;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;
import com.mysql.clusterj.query.QueryDomainType;

import java.lang.reflect.Field;
import java.sql.SQLException;
import java.util.Arrays;
import java.util.BitSet;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import org.apache.openjpa.jdbc.kernel.JDBCFetchConfiguration;
import org.apache.openjpa.jdbc.meta.ClassMapping;
import org.apache.openjpa.jdbc.meta.FieldMapping;
import org.apache.openjpa.jdbc.meta.JavaSQLTypes;

import org.apache.openjpa.jdbc.meta.strats.RelationFieldStrategy;
import org.apache.openjpa.jdbc.meta.strats.RelationStrategies;
import org.apache.openjpa.jdbc.schema.Column;
import org.apache.openjpa.jdbc.schema.ForeignKey;
import org.apache.openjpa.jdbc.schema.Index;
import org.apache.openjpa.kernel.OpenJPAStateManager;
import org.apache.openjpa.meta.JavaTypes;
import org.apache.openjpa.util.IntId;
import org.apache.openjpa.util.LongId;
import org.apache.openjpa.util.ObjectId;
import org.apache.openjpa.util.OpenJPAId;
import org.apache.openjpa.util.StringId;

/**
 *
 */
public class NdbOpenJPADomainFieldHandlerImpl extends AbstractDomainFieldHandlerImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(NdbOpenJPADomainFieldHandlerImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(NdbOpenJPADomainFieldHandlerImpl.class);

    private static com.mysql.clusterj.core.store.Column[] emptyStoreColumns = new com.mysql.clusterj.core.store.Column[] {};
    /** The openjpa field mapping for this field */
    private FieldMapping fieldMapping;
    /** For single-column mappings, the mapped column */
    private Column column;
    /** The openjpa javaType of this field from org.apache.openjpa.meta.JavaTypes */
    private int javaType;
    /** The java.lang.reflect.Field in the oid class corresponding to the pk field */
    private Field oidField;
    /** The name of the class of this type, used for messages */
    private String javaTypeName;

    /** True if this field is a relation */
    private boolean isRelation = false;
    /** True if this field is mapped by the relation field in the other class */
    private boolean isMappedBy = false;
    /** True if the field is mapped to a single-valued field in the other class */
    private boolean isToOne = false;
    /** True if the field is embedded */
    private boolean isEmbedded;
    /** The openjpa class mapping of the related class */
    private ClassMapping relatedTypeMapping = null;
    /** The domain type handler of the related class */
    private NdbOpenJPADomainTypeHandlerImpl<?> relatedDomainTypeHandler = null;
    /** The openjpa field mapping for the related field */
    private FieldMapping relatedFieldMapping;
    /** The domain field handler for the related field */
    private FieldMapping mappedByMapping;
    /** The class of the related type */
    private Class<?> relatedType = null;
    /** The name of the class of the related type */
    private String relatedTypeName = null;

    /** These fields are to manage composite key relationships */
    /** The openjpa columns mapped by this relationship */
    private Column[] columns;

    /** The store Columns mapped by this relationship */
    private com.mysql.clusterj.core.store.Column[] storeColumns = emptyStoreColumns;

    /** The name of the related field */
    private String relatedFieldName;

    /** If this field is supported for clusterjpa */
    private boolean supported = true;

    /** The reason the field is not supported */
    private String reason = "";

    private RelatedFieldLoadManager relatedFieldLoadManager;

    public FieldMapping getFieldMapping() {
        return fieldMapping;
    }

    public NdbOpenJPADomainFieldHandlerImpl(Dictionary dictionary, NdbOpenJPADomainTypeHandlerImpl<?> domainTypeHandler,
            NdbOpenJPAConfigurationImpl domainTypeHandlerFactory, final FieldMapping fieldMapping) {

        String message = null;
        this.fieldMapping = fieldMapping;
        this.domainTypeHandler = domainTypeHandler;
        this.name = fieldMapping.getName();
        this.fieldNumber = fieldMapping.getIndex();
        this.primaryKey = fieldMapping.isPrimaryKey();
        this.columns = fieldMapping.getColumns();
        this.objectOperationHandlerDelegate = objectOperationHandlerUnsupportedType;
        this.mappedByMapping = fieldMapping.getMappedByMapping();
        this.isMappedBy = mappedByMapping != null;
        this.isToOne = fieldMapping.getStrategy() instanceof RelationFieldStrategy;
        if (isMappedBy) {
            relatedType = mappedByMapping.getDeclaringType();
            relatedFieldMapping = fieldMapping.getMappedByMapping();
        }
        // TODO are these valid for every field?
        this.relatedTypeMapping = fieldMapping.getDeclaredTypeMapping();
        if (relatedTypeMapping != null) {
            relatedType = relatedTypeMapping.getDescribedType();
        }
        if (relatedType != null) {
            relatedTypeName = relatedType.getName();
        }
        // TODO: the following might not be definitive to identify a relationship
        this.isRelation = fieldMapping.getStrategy().getClass().getName().contains("Relation");
        this.isEmbedded = fieldMapping.getStrategy().getClass().getName().contains("Embed");
        if (logger.isDetailEnabled()) logger.detail(
                "field: " + name + " strategy: " + fieldMapping.getStrategy().getClass().getName() + " with " + columns.length + " columns.");
        if ((!(isRelation | isEmbedded))) {
            if (columns.length == 1) {
                // each field is mapped to one column
                this.column = columns[0];
                this.columnName = column.getName();
                Table table = domainTypeHandler.getTable();
                if (table == null) {
                    message = local.message("ERR_No_Mapped_Table", domainTypeHandler.getName());
                    setUnsupported(message);
                    return;                    
                }
                this.storeColumn = table.getColumn(columnName);
                if (storeColumn == null) {
                    message = local.message("ERR_No_Column", name, table.getName(), columnName);
                    setUnsupported(message);
                    return;
                }
                this.storeColumns = new com.mysql.clusterj.core.store.Column[] {storeColumn};
                charsetName = storeColumn.getCharsetName();
                // set up the default object operation handler for the column type
                // TODO this might better use the "Class type;" field in superclass
                this.javaType = column.getJavaType();
                this.objectOperationHandlerDelegate = getObjectOperationHandler(javaType);
                if (objectOperationHandlerUnsupportedType.equals(objectOperationHandlerDelegate)) {
                    message = local.message("ERR_Unsupported_Meta_Type", javaType);
                    setUnsupported(message);
                    return;
                } else {
                    this.javaTypeName = NdbOpenJPAUtility.getJavaTypeName(javaType);
                    if (storeColumn.isPrimaryKey()) {
                        domainTypeHandler.registerPrimaryKeyColumn(this, storeColumn.getName());
                    }
                }
            } else if (columns.length > 1) {
                // error, no support
                StringBuffer buffer = new StringBuffer();
                String separator = "";
                for (Column errorColumn : columns) {
                    buffer.append(separator);
                    buffer.append(errorColumn.getName());
                    separator = ", ";
                }
                message = local.message("ERR_More_Than_One_Column_Mapped_To_A_Field",
                        domainTypeHandler.getName(), name, buffer);
                logger.info(message);
                setUnsupported(message);
                return;
            } else if (columns.length == 0) {
                message = local.message("ERR_No_Column_Mapped_To_A_Field",
                        domainTypeHandler.getName(), name, fieldMapping.getTable(), fieldMapping.getStrategy().getClass().getName());
                logger.info(message);
                setUnsupported(message);
                return;
            }
            if (this.primaryKey) {
                // each field is mapped to its own column
                // if using a user-defined openJPAId class, set up the value handler
                oidField = getFieldForOidClass(this, domainTypeHandler.getOidClass(), name);
                indexNames.add("PRIMARY");
                switch (javaType) {
                    case JavaTypes.INT:
                        this.objectOperationHandlerDelegate = objectOperationHandlerKeyInt;
                        break;
                    case JavaTypes.INT_OBJ: 
                        this.objectOperationHandlerDelegate = objectOperationHandlerKeyObjectInteger;
                        break;
                    case JavaTypes.LONG:
                        this.objectOperationHandlerDelegate = objectOperationHandlerKeyLong;
                        break;
                    case JavaTypes.LONG_OBJ: 
                        this.objectOperationHandlerDelegate = objectOperationHandlerKeyObjectLong;
                        break;
                   case JavaTypes.STRING: this.objectOperationHandlerDelegate =
                        objectOperationHandlerKeyString;
                        break;
                    default: 
                        message = local.message("ERR_Illegal_Primary_Key_Type",
                            domainTypeHandler.getName(), name, columnName, javaTypeName);
                        logger.info(message);
                        setUnsupported(message);
                }
            }
        } else if (isRelation) {
            // relationships might have zero, one, or more columns
            if (columns.length == 1) {
                this.column = columns[0];
                this.columnName = column.getName();
                this.columnNames = new String[] {columnName};
                Table table = domainTypeHandler.getTable();
                this.storeColumn = table.getColumn(columnName);
                if (storeColumn == null) {
                    message = local.message("ERR_No_Column", name, table.getName(), columnName);
                    setUnsupported(message);
                    return;
                }
                this.storeColumns = new com.mysql.clusterj.core.store.Column[] {storeColumn};
                // set up the default object operation handler for the column type
                this.javaType = column.getJavaType();
                this.javaTypeName = NdbOpenJPAUtility.getJavaTypeName(javaType);
                this.objectOperationHandlerDelegate = getObjectOperationHandlerRelationDelegate(javaType);
                if (objectOperationHandlerDelegate == null) {
                    // unsupported primary key type
                    return;
                }
            } else if (columns.length == 0) {
                if (isMappedBy) {
                    // this is the case of a OneToMany field mapped by columns in another table
                    this.objectOperationHandlerDelegate = objectOperationHandlerVirtualType;
                } else {
                    message = local.message("ERR_No_Columns_And_Not_Mapped_By",
                            this.domainTypeHandler.getName(), this.name);
                    logger.info(message);
                    setUnsupported(message);
                }
            } else {
                // multiple columns for related object
                if (isMappedBy) {
                    // this is the case of OneToOne field mapped by columns in another table
                    this.objectOperationHandlerDelegate = objectOperationHandlerVirtualType;
                } else {
                    // create an array of NdbOpenJPADomainFieldHandlerImpl
                    // one for each column in the foreign key
                    // each one needs to be able to extract the foreign key
                    // value from the openJPAId instance of the related instance
                    // using the oidField object
                    this.relatedTypeMapping = fieldMapping.getDeclaredTypeMapping();
                    this.relatedType = relatedTypeMapping.getDescribedType();
                    Class<?> oid = relatedTypeMapping.getObjectIdType();
                    if (logger.isDetailEnabled()) logger.detail(
                            "For class: " + domainTypeHandler.getName() +
                            " field: " + name + " related type is: " + relatedType.getName() +
                            " objectid type: " + oid.getName());
                    // create the domain field handlers for each column
                    this.compositeDomainFieldHandlers = new NdbOpenJPADomainFieldHandlerImpl[columns.length];
                    this.columnNames = new String[columns.length];
                    this.storeColumns = new com.mysql.clusterj.core.store.Column[columns.length];
                    for (int i = 0; i < columns.length; ++i) {
                        StringBuffer detailMessage = new StringBuffer();
                        Column localColumn = columns[i];
                        String localColumnName = localColumn.getName();
                        Table table = domainTypeHandler.getTable();
                        com.mysql.clusterj.core.store.Column localStoreColumn = table.getColumn(localColumnName);
                        if (localStoreColumn == null) {
                            message = local.message("ERR_No_Column", name, table.getName(), localColumnName);
                            logger.info(message);
                            setUnsupported(message);
                            return;
                        }
                        this.storeColumns[i] = localStoreColumn;
                        this.columnNames[i] = localColumnName;
                        ForeignKey foreignKey = fieldMapping.getForeignKey();
                        // get the primary key column corresponding to the local column
                        Column pkColumn = foreignKey.getPrimaryKeyColumn(localColumn);
                        if (logger.isDetailEnabled()) {
                            detailMessage.append(" column: " + localColumnName);
                            detailMessage.append(" fk-> " + foreignKey);
                            detailMessage.append(" pkColumn-> " + pkColumn);
                            logger.detail(detailMessage.toString());
                        }
                        NdbOpenJPADomainFieldHandlerImpl relatedFieldHandler = 
                            new NdbOpenJPADomainFieldHandlerImpl(this, localColumn, pkColumn);
                        if (relatedFieldHandler.isSupported()) {
                            this.compositeDomainFieldHandlers[i] = relatedFieldHandler;
                        } else {
                            message = relatedFieldHandler.getReason();
                            setUnsupported(message);
                            return;
                        }
                                
                    }
                    this.objectOperationHandlerDelegate =
                            objectOperationHandlerRelationCompositeField;
                }
            }
        } else {
            // embedded field
            message = local.message("ERR_Embedded_Fields_Not_Supported",
                    this.domainTypeHandler.getName(), this.name);
            logger.info(message);
            setUnsupported(message);
            return;
        }
        // now handle indexes, for supported field types
        Index index = fieldMapping.getJoinIndex();
        // TODO: where is this annotation used?
        if (index != null) {
            String indexName = index.getName();
            Column[] indexColumns = index.getColumns();
            if (logger.isDetailEnabled()) {
                StringBuilder buffer = new StringBuilder("Found index name ");
                buffer.append(indexName);
                buffer.append(" [");
                for (Column indexColumn : indexColumns) {
                    if (logger.isDetailEnabled()) {
                        buffer.append(indexColumn.getName());
                        buffer.append(" ");
                    }
                }
                buffer.append("]");
                logger.detail(buffer.toString());
            }
        }
        index = fieldMapping.getValueIndex();
        // Value indexes are used for ManyToOne and OneToOne relationship indexes on the mapped side
        if (index != null) {
            StringBuffer buffer = null;
            if (logger.isDetailEnabled())  buffer = new StringBuffer("Found index ");
            String indexName = index.getName();
            if (logger.isDetailEnabled())  buffer.append(indexName + " [ ");
            Column[] indexColumns = index.getColumns();
            for (Column indexColumn : indexColumns) {
                if (logger.isDetailEnabled()) buffer.append(indexColumn.getName() + " ");
            }
            if (logger.isDetailEnabled()) buffer.append("]");
            if (logger.isDetailEnabled()) logger.detail(buffer.toString());
            // create an index entry for clusterj queries
            // Create an index handler and register this instance with the domain type handler
            indices = domainTypeHandler.createIndexHandler(this, dictionary, indexName);
        }
        this.type = fieldMapping.getType();
        if (logger.isTraceEnabled()) {
            logger.trace(
                    " number: " + this.fieldNumber +
                    " name: " + this.name +
                    " column: " + this.columnName +
                    " Java type: " + javaType +
                    " strategy: " + toString(fieldMapping.getStrategy()) +
                    " ObjectOperationHandler: " + objectOperationHandlerDelegate.handler());
        }
    }

    /** Initialize relationship handling. There are three types of relationships
     * supported by clusterjpa:
     * <ul><li>direct ToOne relationship mapped by a foreign key on this side
     * </li><li>ToOne relationship mapped by a foreign key on the other side
     * </li><li>ToMany relationship mapped by a foreign key on the other side
     * </li></ul>
     * 
     */
    public void initializeRelations() {
        if (isRelation) {
            // set up related field load handler
            if (isMappedBy && isToOne) {
                // mapped by other side with one instance
                this.relatedDomainTypeHandler = ((NdbOpenJPADomainTypeHandlerImpl<?>)domainTypeHandler)
                        .registerDependency(relatedTypeMapping);
                this.relatedFieldName = relatedFieldMapping.getName();
                relatedFieldLoadManager = new RelatedFieldLoadManager() {
                    public void load(OpenJPAStateManager sm, NdbOpenJPAStoreManager store, 
                            JDBCFetchConfiguration fetch) throws SQLException {
                        SessionSPI session = store.getSession();
                        session.startAutoTransaction();
                        NdbOpenJPAResult queryResult = queryRelated(sm, store);
                        Object related = null;
                        try {
                            if (queryResult.next()) {
                                // instantiate the related object from the result of the query
                                related = store.load(relatedTypeMapping, fetch, (BitSet) null, queryResult);
                            }
                            if (logger.isDetailEnabled()) logger.detail("related object is: " + related);
                            // store the value of the related object in this field
                            sm.storeObjectField(fieldNumber, related);
                            session.endAutoTransaction();
                        } catch (Exception e) {
                            session.failAutoTransaction();
                        }
                    }
                };
                if (logger.isDetailEnabled()) logger.detail("Single-valued relationship field " + name
                        + " is mapped by " + relatedTypeName + " field " + relatedFieldName
                        + " with relatedDomainTypeHandler " + relatedDomainTypeHandler.getName());
            } else if (isMappedBy && !isToOne) {
                // mapped by other side with multiple instances
                this.relatedTypeMapping = mappedByMapping.getDeclaringMapping();
                this.relatedDomainTypeHandler = ((NdbOpenJPADomainTypeHandlerImpl<?>)domainTypeHandler)
                        .registerDependency(relatedTypeMapping);
                this.relatedFieldName = mappedByMapping.getName();
                relatedTypeName = relatedDomainTypeHandler.getName();
                if (logger.isDetailEnabled()) logger.detail("Multi-valued relationship field " + name
                        + " is mapped by " + relatedTypeName + " field " + relatedFieldName);
                    relatedFieldLoadManager = new RelatedFieldLoadManager() {
                        public void load(OpenJPAStateManager sm, NdbOpenJPAStoreManager store, 
                                JDBCFetchConfiguration fetch) throws SQLException {
                            SessionSPI session = store.getSession();
                            session.startAutoTransaction();
                            try {
                                NdbOpenJPAResult queryResult = queryRelated(sm, store);
                                while (queryResult.next()) {
                                    if (logger.isDetailEnabled()) logger.detail("loading related instance of type: " + relatedTypeMapping.getDescribedType().getName());
                                    store.load(relatedTypeMapping, fetch, (BitSet) null, queryResult);
                                }
                                fieldMapping.load(sm, store, fetch);
                                session.endAutoTransaction();
                            } catch (Exception e) {
                                session.failAutoTransaction();
                                throw new ClusterJException(local.message("ERR_Exception_While_Loading"), e);
                            }
                    }
                };
            } else {
                // this side contains foreign key to other side
                if (logger.isDetailEnabled()) logger.detail("NdbOpenJPADomainFieldHandlerImpl.initializeRelations for " 
                        + fieldMapping.getName() + " column " + (column==null?"null":column.getName())
                        + " relatedFieldName " + relatedFieldName
                        + " relatedFieldMapping " + relatedFieldMapping
                        + " relatedTypeMapping " + relatedTypeMapping);
                // record dependency to related type if not null
                if (relatedTypeMapping != null) {
                    this.relatedDomainTypeHandler = ((NdbOpenJPADomainTypeHandlerImpl<?>)domainTypeHandler)
                        .registerDependency(relatedTypeMapping);
                    relatedFieldLoadManager = new RelatedFieldLoadManager() {
                        public void load(OpenJPAStateManager sm, NdbOpenJPAStoreManager store, 
                                JDBCFetchConfiguration fetch) throws SQLException {
                            if (logger.isDetailEnabled()) logger.detail("Loading field " + name + "from stored key");
                            fieldMapping.load(sm, store, fetch);
                        }
                    };
                }
            }
        }
    }

    /** Get the object operation handler for the specific java field type.
     * @param javaType the java type from JavaTypes or JavaSQLTypes
     * @return the object operation handler
     */
    private ObjectOperationHandler getObjectOperationHandler(int javaType) {
        // the default is unsupported 
        ObjectOperationHandler result = objectOperationHandlerUnsupportedType;
        // if a known java type, the default is from the table of object operation handlers
        if (javaType < objectOperationHandlers.length) {
            result = objectOperationHandlers[javaType];
        }
        // handle exceptions, including all JavaSQLTypes and special handling for String
        switch (javaType) {
            case JavaSQLTypes.SQL_DATE:
                return objectOperationHandlerJavaSqlDate;
            case JavaSQLTypes.TIME:
                return objectOperationHandlerJavaSqlTime;
            case JavaSQLTypes.TIMESTAMP:
                return objectOperationHandlerJavaSqlTimestamp;
            case JavaSQLTypes.BYTES:
                switch(storeColumn.getType()) {
                    case Blob:
                    case Longvarbinary:
                        return objectOperationHandlerBytesLob;
                    case Binary:
                    case Varbinary:
                        return objectOperationHandlerBytes;
                    default:
                }
            case JavaTypes.STRING:
                switch(storeColumn.getType()) {
                    case Text:
                        return objectOperationHandlerStringLob;
                    case Char:
                    case Varchar:
                    case Longvarchar:
                        return objectOperationHandlerString;
                    default:
                }
            default:
        }
        return result;
    }

    /** This field handler is used with compound "foreign keys". Each column of
     * the compound "foreign key" has its own field handler. The parent
     * field handler has the relationship field but no columns.
     *
     * @param parent the field handler with the relationship field
     * @param localColumn the "foreign key" column in this table
     * @param pkColumn the primary key column in the other table
     */
    public NdbOpenJPADomainFieldHandlerImpl(NdbOpenJPADomainFieldHandlerImpl parent,
            Column localColumn, Column pkColumn) {
        String message = null;
        if (logger.isDetailEnabled()) logger.detail("NdbOpenJPADomainFieldHandlerImpl<init> for localColumn: " + localColumn + " pkColumn: " + pkColumn);
        this.column = localColumn;
        Table table = parent.domainTypeHandler.getStoreTable();
        this.storeColumn = table.getColumn(localColumn.getName());
        if (storeColumn == null) {
            message = local.message("ERR_No_Column", parent.getName(), table.getName(), columnName);
            setUnsupported(message);
            logger.info(message);
            return;
        }
        this.javaType = column.getJavaType();
        this.objectOperationHandlerDelegate = getObjectOperationHandlerRelationDelegate(javaType);
        if (objectOperationHandlerDelegate == null) {
            // unsupported primary key type
            return;
        }
        this.columnName = column.getName();
        this.fieldNumber = parent.fieldNumber;
        this.domainTypeHandler = parent.domainTypeHandler;
        this.relatedTypeMapping = parent.relatedTypeMapping;
        if (relatedTypeMapping != null) {
            relatedType = relatedTypeMapping.getDescribedType();
            if (relatedType != null) {
                relatedTypeName = relatedType.getName();
            }
        }
        // now find the field in the related class corresponding to this column
        FieldMapping[] relatedFieldMappings = relatedTypeMapping.getPrimaryKeyFieldMappings();
        for (FieldMapping rfm: relatedFieldMappings) {
            Column[] rcs = rfm.getColumns();
            if (logger.isDetailEnabled()) logger.detail("NdbOpenJPADomainFieldHandlerImpl<init> trying primary key column: " + rcs[0]);
            if (rcs.length == 1 && rcs[0].equals(pkColumn)) {
                // found the corresponding pk field
                String pkFieldName = rfm.getName();
                oidField = getFieldForOidClass(this, relatedTypeMapping.getObjectIdType(), pkFieldName);
            if (logger.isDetailEnabled()) logger.detail("NdbOpenJPADomainFieldHandlerImpl<init> found primary key column: " + rcs[0] + " for field: " + pkFieldName);
                break;
            }
        }
        if (oidField == null) {
            message = local.message("ERR_No_Oid_Field", pkColumn);
            setUnsupported(message);
            logger.info(message);
            return;
        }
        if (logger.isTraceEnabled()) {
            logger.trace(" Relation Field Handler for column: " + columnName +
                    " number: " + this.fieldNumber +
                    " name: " + this.name +
                    " column: " + this.columnName +
                    " Java type: " + javaType +
                    " ObjectOperationHandler: " + objectOperationHandlerDelegate.handler());
        }

    }

    interface RelatedFieldLoadManager {
        public void load(OpenJPAStateManager sm, NdbOpenJPAStoreManager store, 
                JDBCFetchConfiguration fetch) throws SQLException ;
    }

    /** Load the value of this field. This will be done here for relationship 
     * since basic fields are loaded when the instance is first initialized.
     *  
     * @param sm the openjpa state manager for the instance
     * @param store the store manager
     * @param fetch the fetch configuration, presently unused
     * @throws SQLException
     */
    public void load(OpenJPAStateManager sm, NdbOpenJPAStoreManager store, JDBCFetchConfiguration fetch)
            throws SQLException {
        if (isRelation) {
            relatedFieldLoadManager.load(sm, store, fetch);
        } else {
            throw new ClusterJFatalInternalException("load called for non-relationship field "
                    + this.getName() + " mapped to column " + this.columnName);
        }
    }

    /** Query the related type for instance(s) whose related field refers to this instance.
     * @param sm the state manager
     * @param store the store manager
     * @return the result of executing a query for the related type based on this instance's primary key
     */
    private NdbOpenJPAResult queryRelated(OpenJPAStateManager sm, NdbOpenJPAStoreManager store) {
        // get the Oid object for this sm
        OpenJPAId openJPAId = (OpenJPAId)sm.getObjectId();
        Object thisOid = openJPAId.getIdObject();
        QueryDomainType<?> queryDomainType = store.createQueryDomainType(relatedType);
        if (logger.isDetailEnabled()) logger.detail("created query for " + queryDomainType.getType().getName());
        // query for related type equals this pk oid value
        Predicate predicate = queryDomainType.get(relatedFieldName).equal(queryDomainType.param(relatedFieldName));
        queryDomainType.where(predicate);
        Map<String, Object> parameterList = new HashMap<String, Object>();
        parameterList.put(relatedFieldName, thisOid);
        if (logger.isDetailEnabled()) logger.detail(parameterList.toString());
        NdbOpenJPAResult queryResult = store.executeQuery(relatedDomainTypeHandler, queryDomainType, parameterList);
        // debug query result
        if (logger.isDetailEnabled()) {
            DomainTypeHandler<?> handler = queryResult.domainTypeHandler;
            Set<String> columnNames = queryResult.getColumnNames();
            StringBuffer buffer = new StringBuffer("Executed query for ");
            buffer.append(handler.getName());
            buffer.append(" returned columns: ");
            buffer.append(Arrays.toString(columnNames.toArray()));
            logger.detail(buffer.toString());
        }
        return queryResult;
    }

    public void load(OpenJPAStateManager sm, NdbOpenJPAStoreManager store, JDBCFetchConfiguration fetch, 
            NdbOpenJPAResult result) throws SQLException {
        fieldMapping.load(sm, store, fetch, result);
    }

    /** Add filters to the query and return the values to be used for the filter.
     * 
     * @param queryDomainType the QueryDomainType
     * @param thisOid the object id to be used to query foreign keys
     * @return the parameter map for the query with bound data values
     */
    public Map<String, Object> createParameterMap(QueryDomainType<?> queryDomainType, Object thisOid) {
        return ((ObjectOperationHandlerRelationField)objectOperationHandlerDelegate).createParameterMap(this, queryDomainType, thisOid);
    }

    public ObjectOperationHandler[] objectOperationHandlers =
            new ObjectOperationHandler[] {
        objectOperationHandlerBoolean,         /* 0: boolean */
        objectOperationHandlerByte,            /* 1: byte */
        objectOperationHandlerUnsupportedType, /* 2: char */
        objectOperationHandlerDouble,          /* 3: double */
        objectOperationHandlerFloat,           /* 4: float */
        objectOperationHandlerInt,             /* 5: int */
        objectOperationHandlerLong,            /* 6: long */
        objectOperationHandlerShort,           /* 7: short */
        objectOperationHandlerUnsupportedType, /* 8: Object */
        objectOperationHandlerString,          /* 9: String */
        objectOperationHandlerUnsupportedType, /* 10: Number */
        objectOperationHandlerUnsupportedType, /* 11: Array */
        objectOperationHandlerUnsupportedType, /* 12: Collection */
        objectOperationHandlerUnsupportedType, /* 13: Map */
        objectOperationHandlerJavaUtilDate,    /* 14: java.util.Date */
        objectOperationHandlerUnsupportedType, /* 15: PC */
        objectOperationHandlerObjectBoolean,   /* 16: Boolean */
        objectOperationHandlerObjectByte,      /* 17: Byte */
        objectOperationHandlerUnsupportedType, /* 18: Character */
        objectOperationHandlerObjectDouble,    /* 19: Double */
        objectOperationHandlerObjectFloat,     /* 20: Float */
        objectOperationHandlerObjectInteger,   /* 21: Integer */
        objectOperationHandlerObjectLong,      /* 22: Long */
        objectOperationHandlerObjectShort,     /* 23: Short */
        objectOperationHandlerDecimal,         /* 24: BigDecimal */
        objectOperationHandlerBigInteger,      /* 25: BigInteger */
        objectOperationHandlerUnsupportedType, /* 26: Locale */
        objectOperationHandlerUnsupportedType, /* 27: PC Untyped */
        objectOperationHandlerUnsupportedType, /* 28: Calendar */
        objectOperationHandlerUnsupportedType, /* 29: OID */
        objectOperationHandlerUnsupportedType, /* 30: InputStream */
        objectOperationHandlerUnsupportedType  /* 31: InputReader */        
    };

    public int compareTo(Object o) {
        return compareTo((NdbOpenJPADomainFieldHandlerImpl)o);
    }

    protected String toString(Object o) {
        return o.getClass().getSimpleName();
    }

    Column[] getColumns() {
        return fieldMapping.getColumns();
    }

    protected Object getKeyValue(Object keys) {
        Object key = keys;
        if (keys instanceof ObjectId) {
            key = ((ObjectId)keys).getId();
        }
        return getKeyValue(oidField, key);
    }

    protected static Object getKeyValue(Field field, Object keys) {
        try {
            Object result;
            String fieldName = "none";
            if (field != null) {
                result = field.get(keys);
                fieldName = field.getName();
            } else {
                result = keys;
            }
            if (logger.isDetailEnabled()) logger.detail("For field " + fieldName + " keys: " + keys + " value returned is " + result);
            return result;
        } catch (IllegalArgumentException ex) {
            String message = "IllegalArgumentException, field " + field.getDeclaringClass().getName() + ":" + field.getName() + " keys: " + keys;
            logger.error(message);
            throw new ClusterJUserException(message, ex);
        } catch (IllegalAccessException ex) {
            String message = "IllegalAccessException, field " + field.getDeclaringClass().getName() + ":" + field.getName() + " keys: " + keys;
            throw new ClusterJUserException(message, ex);
        }
    }

    public Field getOidField() {
        return oidField;
    }

    protected static Field getFieldForOidClass(
            NdbOpenJPADomainFieldHandlerImpl ndbOpenJPADomainFieldHandlerImpl,
            Class<?> oidClass, String fieldName) {
        String message = null;
        Field result = null;
        if (logger.isDetailEnabled()) logger.detail("Oid class: " + oidClass.getName());
        // the openJPAId class might be a simple type or a user-defined type
        if (OpenJPAId.class.isAssignableFrom(oidClass)) {
            return null;
        } else {
            try {
                // user-defined class; get Field to extract values at runtime
                result = oidClass.getField(fieldName);
                if (logger.isDetailEnabled()) logger.detail("OidField: " + result);
                return result;
            } catch (NoSuchFieldException ex) {
                message = local.message("ERR_No_Field_In_Oid_Class", oidClass.getName(), fieldName);
                logger.info(message);
                ndbOpenJPADomainFieldHandlerImpl.setUnsupported(message);
                return null;
            } catch (SecurityException ex) {
                message = local.message("ERR_Security_Violation_For_Oid_Class", oidClass.getName());
                logger.info(message);
                ndbOpenJPADomainFieldHandlerImpl.setUnsupported(message);
                return null;
            }
        }
    }

    protected ObjectOperationHandler getObjectOperationHandlerRelationDelegate(int javaType) {
        String message;
        switch (javaType) {
            case JavaTypes.INT:
            case JavaTypes.INT_OBJ:
                return objectOperationHandlerRelationIntField;

            case JavaTypes.LONG:
            case JavaTypes.LONG_OBJ:
                return objectOperationHandlerRelationLongField;

            case JavaTypes.STRING:
                return objectOperationHandlerRelationStringField;

            default:
                message = local.message("ERR_Illegal_Foreign_Key_Type",
                        domainTypeHandler.getName(), name, columnName, javaType);
                setUnsupported(message);
                return null;
        }
    }

    static abstract class ObjectOperationHandlerRelationField implements ObjectOperationHandler {

        public boolean isPrimitive() {
            return false;
        }

        /** Add the filter to the query and create the parameter map with the bound value of the oid.
         * 
         * @param domainFieldHandler the domain field handler
         * @param queryDomainObject the query domain object
         * @param oid the object id value to bind to the filter
         * @return the map with the bound parameter
         */
        public Map<String, Object> createParameterMap(NdbOpenJPADomainFieldHandlerImpl domainFieldHandler, QueryDomainType<?> queryDomainObject, Object oid) {
            String name = domainFieldHandler.name;
            PredicateOperand parameter = queryDomainObject.get(name);
            Predicate predicate = parameter.equal(parameter);
            queryDomainObject.where(predicate);
            // construct a map of parameter binding to the value in oid
            Map<String, Object> result = new HashMap<String, Object>();
            Object value = domainFieldHandler.getKeyValue(oid);
            result.put(name, value);
            if (logger.isDetailEnabled()) logger.detail("Map.Entry key: " + name + ", value: " + value);
            return result;
        }

        public void objectInitializeJavaDefaultValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            op.getValue(fmd.getStoreColumn());
        }

        public Object getDefaultValueFor(AbstractDomainFieldHandlerImpl fmd, String columnDefaultValue) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void objectSetValue(AbstractDomainFieldHandlerImpl fmd, ResultData rs, ValueHandler handler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, BoundType type, IndexScanOperation op) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, BinaryCondition condition, ScanFilter filter) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public void operationEqual(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Implementation_Should_Not_Occur"));
        }

        public boolean isValidIndexType(AbstractDomainFieldHandlerImpl fmd, boolean hashNotOrdered) {
            // relationships can use either hash or btree indexes
            return true;
        }

        protected int getInt(Object objectId) {
            // get an int value from an objectId
            if (objectId instanceof IntId) {
                return ((IntId)objectId).getId();
            } else if (objectId instanceof OpenJPAId) {
                OpenJPAId openJPAId = (OpenJPAId)objectId;
                Object id = openJPAId.getIdObject();
                if (id instanceof Integer) {
                    return ((Integer)id).intValue();
                }
                throw new UnsupportedOperationException(
                        local.message("ERR_Unsupported_Object_Id_Type", "int key", "OpenJPAId"));
            } else {
                String message = (objectId == null)?"<null>":objectId.getClass().getName();
                throw new UnsupportedOperationException(
                        local.message("ERR_Unsupported_Object_Id_Type", "int key", message));
            }
        }

        protected long getLong(Object objectId) {
            // get a long value from an objectId
            if (objectId instanceof LongId) {
                return ((LongId)objectId).getId();
            } else if (objectId instanceof OpenJPAId) {
                OpenJPAId openJPAId = (OpenJPAId)objectId;
                Object id = openJPAId.getIdObject();
                if (id instanceof Long) {
                    return ((Long)id).longValue();
                }
                throw new UnsupportedOperationException(
                        local.message("ERR_Unsupported_Object_Id_Type", "long key", "OpenJPAId"));
            } else {
                String message = (objectId == null)?"<null>":objectId.getClass().getName();
                throw new UnsupportedOperationException(
                        local.message("ERR_Unsupported_Object_Id_Type", "long key", message));
            }
        }

        protected String getString(Object objectId) {
            // get a String value from an objectId
            if (objectId instanceof StringId) {
                return ((StringId)objectId).getId();
            } else if (objectId instanceof OpenJPAId) {
                OpenJPAId openJPAId = (OpenJPAId)objectId;
                Object id = openJPAId.getIdObject();
                if (id instanceof String) {
                    return (String)id;
                }
                throw new UnsupportedOperationException(
                        local.message("ERR_Unsupported_Object_Id_Type", "String key", "OpenJPAId"));
            } else {
                String message = (objectId == null)?"<null>":objectId.getClass().getName();
                throw new UnsupportedOperationException(
                        local.message("ERR_Unsupported_Object_Id_Type", "String key", message));
            }
        }

        protected OpenJPAStateManager getRelatedStateManager(ValueHandler handler, AbstractDomainFieldHandlerImpl fmd) {
            // get related object
            OpenJPAStateManager sm = ((NdbOpenJPAValueHandler) handler).getStateManager();
            NdbOpenJPAStoreManager store = ((NdbOpenJPAValueHandler) handler).getStoreManager();
            OpenJPAStateManager rel = RelationStrategies.getStateManager(sm.fetchObjectField(fmd.getFieldNumber()), store.getContext());
            return rel;
        }

        public void partitionKeySetPart(AbstractDomainFieldHandlerImpl fmd,
                PartitionKey partitionKey, ValueHandler keyValueHandler) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Operation_Not_Supported","partitionKeySetPart", "non-key fields"));
        }

        public Object getValue(QueryExecutionContext context, String index) {
            return context.getObject(index);
        }
    };

    static ObjectOperationHandler objectOperationHandlerRelationIntField =
            new ObjectOperationHandlerRelationField() {

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            OpenJPAStateManager rel = getRelatedStateManager(handler, fmd);
            // get openJPAId from related object
            if (rel == null) {
                if (logger.isDetailEnabled()) logger.detail("Related object is null");
                op.setNull(fmd.getStoreColumn());
            } else {
                Object objid = rel.getObjectId();
                if (objid == null) {
                    // TODO: doesn't seem right
                    op.setNull(fmd.getStoreColumn());
                    if (logger.isDetailEnabled()) logger.detail("Related object class: " + rel.getMetaData().getTypeAlias() + " object id: " + objid);
                } else {
                    int oid = getInt(objid);
                    if (logger.isDetailEnabled()) logger.detail("Related object class: " + rel.getMetaData().getTypeAlias() + " key: " + oid);
                    op.setInt(fmd.getStoreColumn(), oid);
                }
            }
        }

        public String handler() {
            return "Object ToOne Int key.";
        }

        @Override
        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (value == null) {
                op.setNull(fmd.getStoreColumn());
            } else {
                op.setInt(fmd.getStoreColumn(),(Integer) value);
            }
        }

        @Override
        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object oid, BinaryCondition condition, ScanFilter filter) {
            Field field = ((NdbOpenJPADomainFieldHandlerImpl)fmd).getOidField();
            Object value = getKeyValue(field, oid);
            if (logger.isDetailEnabled()) logger.detail("For column: " + fmd.getColumnName() + " oid: " + oid + " value: " + value);
            filter.cmpInt(condition, fmd.getStoreColumn(), ((Integer) value).intValue());
        }

        /** Set bounds for an index operation.
         * 
         * @param fmd the domain field handler
         * @param value the value to set
         * @param type the bound type (i.e. EQ, NE, LE, LT, GE, GT)
         * @param op the index operation
         */
        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, BoundType type, IndexScanOperation op) {
            op.setBoundInt(fmd.getStoreColumn(), type, (Integer)value);
        }

    };

    static ObjectOperationHandler objectOperationHandlerRelationLongField =
            new ObjectOperationHandlerRelationField() {

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            OpenJPAStateManager rel = getRelatedStateManager(handler, fmd);
            // get openJPAId from related object
            if (rel == null) {
                if (logger.isDetailEnabled()) logger.detail("Related object is null");
                op.setNull(fmd.getStoreColumn());
            } else {
                long oid = getLong(rel.getObjectId());
                if (logger.isDetailEnabled()) logger.detail("Related object class: " + rel.getMetaData().getTypeAlias() + " key: " + oid);
                op.setLong(fmd.getStoreColumn(), oid);
            }
        }

        public String handler() {
            return "Object ToOne Long key.";
        }

        @Override
        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (value == null) {
                op.setNull(fmd.getStoreColumn());
            } else {
                op.setLong(fmd.getStoreColumn(),(Long) value);
            }
        }

        @Override
        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object oid, BinaryCondition condition, ScanFilter filter) {
            Field field = ((NdbOpenJPADomainFieldHandlerImpl)fmd).getOidField();
            Object value = getKeyValue(field, oid);
            if (logger.isDetailEnabled()) logger.detail("For column: " + fmd.getColumnName() + " oid: " + oid + " value: " + value);
            filter.cmpLong(condition, fmd.getStoreColumn(), ((Long) value).longValue());
        }

        /** Set bounds for an index operation.
         * 
         * @param fmd the domain field handler
         * @param value the value to set
         * @param type the bound type (i.e. EQ, NE, LE, LT, GE, GT)
         * @param op the index operation
         */
        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, BoundType type, IndexScanOperation op) {
            op.setBoundLong(fmd.getStoreColumn(), type, (Long)value);
        }

    };

    static ObjectOperationHandler objectOperationHandlerRelationStringField =
            new ObjectOperationHandlerRelationField() {

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            OpenJPAStateManager rel = getRelatedStateManager(handler, fmd);
            // get openJPAId from related object
            if (rel == null) {
                if (logger.isDetailEnabled()) logger.detail("Related object is null");
                op.setNull(fmd.getStoreColumn());
            } else {
                String oid = getString(rel.getObjectId());
                if (logger.isDetailEnabled()) logger.detail("Related object class: " + rel.getMetaData().getTypeAlias() + " key: " + oid);
                op.setString(fmd.getStoreColumn(), oid);
            }
        }

        public String handler() {
            return "Object ToOne String key.";
        }

        @Override
        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            if (value == null) {
                op.setNull(fmd.getStoreColumn());
            } else {
                op.setString(fmd.getStoreColumn(),(String) value);
            }
        }

        @Override
        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object oid, BinaryCondition condition, ScanFilter filter) {
            Field field = ((NdbOpenJPADomainFieldHandlerImpl)fmd).getOidField();
            Object value = getKeyValue(field, oid);
            if (logger.isDetailEnabled()) logger.detail("For column: " + fmd.getColumnName() + " oid: " + oid + " filter.cmpString: " + value);
            filter.cmpString(condition, fmd.getStoreColumn(), (String) value);
        }

        /** Set bounds for an index operation.
         * 
         * @param fmd the domain field handler
         * @param value the value to set
         * @param type the bound type (i.e. EQ, NE, LE, LT, GE, GT)
         * @param op the index operation
         */
        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object value, BoundType type, IndexScanOperation op) {
            op.setBoundString(fmd.getStoreColumn(), type, (String)value);
        }

    };

    static ObjectOperationHandlerRelationField objectOperationHandlerRelationCompositeField = new ObjectOperationHandlerRelationField() {

        public String handler() {
            return "Composite key.";
        }

        @Override
        public void operationGetValue(AbstractDomainFieldHandlerImpl fmd, Operation op) {
            for (AbstractDomainFieldHandlerImpl localHandler: fmd.compositeDomainFieldHandlers) {
                localHandler.operationGetValue(op);
            }
        }

        @Override
        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, Object value, Operation op) {
            throw new ClusterJFatalInternalException(
                    local.message("ERR_Implementation_Should_Not_Occur"));
        }

        /** Set the filter for this composite key relationship field. This only works for equal conditions.
         * Set the filter to AND, and for each field in the composite key, extract the value
         * from the oid and add it to the filter.
         */
        @Override
        public void filterCompareValue(AbstractDomainFieldHandlerImpl fmd, Object value, BinaryCondition condition, ScanFilter filter) {

            if (!BinaryCondition.COND_EQ.equals(condition)) {
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Illegal_Filter_Condition", condition));
            }
            filter.begin();
            for (AbstractDomainFieldHandlerImpl localHandler : fmd.compositeDomainFieldHandlers) {
                if (value != null) {
                    // extract the value from the oid and add the filter condition
                    localHandler.filterCompareValue(value, condition, filter);
                } else {
                    // set null for each local column
                    localHandler.filterCompareValue((Object)null, condition, filter);
                }
            }
            filter.end();
        }

        public void operationSetValue(AbstractDomainFieldHandlerImpl fmd, ValueHandler handler, Operation op) {
            OpenJPAStateManager rel = getRelatedStateManager(handler, fmd);
            OpenJPAId openJPAId = null;
            Object oid = null;
            if (rel == null) {
                if (logger.isDetailEnabled()) logger.detail("Related object is null");
            } else {
                if (logger.isDetailEnabled()) logger.detail("Related object class: " + rel.getMetaData().getTypeAlias() + " key: " + openJPAId);
                openJPAId = (OpenJPAId) rel.getObjectId();
                oid = openJPAId.getIdObject();
            }
            for (AbstractDomainFieldHandlerImpl localHandler : fmd.compositeDomainFieldHandlers) {
                Object value = null;
                if (rel != null) {
                    // get the value from the related object
                    Field field = ((NdbOpenJPADomainFieldHandlerImpl)localHandler).getOidField();
                    value = getKeyValue(field, oid);
                    localHandler.operationSetValue(value, op);
                } else {
                    // set null for each local column
                    localHandler.operationSetValue((Object)null, op);
                }
            }
        }

        @Override
        public Map<String, Object> createParameterMap(NdbOpenJPADomainFieldHandlerImpl domainFieldHandler, 
                QueryDomainType<?> queryDomainObject, Object oid) {
            Map<String, Object> result = new HashMap<String, Object>();
            Predicate predicate = null;
            for (AbstractDomainFieldHandlerImpl localHandler: domainFieldHandler.compositeDomainFieldHandlers) {
                String name = localHandler.getColumnName();
                PredicateOperand parameter = queryDomainObject.param(name);
                PredicateOperand field = queryDomainObject.get(name);
                if (predicate == null) {
                    predicate = field.equal(parameter);
                } else {
                    predicate.and(field.equal(parameter));
                }
                // construct a map of parameter binding to the value in oid
                Object value = domainFieldHandler.getKeyValue(oid);
                result.put(name, value);
                if (logger.isDetailEnabled()) logger.detail("Map.Entry key: " + name + ", value: " + value);
            }
            queryDomainObject.where(predicate);
            return result;
        }

        /** Set bounds for an index operation. Delegate this to each domain field handler which
         * will extract the column data from the object id.
         * 
         * @param fmd the domain field handler
         * @param oid the value to set
         * @param type the bound type (i.e. EQ, LE, LT, GE, GT)
         * @param op the index operation
         */
        public void operationSetBounds(AbstractDomainFieldHandlerImpl fmd, Object oid, BoundType type, IndexScanOperation op) {
            for (AbstractDomainFieldHandlerImpl localHandler : fmd.compositeDomainFieldHandlers) {
                Field field = ((NdbOpenJPADomainFieldHandlerImpl)localHandler).getOidField();
                Object columnData = getKeyValue(field, oid);
                localHandler.operationSetBounds(columnData, type, op);
            }
        }

    };

    public com.mysql.clusterj.core.store.Column[] getStoreColumns() {
        return storeColumns;
    }

    public boolean isSupported() {
        return supported;
    }

    public boolean isRelation() {
        return isRelation;
    }

    public String getReason() {
        return reason;
    }

    private void setUnsupported(String reason) {
        this.supported = false;
        this.reason = reason;
    }

}
