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

public class DeleteInsertTest extends AbstractClusterJModelTest {

    private static final String tablename = "t_basic";

    private static final int NUMBER_TO_INSERT = 200;
    
    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createEmployeeInstances(NUMBER_TO_INSERT);
        tx = session.currentTransaction();
        int count = 0;
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            tx.begin();
            session.deletePersistent(Employee.class, i);
            try {
                tx.commit();
                ++count;
            } catch (Exception ex) {
                // ignore exceptions -- might not be any instances to delete
            }
        }
        addTearDownClasses(Employee.class);
//        System.out.println("Deleted " + count + " instances.");
    }

    public void testInsert() {
        tx = session.currentTransaction();
        tx.begin();

        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            session.makePersistent(employees.get(i));
        }

        tx.commit();
    }
}
