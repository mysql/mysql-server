/*
   Copyright (C) 2009 Sun Microsystems Inc.
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

package com.mysql.clusterj;

import java.util.List;
import java.util.Map;

/**
 * A Query instance represents a specific query with bound parameters.
 * The instance is created by the method {@link com.mysql.clusterj.Session#createQuery}.
 */
public interface Query<E> {

    /** Set the value of a parameter.
     * @param parameterName the name of the parameter
     * @param value the value for the parameter
     */
    void setParameter(String parameterName, Object value);

    /** Get the results as a list.
     * @return the result
     */
    List<E> getResultList();

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
     * Explain this query.
     * @return the data about the execution of this query
     */
    Map<String, Object> explain();

}
