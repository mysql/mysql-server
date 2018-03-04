/*
   Copyright 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

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
