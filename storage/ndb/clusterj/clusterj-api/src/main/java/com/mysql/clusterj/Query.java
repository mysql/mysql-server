/*
   Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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

}
