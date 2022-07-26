/*
   Copyright (c) 2009, 2022, Oracle and/or its affiliates.

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

package com.mysql.clusterj;

import java.util.List;
import java.util.Map;

/**
 * A Query instance represents a specific query with bound parameters.
 * The instance is created by the method {@link com.mysql.clusterj.Session#createQuery}.
 */
public interface Query<E> {

    /** The query explain scan type key */
    public static final String SCAN_TYPE = "ScanType";

    /** The query explain scan type value for primary key */
    static final String SCAN_TYPE_PRIMARY_KEY = "PRIMARY_KEY";

    /** The query explain scan type value for unique key */
    static final String SCAN_TYPE_UNIQUE_KEY = "UNIQUE_KEY";

    /** The query explain scan type value for index scan */
    static final String SCAN_TYPE_INDEX_SCAN = "INDEX_SCAN";

    /** The query explain scan type value for table scan */
    static final String SCAN_TYPE_TABLE_SCAN = "TABLE_SCAN";

    /** The query explain index used key */
    static final String INDEX_USED = "IndexUsed";

    /** Ordering */
    static enum Ordering {
        ASCENDING, 
        DESCENDING;
    };

    /** Set the value of a parameter. If called multiple times for the same
     * parameter, silently replace the value.
     * @param parameterName the name of the parameter
     * @param value the value for the parameter
     */
    void setParameter(String parameterName, Object value);

    /** Get the results as a list.
     * @return the result
     * @throws ClusterJUserException if not all parameters are bound
     * @throws ClusterJDatastoreException if an exception is reported by the datastore
     */
    List<E> getResultList();

    /** Delete the instances that satisfy the query criteria.
     * @return the number of instances deleted
     */
    int deletePersistentAll();

    /** Execute the query with exactly one parameter.
     * @param parameter the parameter
     * @return the result
     */
    Results<E> execute(Object parameter);

    /** Execute the query with one or more parameters.
     * Parameters are resolved in the order they were declared in the query.
     * @param parameters the parameters
     * @return the result
     */
    Results<E> execute(Object... parameters);

    /** Execute the query with one or more named parameters.
     * Parameters are resolved by name.
     * @param parameters the parameters
     * @return the result
     */
    Results<E> execute(Map<String, ?> parameters);

    /**
     * Explain how this query will be or was executed.
     * If called before binding all parameters, throws ClusterJUserException.
     * Return a map of key:value pairs that explain
     * how the query will be or was executed.
     * Details can be obtained by calling toString on the value.
     * The following keys are returned:
     * <ul><li>ScanType: the type of scan, with values:
     * <ul><li>PRIMARY_KEY: the query used key lookup with the primary key
     * </li><li>UNIQUE_KEY: the query used key lookup with a unique key
     * </li><li>INDEX_SCAN: the query used a range scan with a non-unique key
     * </li><li>TABLE_SCAN: the query used a table scan
     * </li></ul>
     * </li><li>IndexUsed: the name of the index used, if any
     * </li></ul>
     * @return the data about the execution of this query
     * @throws ClusterJUserException if not all parameters are bound
     */
    Map<String, Object> explain();

    /**
     * Set limits on results to return. The execution of the query is
     * modified to return only a subset of results. If the filter would
     * normally return 100 instances, skip is set to 50, and
     * limit is set to 40, then the first 50 results that would have 
     * been returned are skipped, the next 40 results are returned and the
     * remaining 10 results are ignored.
     * <p>
     * Skip must be greater than or equal to 0. Limit must be greater than or equal to 0.
     * Limits may not be used with deletePersistentAll.
     * @param skip the number of results to skip
     * @param limit the number of results to return after skipping;
     * use Long.MAX_VALUE for no limit.
     */
    void setLimits (long skip, long limit);

    /** Set ordering for the results of this query. The execution of the query
     * is modified to use an index previously defined.
     * <ul><li>There must be an index defined on the columns mapped to
     * the ordering fields, in the order of the ordering fields.
     * </li><li>There must be no gaps in the ordering fields relative to the index.
     * </li><li>All ordering fields must be in the index, but not all
     * fields in the index need be in the ordering fields.
     * </li><li>If an "in" predicate is used in the filter on a field in the ordering,
     * it can only be used with the first field.
     * </li><li>If any of these conditions is violated, ClusterJUserException is
     * thrown when the query is executed.
     * </li></ul>
     * If an "in" predicate is used, each element in the parameter
     * defines a separate range, and ordering is performed within that range.
     * There may be a better (more efficient) index based on the filter,
     * but specifying the ordering will force the query to use an index
     * that contains the ordering fields.
     * @param ordering either Ordering.ASCENDING or Ordering.DESCENDING
     * @param orderingFields the fields to order by
     */
    void setOrdering(Ordering ordering, String... orderingFields);

}
