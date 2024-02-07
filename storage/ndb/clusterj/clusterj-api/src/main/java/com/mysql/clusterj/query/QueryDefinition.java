/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.query;

/** QueryDefinition allows users to define queries.
 *
 */
public interface QueryDefinition<E> {

    /** Specify the predicate to satisfy the query.
     *
     * @param predicate the Predicate
     * @return this query definition
     */
    QueryDefinition<E> where (Predicate predicate);

    /** Specify a parameter for the query.
     *
     * @param parameterName the name of the parameter
     * @return the PredicateOperand representing the parameter
     */
    PredicateOperand param(String parameterName);

    /** Convenience method to negate a predicate.
     * @param predicate the predicate to negate
     * @return the inverted predicate
     */
    public Predicate not(Predicate predicate);

}
