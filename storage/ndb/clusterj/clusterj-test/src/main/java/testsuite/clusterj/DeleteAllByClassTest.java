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
        assertEquals("Mismatch on number of deleted instances: ",
                NUMBER_TO_INSERT, count);
    }
}
