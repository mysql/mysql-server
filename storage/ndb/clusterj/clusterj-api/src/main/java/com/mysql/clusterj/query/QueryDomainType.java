/*
   Copyright 2010 Sun Microsystems, Inc.
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

package com.mysql.clusterj.query;

/** QueryDomainType represents the domain type of a query. The domain type
 * validates property names that are used to filter results.
 *
 */
public interface QueryDomainType<E> extends QueryDefinition<E> {

    /** Get a PredicateOperand representing a property of the domain type.
     * 
     * @param propertyName the name of the property
     * @return a representation the value of the property
     */
    PredicateOperand get(String propertyName);

    /** Get the domain type of the query.
     *
     * @return the domain type of the query
     */
    Class<E> getType();

}
