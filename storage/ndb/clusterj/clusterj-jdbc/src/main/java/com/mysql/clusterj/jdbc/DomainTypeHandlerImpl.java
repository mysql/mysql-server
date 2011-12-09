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

package com.mysql.clusterj.jdbc;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;

import com.mysql.clusterj.core.metadata.AbstractDomainTypeHandlerImpl;
import com.mysql.clusterj.core.metadata.IndexHandlerImpl;

import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.spi.ValueHandler;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.lang.reflect.Proxy;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/** This instance manages the persistent representation of a jdbc statement or result set.
 * The class also provides the factory for DomainTypeHandlers via the static method
 * getDomainTypeHandler(String tableName, Dictionary dictionary).
 * @param T the class of the persistence-capable type
 */
public class DomainTypeHandlerImpl<T> extends AbstractDomainTypeHandlerImpl<T> {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(DomainTypeHandlerImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(DomainTypeHandlerImpl.class);

    /** The map of table name to DomainTypeHandler */
    static Map<String, DomainTypeHandlerImpl<?>> domainTypeHandlerMap = new HashMap<String, DomainTypeHandlerImpl<?>>();

    /** Find a domain type handler by table name or create a new one.
     * If the table does not exist in ndb, return null.
     * @param tableName the name of the table
     * @param dictionary the dictionary
     * @return the domain type handle for the table or null if the table does not exist in ndb
     */
    public static DomainTypeHandlerImpl<?> getDomainTypeHandler(String tableName, Dictionary dictionary) {
        DomainTypeHandlerImpl<?> result = null;
        synchronized (domainTypeHandlerMap) {
            result = domainTypeHandlerMap.get(tableName);
            if (result == null) {
                Table table = dictionary.getTable(tableName);
                if (table != null) {
                    result = new DomainTypeHandlerImpl(tableName, dictionary);
                    domainTypeHandlerMap.put(tableName, result);
                    if (logger.isDetailEnabled()) logger.detail("New DomainTypeHandler created for table " + tableName);
                } else {
                    if (logger.isDetailEnabled()) logger.detail("DomainTypeHandler for table " + tableName
                            + " not created; table is not an ndb table.");
                }
            } else {
                if (logger.isDetailEnabled()) logger.detail("Found DomainTypeHandler for table " + tableName);
            }
        }
        return result;
    }

    /** Construct a DomainTypeHandler for a table. Find primary key columns, partition key columns,
     * and indexes. Construct a DomainTypeHandler for each column in the table.
     */
    protected DomainTypeHandlerImpl(String tableName, Dictionary dictionary) {
        if (logger.isDebugEnabled()) logger.debug("New JDBC DomainTypeHandlerImpl for table " + tableName);
        this.tableName = tableName;
        this.table = dictionary.getTable(tableName);
        if (table == null) {
            throw new ClusterJUserException(local.message("ERR_Get_NdbTable", tableName, tableName));
        }
        String[] columnNames = table.getColumnNames();
        fieldNames = columnNames;
        if (logger.isDebugEnabled()) logger.debug("Found Table for " + tableName
                + " with columns " + Arrays.toString(columnNames));
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
            String[] indexColumnNames = index.getColumnNames();
            IndexHandlerImpl imd = new IndexHandlerImpl(this, dictionary, index, indexColumnNames);
            indexHandlerImpls.add(imd);
        }

        // Now iterate the columns in the table, creating a DomainFieldHandler for each column
        for (String columnName: columnNames) {
            Column column = table.getColumn(columnName);
            DomainFieldHandler domainFieldHandler = new DomainFieldHandlerImpl(this, table,
                    numberOfFields++, column);
            fieldNameToNumber.put(domainFieldHandler.getName(), domainFieldHandler.getFieldNumber());
            persistentFieldHandlers.add(domainFieldHandler);
            if (!column.isPrimaryKey()) {
                nonPKFieldHandlers.add(domainFieldHandler);
            }
        }
        // Check that all index columnNames have corresponding fields
        // indexes without fields will be unusable for query
        for (IndexHandlerImpl indexHandler:indexHandlerImpls) {
            indexHandler.assertAllColumnsHaveFields();
        }

    }

    /** Is this type supported? */
    public boolean isSupportedType() {
        // only supported types survive construction
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

    @Override
    public void objectResetModified(ValueHandler handler) {
        // ignore this for now
    }

    public void objectSetKeys(Object arg0, Object arg1) {
        throw new ClusterJFatalInternalException("Not implemented.");
    }
}
