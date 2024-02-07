/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.
   Use is subject to license terms.

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
