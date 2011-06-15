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

package testsuite.clusterj;

import testsuite.clusterj.model.Employee;

public class QueryBtreeIndexScanTest extends AbstractQueryTest {

    @Override
    public Class getInstanceType() {
        return Employee.class;
    }

    @Override
    void createInstances(int number) {
        createEmployeeInstances(10);
        instances.addAll(employees);
    }

    /** Test all single-predicate queries using Employee.age, which has a
     * btree index defined.
     */
    public void testBtreeIndexScan() {
        equalQuery("age", "idx_btree_age", 8, 8);
        greaterEqualQuery("age", "idx_btree_age", 7, 7, 8, 9);
        greaterThanQuery("age", "idx_btree_age", 6, 7, 8, 9);
        lessEqualQuery("age", "idx_btree_age", 4, 4, 3, 2, 1, 0);
        lessThanQuery("age", "idx_btree_age", 4, 3, 2, 1, 0);
        betweenQuery("age", "idx_btree_age", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("age", "idx_btree_age", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("age", "idx_btree_age", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("age", "idx_btree_age", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("age", "idx_btree_age", 4, 6, 5);

        failOnError();
    }

}
