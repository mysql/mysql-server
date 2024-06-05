/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

package testsuite.clusterj;

import testsuite.clusterj.model.Employee;

public class QueryHashIndexScanTest extends AbstractQueryTest {

    @Override
    public Class getInstanceType() {
        return Employee.class;
    }

    @Override
    void createInstances(int number) {
        createEmployeeInstances(10);
        instances.addAll(employees);
    }

    /** Test all single-predicate queries using Employee.magic, which has a
     * hash index defined.
     *
     */
    public void testHashIndexScan() {
        equalQuery("magic", "idx_unique_hash_magic", 8, 8);
        greaterEqualQuery("magic", "none", 7, 7, 8, 9);
        greaterThanQuery("magic", "none", 6, 7, 8, 9);
        lessEqualQuery("magic", "none", 4, 4, 3, 2, 1, 0);
        lessThanQuery("magic", "none", 4, 3, 2, 1, 0);
        betweenQuery("magic", "none", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("magic", "none", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("magic", "none", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("magic", "none", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("magic", "none", 4, 6, 5);

        failOnError();
    }

}
