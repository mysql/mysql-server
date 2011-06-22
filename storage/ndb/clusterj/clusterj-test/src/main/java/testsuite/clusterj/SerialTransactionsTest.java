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

public class SerialTransactionsTest extends AbstractClusterJModelTest {

    private static final int NUMBER_TO_INSERT = 4;

    protected int numberOfEmployees() {
        return NUMBER_TO_INSERT;
    }

    @Override
    public void localSetUp() {
        createSessionFactory();
        createSession();
        createEmployeeInstances(NUMBER_TO_INSERT);
        addTearDownClasses(Employee.class);
    }


    public void test() {
        deleteAll();
        createSession();
        findAll();
        createSession();
        createAll();
        createSession();
        findAll();
        createSession();
        findAll();
        createSession();
        updateThenVerifyAll();
        failOnError();
    }

    protected void createAll() {
        tx.begin();
        session.makePersistentAll(employees);
        tx.commit();
    }

    /** Find instances. */
    public void findAll() {
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.find(Employee.class, i);
        }
        tx.commit();
    }

    public void deleteAll() {
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.find(Employee.class, i);
            session.deletePersistent(e);
        }
        tx.commit();
    }

    public void updateThenVerifyAll() {
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.find(Employee.class, i);
            // change age 
            e.setAge(NUMBER_TO_INSERT - i);
            // send the change to the database
            session.updatePersistent(e);
        }
        tx.commit();
        
        // now verify that the changes were committed
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.find(Employee.class, i);
            // verify age 
            int expected = NUMBER_TO_INSERT - i;
            int actual = e.getAge();
            if (expected != actual) {
                error("Failed update: for employee " + i
                        + " expected age " + expected
                        + " actual age " + actual);
            }
        }
        tx.commit();
    }
}
