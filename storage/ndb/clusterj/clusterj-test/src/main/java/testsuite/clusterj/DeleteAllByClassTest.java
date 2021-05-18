/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

package testsuite.clusterj;

import testsuite.clusterj.model.Employee;

public class DeleteAllByClassTest extends AbstractClusterJModelTest {

    private static final String tablename = "t_basic";

    private static final int NUMBER_TO_INSERT = 200;
    
    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        session.deletePersistentAll(Employee.class);
        createEmployeeInstances(NUMBER_TO_INSERT);
        tx = session.currentTransaction();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            try {
                tx.begin();
                session.makePersistent(employees.get(i));
                tx.commit();
            } catch (Throwable t) {
                // ignore possible duplicate keys
            } finally {
                if (tx.isActive()) {
                    tx.rollback();
                }
            }
        }
        addTearDownClasses(Employee.class);
    }

    public void testDeleteAllByClass() {
        tx = session.currentTransaction();
        tx.begin();
        int count = session.deletePersistentAll(Employee.class);
        tx.commit();
        errorIfNotEqual("Mismatch on number of deleted instances: ",
                NUMBER_TO_INSERT, count);
        failOnError();
    }
}
