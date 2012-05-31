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

package com.mysql.clusterj.core.query;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Query;

import com.mysql.clusterj.Query.Ordering;
import com.mysql.clusterj.core.query.PredicateImpl.ScanType;
import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.spi.ValueHandlerBatching;

import com.mysql.clusterj.core.store.Blob;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.IndexOperation;
import com.mysql.clusterj.core.store.IndexScanOperation;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.ScanOperation;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;
import com.mysql.clusterj.query.QueryDefinition;
import com.mysql.clusterj.query.QueryDomainType;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class QueryDomainTypeImpl<T> implements QueryDomainType<T> {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(QueryDomainTypeImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(QueryDomainTypeImpl.class);

    /** My class. */
    protected Class<T> cls;

    /** My DomainTypeHandler. */
    protected DomainTypeHandler<T> domainTypeHandler;

    /** My where clause. */
    protected PredicateImpl where;

    /** My parameters. These encapsulate the parameter names not the values. */
    protected Map<String, ParameterImpl> parameters =
            new HashMap<String, ParameterImpl>();

    /** My properties. These encapsulate the property names not the values. */
    protected Map<String, PropertyImpl> properties =
            new HashMap<String, PropertyImpl>();

    /** My index for this query */
    CandidateIndexImpl index = null;

    /** My ordering fields for this query */
    String[] orderingFields = null;

    /** My ordering for this query */
    Ordering ordering = null;

    public QueryDomainTypeImpl(DomainTypeHandler<T> domainTypeHandler, Class<T> cls) {
        this.cls = cls;
        this.domainTypeHandler = domainTypeHandler;
    }

    public QueryDomainTypeImpl(DomainTypeHandler<T> domainTypeHandler) {
        this.domainTypeHandler = domainTypeHandler;
    }

    public PredicateOperand get(String propertyName) {
        // if called multiple times for the same property,
        // return the same PropertyImpl instance
        PropertyImpl property = properties.get(propertyName);
        if (property != null) {
            return property;
        } else {
            DomainFieldHandler fmd = domainTypeHandler.getFieldHandler(propertyName);
            property = new PropertyImpl(this, fmd);
            properties.put(propertyName, property);
            return property;
        }
    }

    /** Set the where clause. Mark parameters used by this query.
     * @param predicate the predicate
     * @return the query definition (this)
     */
    public QueryDefinition<T> where(Predicate predicate) {
        if (predicate == null) {
            throw new ClusterJUserException(
                    local.message("ERR_Query_Where_Must_Not_Be_Null"));
        }
        if (!(predicate instanceof PredicateImpl)) {
            throw new UnsupportedOperationException(
                    local.message("ERR_NotImplemented"));
        }
        // if a previous where clause, unmark the parameters
        if (where != null) {
            where.unmarkParameters();
            where = null;
        }
        this.where = (PredicateImpl)predicate;
        where.markParameters();
        // statically analyze the where clause, looking for:
        // primary keys all specified with equal
        // unique keys all specified with equal
        // btree index keys partly specified with ranges
        // none of the above
        where.prepare();
        return this;
    }

    public PredicateOperand param(String parameterName) {
        final ParameterImpl parameter = parameters.get(parameterName);
        if (parameter != null) {
            return parameter;
        } else {
            ParameterImpl result = new ParameterImpl(this, parameterName);
            parameters.put(parameterName, result);
            return result;
        }
    }

    /** Convenience method to negate a predicate.
     * @param predicate the predicate to negate
     * @return the inverted predicate
     */
    public Predicate not(Predicate predicate) {
        return predicate.not();
    }

    /** Query.getResultList delegates to this method.
     * @param skip the number of rows to skip
     * @param limit the limit of rows to return after skipping
     * @param orderingFields 
     * @param ordering 
     * 
     * @return the results of executing the query
     */
    public List<T> getResultList(QueryExecutionContext context, long skip, long limit,
            Ordering ordering, String[] orderingFields) {
        assertAllParametersBound(context);

        SessionSPI session = context.getSession();
        session.startAutoTransaction();
        // set up results and table information
        List<T> resultList = new ArrayList<T>();
        try {
            // execute the query
            ResultData resultData = getResultData(context, skip, limit, ordering, orderingFields);
            // put the result data into the result list
            while (resultData.next()) {
                T row = session.newInstance(resultData, domainTypeHandler);
                resultList.add(row);
            }
            session.endAutoTransaction();
            return resultList;
        } catch (ClusterJException ex) {
            session.failAutoTransaction();
            throw ex;
        } catch (Exception ex) {
            session.failAutoTransaction();
            throw new ClusterJException(
                    local.message("ERR_Exception_On_Query"), ex);
        }
    }

    /** Execute the query and return the result data. The type of operation
     * (primary key lookup, unique key lookup, index scan, or table scan) 
     * depends on the where clause and the bound parameter values.
     * 
     * @param context the query context, including the bound parameters
     * @param skip the number of rows to skip
     * @param limit the limit of rows to return after skipping
     * @param orderingFields 
     * @param ordering 
     * @return the raw result data from the query
     * @throws ClusterJUserException if not all parameters are bound
     */
    public ResultData getResultData(QueryExecutionContext context, long skip, long limit,
            Ordering ordering, String[] orderingFields) {
        SessionSPI session = context.getSession();
        this.ordering = ordering;
        this.orderingFields = orderingFields;
        // execute query based on what kind of scan is needed
        // if no where clause, scan the entire table
        index = getCandidateIndex(context);
        ScanType scanType = index.getScanType();

        if (logger.isDebugEnabled()) logger.debug("using index " + index.getIndexName() + " with scanType " + scanType);
        Map<String, Object> explain = newExplain(index, scanType);
        context.setExplain(explain);
        ResultData result = null;
        Index storeIndex;
        Operation op = null;

        try {
            switch (scanType) {

                case PRIMARY_KEY: {
                    // if skipping any results or limit is zero, return no results
                    if (skip > 0 || limit < 1) {
                        return resultDataEmpty;
                    }
                    // perform a select operation
                    op = session.getSelectOperation(domainTypeHandler.getStoreTable());
                    op.beginDefinition();
                    // set key values into the operation
                    index.operationSetKeys(context, op);
                    // set the expected columns into the operation
                    domainTypeHandler.operationGetValues(op);
                    op.endDefinition();
                    // execute the select and get results
                    result = op.resultData();
                    break;
                }

                case INDEX_SCAN: {
                    storeIndex = index.getStoreIndex();
                    // perform an index scan operation
                    if (index.isMultiRange()) {
                        op = session.getIndexScanOperationMultiRange(storeIndex, domainTypeHandler.getStoreTable());
                        
                    } else {
                        op = session.getIndexScanOperation(storeIndex, domainTypeHandler.getStoreTable());
                        
                    }
                    op.beginDefinition();
                    // set ordering if not already set to allow skip and limit to work
                    if (ordering == null && (skip != 0 || limit != Long.MAX_VALUE)) {
                        ordering = Ordering.ASCENDING;
                    }
                    ((ScanOperation)op).setOrdering(ordering);
                    // set the expected columns into the operation
                    domainTypeHandler.operationGetValues(op);
                    // set the bounds into the operation
                    index.operationSetBounds(context, (IndexScanOperation)op);
                    // set additional filter conditions
                    if (where != null) {
                        where.filterCmpValue(context, (IndexScanOperation)op);
                    }
                    op.endDefinition();
                    // execute the scan and get results
                    result = ((ScanOperation)op).resultData(true, skip, limit);
                    break;
                }

                case TABLE_SCAN: {
                    if (ordering != null) {
                        throw new ClusterJUserException(local.message("ERR_Cannot_Use_Ordering_With_Table_Scan"));
                    }
                    // perform a table scan operation
                    op = session.getTableScanOperation(domainTypeHandler.getStoreTable());
                    op.beginDefinition();
                    // set the expected columns into the operation
                    domainTypeHandler.operationGetValues(op);
                    // set filter conditions into the operation
                    if (where != null) {
                        where.filterCmpValue(context, (ScanOperation)op);
                    }
                    op.endDefinition();
                    // execute the scan and get results
                    result = ((ScanOperation)op).resultData(true, skip, limit);
                    break;
                }

                case UNIQUE_KEY: {
                    // if skipping any results or limit is zero, return no results
                    if (skip > 0 || limit < 1) {
                        return resultDataEmpty;
                    }
                    storeIndex = index.getStoreIndex();
                    // perform a unique lookup operation
                    op = session.getUniqueIndexOperation(storeIndex, domainTypeHandler.getStoreTable());
                    op.beginDefinition();
                    // set the keys of the indexName into the operation
                    where.operationEqual(context, op);
                    // set the expected columns into the operation
                    //domainTypeHandler.operationGetValuesExcept(op, indexName);
                    domainTypeHandler.operationGetValues(op);
                    op.endDefinition();
                    // execute the select and get results
                    result = op.resultData();
                    break;
                }

                default:
                    session.failAutoTransaction();
                    throw new ClusterJFatalInternalException(
                            local.message("ERR_Illegal_Scan_Type", scanType));
            }
        }
        catch (ClusterJException ex) {
            if (op != null) {
                op.freeResourcesAfterExecute();
            }
            session.failAutoTransaction();
            throw ex;
        } catch (Exception ex) {
            if (op != null) {
                op.freeResourcesAfterExecute();
            }
            session.failAutoTransaction();
            throw new ClusterJException(
                    local.message("ERR_Exception_On_Query"), ex);
        }
        return result;
    }

    /** Delete the instances that satisfy the query and return the number
     * of instances deleted. The type of operation used to find the instances
     * (primary key lookup, unique key lookup, index scan, or table scan) 
     * depends on the where clause and the bound parameter values.
     * 
     * @param context the query context, including the bound parameters
     * @return the number of instances deleted
     * @throws ClusterJUserException if not all parameters are bound
     */
    public int deletePersistentAll(QueryExecutionContext context) {
        SessionSPI session = context.getSession();
        // calculate what kind of scan is needed
        // if no where clause, scan the entire table
        index = getCandidateIndex(context);
        ScanType scanType = index.getScanType();
        Map<String, Object> explain = newExplain(index, scanType);
        context.setExplain(explain);
        int result = 0;
        int errorCode = 0;
        Index storeIndex;
        session.startAutoTransaction();
        Operation op = null;
        try {
            switch (scanType) {

                case PRIMARY_KEY: {
                    // perform a delete by primary key operation
                    if (logger.isDetailEnabled()) logger.detail("Using delete by primary key.");
                    op = session.getDeleteOperation(domainTypeHandler.getStoreTable());
                    op.beginDefinition();
                    // set key values into the operation
                    index.operationSetKeys(context, op);
                    op.endDefinition();
                    // execute the delete operation
                    session.executeNoCommit(false, true);
                    errorCode = op.errorCode();
                    // a non-zero result means the row was not deleted
                    result = (errorCode == 0?1:0);
                    break;
                }

                case UNIQUE_KEY: {
                    storeIndex = index.getStoreIndex();
                    if (logger.isDetailEnabled()) logger.detail(
                            "Using delete by unique key  " + index.getIndexName());
                    // perform a delete by unique key operation
                    op = session.getUniqueIndexDeleteOperation(storeIndex,
                            domainTypeHandler.getStoreTable());
                    // set the keys of the indexName into the operation
                    where.operationEqual(context, op);
                    // execute the delete operation
                    session.executeNoCommit(false, true);
                    errorCode = op.errorCode();
                    // a non-zero result means the row was not deleted
                    result = (errorCode == 0?1:0);
                    break;
                }

                case INDEX_SCAN: {
                    storeIndex = index.getStoreIndex();
                    if (logger.isDetailEnabled()) logger.detail(
                            "Using delete by index scan with index " + index.getIndexName());
                    // perform an index scan operation
                    op = session.getIndexScanDeleteOperation(storeIndex,
                            domainTypeHandler.getStoreTable());
                    // set the expected columns into the operation
                    domainTypeHandler.operationGetValues(op);
                    // set the bounds into the operation
                    index.operationSetBounds(context, (IndexScanOperation)op);
                    // set additional filter conditions
                    where.filterCmpValue(context, (IndexScanOperation)op);
                    // delete results of the scan; don't abort if no row found
                    result = session.deletePersistentAll((IndexScanOperation)op, false);
                    break;
                }

                case TABLE_SCAN: {
                    if (logger.isDetailEnabled()) logger.detail("Using delete by table scan");
                    // perform a table scan operation
                    op = session.getTableScanDeleteOperation(domainTypeHandler.getStoreTable());
                    // set the expected columns into the operation
                    domainTypeHandler.operationGetValues(op);
                    // set the bounds into the operation
                    if (where != null) {
                        where.filterCmpValue(context, (ScanOperation)op);
                    }
                    // delete results of the scan; don't abort if no row found
                    result = session.deletePersistentAll((ScanOperation)op, false);
                    break;
                }

                default:
                    throw new ClusterJFatalInternalException(
                            local.message("ERR_Illegal_Scan_Type", scanType));
            }
            session.endAutoTransaction();
            return result;
        } catch (ClusterJException e) {
            if (op != null) {
                op.freeResourcesAfterExecute();
            }
            session.failAutoTransaction();
            throw e;
        } catch (Exception e) {
            if (op != null) {
                op.freeResourcesAfterExecute();
            }
            session.failAutoTransaction();
            throw new ClusterJException(local.message("ERR_Exception_On_Query"), e);
        } 
    }

    /** Update the instances that satisfy the query and return the number
     * of instances updated. The type of operation used to update the instances
     * (primary key lookup, unique key lookup, index scan, or table scan) 
     * depends on the where clause and the bound parameter values.
     * Currently only update by primary key or unique key are supported.
     * 
     * @param context the query context, including the batch of parameters
     * @return the number of instances updated for each set in the batch of parameters
     * @throws ClusterJUserException if not all parameters are bound
     */
    public long[] updatePersistentAll(QueryExecutionContext context, ValueHandlerBatching valueHandler) {
        SessionSPI session = context.getSession();
        // calculate what kind of scan is needed
        // if no where clause, scan the entire table
        index = getCandidateIndex(context);
        ScanType scanType = index.getScanType();
        Map<String, Object> explain = newExplain(index, scanType);
        context.setExplain(explain);
        long[] result = null;
        List<Operation> ops = new ArrayList<Operation>();

        try {
            switch (scanType) {

                case PRIMARY_KEY: {
                    result = new long[valueHandler.getNumberOfStatements()];
                    session.startAutoTransaction();
                    // perform an update by primary key operation
                    if (logger.isDetailEnabled()) logger.detail("Update by primary key.");
                    // iterate the valueHandlerBatching and for each set of parameters, update one row
                    while (valueHandler.next()) {
                        Operation op = session.getUpdateOperation(domainTypeHandler.getStoreTable());
                        // set key values into the operation
                        index.operationSetKeys(context, op);
                        // set modified field values into the operation
                        domainTypeHandler.operationSetModifiedNonPKValues(valueHandler, op);
                        ops.add(op);
                    }
                    // execute the update operations
                    session.endAutoTransaction();
                    if (session.currentTransaction().isActive()) {
                        // autotransaction is not set; need to explicitly execute the transaction
                        session.executeNoCommit(false, true);
                    }
                    for (int i = 0; i < result.length; i++) {
                        // evaluate the results
                        int errorCode = ops.get(i).errorCode();
                        // a non-zero result means the row was not updated
                        result[i] = (errorCode == 0 ? 1 : 0);
                    }
                    break;
                }

                case UNIQUE_KEY: {
                    result = new long[valueHandler.getNumberOfStatements()];
                    session.startAutoTransaction();
                    Index storeIndex = index.getStoreIndex();
                    if (logger.isDetailEnabled()) logger.detail("Update by unique key.");
                    // iterate the valueHandlerBatching and for each set of parameters, update one row
                    while (valueHandler.next()) {
                        IndexOperation op = session.getUniqueIndexUpdateOperation(storeIndex,
                            domainTypeHandler.getStoreTable());
                        // set the keys of the indexName into the operation
                        index.operationSetKeys(context, op);
                        // set modified field values into the operation
                        domainTypeHandler.operationSetModifiedNonPKValues(valueHandler, op);
                        ops.add(op);
                    }
                    // execute the update operations
                    session.endAutoTransaction();
                    if (session.currentTransaction().isActive()) {
                        // autotransaction is not set; need to explicitly execute the transaction
                        session.executeNoCommit(false, true);
                    }
                    for (int i = 0; i < result.length; i++) {
                        // evaluate the results
                        int errorCode = ops.get(i).errorCode();
                        // a non-zero result means the row was not updated
                        result[i] = (errorCode == 0 ? 1 : 0);
                    }
                    break;
                }

                case INDEX_SCAN: {
                    // not supported
                    break;
                }

                case TABLE_SCAN: {
                    // not supported
                    break;
                }

                default:
                    throw new ClusterJFatalInternalException(
                            local.message("ERR_Illegal_Scan_Type", scanType));
            }
            return result;
        } catch (ClusterJException e) {
            for (Operation op: ops) {
                op.freeResourcesAfterExecute();
            }
            session.failAutoTransaction();
            throw e;
        } catch (Exception e) {
            for (Operation op: ops) {
                op.freeResourcesAfterExecute();
            }
            session.failAutoTransaction();
            throw new ClusterJException(local.message("ERR_Exception_On_Query"), e);
        } 
    }

    protected CandidateIndexImpl[] createCandidateIndexes() {
        return domainTypeHandler.createCandidateIndexes();
    }

    /** Explain how this query will be or was executed and store
     * the result in the context.
     * 
     * @param context the context, including bound parameters
     */
    public void explain(QueryExecutionContext context) {
        assertAllParametersBound(context);
        CandidateIndexImpl index = getCandidateIndex(context);
        ScanType scanType = index.getScanType();
        Map<String, Object> explain = newExplain(index, scanType);
        context.setExplain(explain);
    }

    private CandidateIndexImpl getCandidateIndex(QueryExecutionContext context) {
        if (where == null) {
            // there is no filter, so without ordering this is a table scan
            // with ordering, choose an index that contains all ordering fields
            CandidateIndexImpl[] candidateIndexImpls = domainTypeHandler.createCandidateIndexes();
            for (CandidateIndexImpl candidateIndexImpl: candidateIndexImpls) {
                // choose the first index that contains all ordering fields
                if (candidateIndexImpl.containsAllOrderingFields(orderingFields)) {
                    index = candidateIndexImpl;
                    return index;
                }
            }
            index = CandidateIndexImpl.getIndexForNullWhereClause();
        } else {
            // there is a filter; choose the best index that contains all ordering fields
            index = where.getBestCandidateIndex(context, orderingFields);
        }
        return index;
    }

    /** Create a new explain for this query.
     * @param index the index used
     * @param scanType the scan type
     * @return the explain
     */
    protected Map<String, Object> newExplain(CandidateIndexImpl index,
            ScanType scanType) {
        Map<String, Object> explain = new HashMap<String, Object>();
        explain.put(Query.SCAN_TYPE, scanType.toString());
        explain.put(Query.INDEX_USED, index.getIndexName());
        return explain;
    }

    /** Assert that all parameters used by this query are bound.
     * @param context the context, including the parameter map
     * @throws ClusterJUserException if not all parameters are bound
     */
    protected void assertAllParametersBound(QueryExecutionContext context) {
        if (where != null) {
            // Make sure all marked parameters (used in the query) are bound.
            for (ParameterImpl param: parameters.values()) {
                if (param.isMarkedAndUnbound(context)) {
                    throw new ClusterJUserException(
                            local.message("ERR_Parameter_Not_Bound", param.getName()));
                }
            }
        }
    }

    public Class<T> getType() {
        return cls;
    }

    private ResultData resultDataEmpty = new ResultData() {

        public boolean next() {
            // this ResultData has no results
            return false;
        }
        
        public BigInteger getBigInteger(Column columnName) {
            return null;
        }

        public BigInteger getBigInteger(int columnNumber) {
            return null;
        }

        public Blob getBlob(Column storeColumn) {
            return null;
        }

        public Blob getBlob(int columnNumber) {
            return null;
        }

        public boolean getBoolean(Column storeColumn) {
            return false;
        }

        public boolean getBoolean(int columnNumber) {
            return false;
        }

        public boolean[] getBooleans(Column storeColumn) {
            return null;
        }

        public boolean[] getBooleans(int columnNumber) {
            return null;
        }

        public byte getByte(Column storeColumn) {
            return 0;
        }

        public byte getByte(int columnNumber) {
            return 0;
        }

        public byte[] getBytes(Column storeColumn) {
            return null;
        }

        public byte[] getBytes(int columnNumber) {
            return null;
        }

        public Column[] getColumns() {
            return null;
        }

        public BigDecimal getDecimal(Column storeColumn) {
            return null;
        }

        public BigDecimal getDecimal(int columnNumber) {
            return null;
        }

        public double getDouble(Column storeColumn) {
            return 0;
        }

        public double getDouble(int columnNumber) {
            return 0;
        }

        public float getFloat(Column storeColumn) {
            return 0;
        }

        public float getFloat(int columnNumber) {
            return 0;
        }

        public int getInt(Column storeColumn) {
            return 0;
        }

        public int getInt(int columnNumber) {
            return 0;
        }

        public long getLong(Column storeColumn) {
            return 0;
        }

        public long getLong(int columnNumber) {
            return 0;
        }

        public Object getObject(Column storeColumn) {
            return null;
        }

        public Object getObject(int column) {
            return null;
        }

        public Boolean getObjectBoolean(Column storeColumn) {
            return null;
        }

        public Boolean getObjectBoolean(int columnNumber) {
            return null;
        }

        public Byte getObjectByte(Column storeColumn) {
            return null;
        }

        public Byte getObjectByte(int columnNumber) {
            return null;
        }

        public Double getObjectDouble(Column storeColumn) {
            return null;
        }

        public Double getObjectDouble(int columnNumber) {
            return null;
        }

        public Float getObjectFloat(Column storeColumn) {
            return null;
        }

        public Float getObjectFloat(int columnNumber) {
            return null;
        }

        public Integer getObjectInteger(Column storeColumn) {
            return null;
        }

        public Integer getObjectInteger(int columnNumber) {
            return null;
        }

        public Long getObjectLong(Column storeColumn) {
            return null;
        }

        public Long getObjectLong(int columnNumber) {
            return null;
        }

        public Short getObjectShort(Column storeColumn) {
            return null;
        }

        public Short getObjectShort(int columnNumber) {
            return null;
        }

        public short getShort(Column storeColumn) {
            return 0;
        }

        public short getShort(int columnNumber) {
            return 0;
        }

        public String getString(Column storeColumn) {
            return null;
        }

        public String getString(int columnNumber) {
            return null;
        }

    };
}
