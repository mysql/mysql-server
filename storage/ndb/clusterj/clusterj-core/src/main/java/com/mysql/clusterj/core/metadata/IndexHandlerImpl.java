/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.core.query.CandidateIndexImpl;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.util.Arrays;

/** IndexHandlerImpl represents indices defined in the cluster.
 * One instance of this class represents either an ordered index or a unique
 * hash index. So a unique index that results in creating two indices will
 * be represented by two instances of IndexHandlerImpl.
 * The Dictionary access to unique indexes requires the suffix "$unique"
 * to be appended to the name, but the NdbTransaction access requires that
 * the suffix not be used. This class is responsible for mediating the
 * difference.
 * <p>
 * The simple case is one index => one field => one column. 
 * <p>
 * For ClusterJ and JPA, indexes can also support multiple columns, with each column 
 * mapped to one field. This pattern is used for compound primary keys. In this case,
 * there is one instance of IndexHandlerImpl for each index, and the columnNames
 * and fields have the same cardinality.
 * This is one index => multiple (one field => one column)
 * <p>
 * For JPA, indexes can also support one field mapped to multiple columns, which is
 * the pattern used for compound foreign keys to represent relationships.
 * In this case, there is a single instance of IndexHandlerImpl for each index. The 
 * columnNames lists the columns covered by the index, and there is one field. The
 * field manages an instance of the object id class for the relationship.
 * This is one index => one field => multiple columns.
 * <p>
 *
 */
public class IndexHandlerImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(IndexHandlerImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(IndexHandlerImpl.class);

    /** The unique suffix. */
    static final String UNIQUE_SUFFIX = "$unique";

    /** My class */
    protected String className;

    /** My table */
    protected String tableName;

    /** The indexName of the index. */
    private String indexName;

    /** The store Index of this index. */
    protected Index storeIndex;

    /** This is a unique index? */
    protected boolean unique = true;

    /** The fields (corresponding to the columnNames) in the index. */
    protected AbstractDomainFieldHandlerImpl[] fields;

    /** The columnNames in the index. */
    protected final String[] columnNames;

    /** This index is usable (all fields are mapped) */
    private boolean usable = true;

    /** The reason the index is not usable */
    private String reason = null;

    /** Construct an IndexHandlerImpl from an index name and column names.
     * This constructor is used when the column handlers are not yet known.
     * @param domainTypeHandler the domain type handler
     * @param dictionary the dictionary for validation
     * @param storeIndex the store index
     * @param columnNames the column names for the index
     */
    public IndexHandlerImpl(DomainTypeHandler<?> domainTypeHandler,
            Dictionary dictionary, Index storeIndex, String[] columnNames) {
        this.className = domainTypeHandler.getName();
        this.storeIndex = storeIndex;
        this.indexName = storeIndex.getName();
        this.tableName = domainTypeHandler.getTableName();
        this.columnNames = columnNames;
        int numberOfColumns = columnNames.length;
        // the fields are not yet known; will be filled later 
        this.fields = new AbstractDomainFieldHandlerImpl[numberOfColumns];
        this.unique = storeIndex.isUnique();
        if (logger.isDebugEnabled()) logger.debug(toString());
    }

    /** Construct an IndexHandlerImpl from an index declared on a field.
     * There may be more than one column in the index; the columns are taken from the
     * columns mapped by the field.
     * @param domainTypeHandler the domain type handler
     * @param dictionary the Dictionary
     * @param indexName the index name
     * @param fmd the DomainFieldHandlerImpl that corresponds to the column
     */
    public IndexHandlerImpl(DomainTypeHandler<?> domainTypeHandler,
            Dictionary dictionary, String indexName, AbstractDomainFieldHandlerImpl fmd) {
        this.className = domainTypeHandler.getName();
        this.indexName = indexName;
        this.tableName = domainTypeHandler.getTableName();
        this.storeIndex = getIndex(dictionary, tableName, indexName);
        this.unique = isUnique(storeIndex);
        this.columnNames = fmd.getColumnNames();
        this.fields = new AbstractDomainFieldHandlerImpl[]{fmd};
        if (logger.isDebugEnabled()) logger.debug(toString());
    }

    /** Create a CandidateIndexImpl from this IndexHandlerImpl.
     * The CandidateIndexImpl is used to decide on the strategy for a query.
     *
     */
    public CandidateIndexImpl toCandidateIndexImpl() {
        if (!usable) {
            return CandidateIndexImpl.getIndexForNullWhereClause();
        } else {
            return new CandidateIndexImpl(
                    className, storeIndex, unique, fields);
        }
    }

    @Override
    public String toString() {
        StringBuffer buffer = new StringBuffer();
        buffer.append("IndexHandler for class ");
        buffer.append(className);
        buffer.append(" index: ");
        buffer.append(indexName);
        buffer.append(" unique: ");
        buffer.append(unique);
        buffer.append(" columns: ");
        buffer.append(Arrays.toString(columnNames));
        return buffer.toString();
    }

    /** Set the DomainFieldHandlerImpl once it's known.
     *
     * @param i the index into the fields array
     * @param fmd the DomainFieldHandlerImpl for the corresponding column
     */
    public void setDomainFieldHandlerFor(int i, AbstractDomainFieldHandlerImpl fmd) {
        fields[i] = fmd;
        fmd.validateIndexType(indexName, unique);
    }

    /** Accessor for columnNames. */
    public String[] getColumnNames() {
        return columnNames;
    }

    /** Validate that all columnNames have fields.
     * 
     */
    public void assertAllColumnsHaveFields() {
        for (int i = 0; i < columnNames.length; ++i) {
            AbstractDomainFieldHandlerImpl fmd = fields[i];
            if (fmd == null || !(columnNames[i].equals(fmd.getColumnName()))) {
                usable = false;
                reason = local.message(
                        "ERR_Index_Mismatch", className, indexName, columnNames[i]);
            }
        }
    }

    protected boolean isUnique(Index storeIndex) {
        return storeIndex.isUnique();
    }

    public boolean isUsable() {
        return usable;
    }

    public String getReason() {
        return reason;
    }

    protected Index getIndex(Dictionary dictionary,
            String tableName, String indexName) {
        return dictionary.getIndex(indexName, tableName, indexName);
    }

    public String getIndexName() {
        return indexName;
    }

}
