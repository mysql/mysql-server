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
