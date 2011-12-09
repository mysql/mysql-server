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
