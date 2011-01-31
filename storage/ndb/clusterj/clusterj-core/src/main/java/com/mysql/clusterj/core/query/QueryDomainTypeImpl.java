/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.clusterj.core.query;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Query;

import com.mysql.clusterj.core.query.PredicateImpl.ScanType;
import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.spi.ValueHandler;

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
     * 
     * @return the results of executing the query
     */
    public List<T> getResultList(QueryExecutionContextImpl context) {
        assertAllParametersBound(context);

        SessionSPI session = context.getSession();
        session.startAutoTransaction();
        // set up results and table information
        List<T> resultList = new ArrayList<T>();
        try {
            // execute the query
            ResultData resultData = getResultData(context);
            // put the result data into the result list
            while (resultData.next()) {
                T row = (T) session.newInstance(cls);
                ValueHandler handler =domainTypeHandler.getValueHandler(row);
                // set values from result set into object
                domainTypeHandler.objectSetValues(resultData, handler);
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
     * @return the raw result data from the query
     * @throws ClusterJUserException if not all parameters are bound
     */
    public ResultData getResultData(QueryExecutionContextImpl context) {
	SessionSPI session = context.getSession();
        // execute query based on what kind of scan is needed
        // if no where clause, scan the entire table
        CandidateIndexImpl index = where==null?
            CandidateIndexImpl.getIndexForNullWhereClause():
            where.getBestCandidateIndex(context);
        ScanType scanType = index.getScanType();
        Map<String, Object> explain = newExplain(index, scanType);
        context.setExplain(explain);
        ResultData result = null;
        Index storeIndex;

        switch (scanType) {

            case PRIMARY_KEY: {
                // perform a select operation
                Operation op = session.getSelectOperation(domainTypeHandler.getStoreTable());
                // set key values into the operation
                index.operationSetKeys(context, op);
                // set the expected columns into the operation
                domainTypeHandler.operationGetValues(op);
                // execute the select and get results
                result = op.resultData();
                break;
            }

            case INDEX_SCAN: {
                storeIndex = index.getStoreIndex();
                if (logger.isDetailEnabled()) logger.detail("Using index scan with index " + index.getIndexName());

                // perform an index scan operation
                IndexScanOperation op = session.getIndexScanOperation(storeIndex, domainTypeHandler.getStoreTable());
                // set the expected columns into the operation
                domainTypeHandler.operationGetValues(op);
                // set the bounds into the operation
                index.operationSetBounds(context, op);
                // set additional filter conditions
                where.filterCmpValue(context, op);
                // execute the scan and get results
                result = op.resultData();
                break;
            }

            case TABLE_SCAN: {
                if (logger.isDetailEnabled()) logger.detail("Using table scan");
                // perform a table scan operation
                ScanOperation op = session.getTableScanOperation(domainTypeHandler.getStoreTable());
                // set the expected columns into the operation
                domainTypeHandler.operationGetValues(op);
                // set the bounds into the operation
                if (where != null) {
                    where.filterCmpValue(context, op);
                }
                // execute the scan and get results
                result = op.resultData();
                break;
            }

            case UNIQUE_KEY: {
                storeIndex = index.getStoreIndex();
                if (logger.isDetailEnabled()) logger.detail("Using unique lookup with index " + index.getIndexName());
                // perform a unique lookup operation
                IndexOperation op = session.getIndexOperation(storeIndex, domainTypeHandler.getStoreTable());
                // set the keys of the indexName into the operation
                where.operationEqual(context, op);
                // set the expected columns into the operation
                //domainTypeHandler.operationGetValuesExcept(op, indexName);
                domainTypeHandler.operationGetValues(op);
                // execute the select and get results
                result = op.resultData();
                break;
            }

            default:
                session.failAutoTransaction();
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Illegal_Scan_Type", scanType));
        }
        context.deleteFilters();
        return result;
    }

    protected CandidateIndexImpl[] createCandidateIndexes() {
        return domainTypeHandler.createCandidateIndexes();
    }

    /** Explain how this query will be or was executed and store
     * the result in the context.
     * 
     * @param context the context, including bound parameters
     */
    public void explain(QueryExecutionContextImpl context) {
        assertAllParametersBound(context);
        CandidateIndexImpl index = where==null?
                CandidateIndexImpl.getIndexForNullWhereClause():
                where.getBestCandidateIndex(context);
        ScanType scanType = index.getScanType();
        Map<String, Object> explain = newExplain(index, scanType);
        context.setExplain(explain);
    }

    /** Create a new explain for this query.
     * @param index the index used
     * @param scanType the scan type
     * @return
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
    protected void assertAllParametersBound(QueryExecutionContextImpl context) {
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

}
