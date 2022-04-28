/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

    /** Return a Predicate representing comparing this to null.
     *
     * @return a new Predicate
     */
    Predicate isNull();

    /** Return a Predicate representing comparing this to not null.
     *
     * @return a new Predicate
     */
    Predicate isNotNull();

}
