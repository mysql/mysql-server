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

public class FindByPrimaryKeyTest extends AbstractClusterJModelTest {

    private static final String tablename = "t_basic";

    private static final int NUMBER_TO_INSERT = 60;

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createEmployeeInstances(NUMBER_TO_INSERT);
        tx = session.currentTransaction();
        tx.begin();
        session.deletePersistentAll(Employee.class);
        tx.commit();
        addTearDownClasses(Employee.class);
    }

    public void testFind() {
        // first, create instances to find
        tx = session.currentTransaction();
        tx.begin();
        
        int count = 0;

        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            session.makePersistent(employees.get(i));
            ++count;
        }
        tx.commit();

        tx.begin();
        
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            Employee e = session.find(Employee.class, i);
            // make sure all fields were fetched
            consistencyCheck(e);
            // see if it is the right Employee
            int actualId = e.getId();
            if (actualId != i) {
                error("Expected Employee.id " + i + " but got " + actualId);
            }
        }
        tx.commit();
        failOnError();
        
        // try to find a non-existing row with autocommit on
        Employee e1 = session.find(Employee.class, NUMBER_TO_INSERT + 100);
        if (e1 != null) {
            error("Autocommit found non-existing row " + e1.getId());
        }
        
        // try to find a non-existing row with autocommit off
        tx = session.currentTransaction();
        tx.begin();
        Employee e2 = session.find(Employee.class, NUMBER_TO_INSERT + 100);
        tx.commit();
        if (e2 != null) {
            error("Non-autocommit found non-existing row " + e2.getId());
        }
    }
}
