/*
   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

/** PredicateOperand represents a column or parameter that can be compared to
 * another
 *
 */
public interface PredicateOperand {

    /** Return a Predicate representing comparing this to another using
     * "equal to" semantics.
     *
     * @param other the other PredicateOperand
     * @return a new Predicate
     */
    Predicate equal(PredicateOperand other);

    /** Return a Predicate representing comparing this to another using
     * "greater than" semantics.
     *
     * @param other the other PredicateOperand
     * @return a new Predicate
     */
    Predicate greaterThan(PredicateOperand other);

    /** Return a Predicate representing comparing this to another using
     * "greater than or equal to" semantics.
     *
     * @param other the other PredicateOperand
     * @return a new Predicate
     */
    Predicate greaterEqual(PredicateOperand other);

    /** Return a Predicate representing comparing this to another using
     * "less than" semantics.
     *
     * @param other the other PredicateOperand
     * @return a new Predicate
     */
    Predicate lessThan(PredicateOperand other);

    /** Return a Predicate representing comparing this to another using
     * "less than or equal to" semantics.
     *
     * @param other the other PredicateOperand
     * @return a new Predicate
     */
    Predicate lessEqual(PredicateOperand other);

    /** Return a Predicate representing comparing this to another using
     * "between" semantics.
     *
     * @param lower another PredicateOperand
     * @param upper another PredicateOperand
     * @return a new Predicate
     */
    Predicate between(PredicateOperand lower, PredicateOperand upper);

    /** Return a Predicate representing comparing this to a collection of
     * values using "in" semantics.
     *
     * @param other another PredicateOperand
     * @return a new Predicate
     */
    Predicate in(PredicateOperand other);

    /** Return a Predicate representing comparing this to another using 
     * "like" semantics.
     *
     * @param other another PredicateOperand
     * @return a new Predicate
     */
    Predicate like(PredicateOperand other);

}
