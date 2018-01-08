/*
   Copyright 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

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

import java.util.ArrayList;
import java.util.List;
import testsuite.clusterj.model.Employee;

public class SaveTest extends AbstractClusterJModelTest {

    private static final int NUMBER_TO_INSERT = 4;
    
    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createEmployeeInstances(NUMBER_TO_INSERT);
        tx = session.currentTransaction();
        tx.begin();
        session.deletePersistentAll(Employee.class);
        tx.commit();
        tx.begin();
        List<Employee> insertedEmployees = new ArrayList<Employee>();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            if (i%2 == 0) {
                // only make even employees persistent now
                insertedEmployees.add(employees.get(i));
            }
        }
        session.makePersistentAll(insertedEmployees);
        tx.commit();
        addTearDownClasses(Employee.class);
    }

    public void testSave() {
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.find(Employee.class, i);
            if (e != null) {
                if (i%2 != 0) {
                    error("Employee " + i + " should not exist.");
                }
                // if exists, change age
                e.setAge(NUMBER_TO_INSERT - i);
            } else {
                // if not exist, insert with new age
                if (i%2 == 0) {
                    error("Employee " + i + " should exist.");
                } else {
                    e = employees.get(i);
                    e.setAge(NUMBER_TO_INSERT - i);
                }
            }
            // send the change to the database
            session.savePersistent(e);
        }
        tx.commit();

        // now verify that the changes were committed
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.find(Employee.class, i);
            if (e == null) {
                error("Failed save: employee " + i + " does not exist.");
            } else {
                // verify age
                int expected = NUMBER_TO_INSERT - i;
                int actual = e.getAge();
                if (expected != actual) {
                    error("Failed save: for employee " + i
                            + " expected age " + expected
                            + " actual age " + actual);
                }
            }
        }
        tx.commit();
        failOnError();
    }

    public void testSaveAll() {
        tx.begin();
        List<Employee> emps = new ArrayList<Employee>();
        List<Employee> expectedEmployees = new ArrayList<Employee>();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.find(Employee.class, i);
            if (e != null) {
                if (i%2 != 0) {
                    error("Employee " + i + " should not exist.");
                }
                // if exists, change age
                e.setAge(NUMBER_TO_INSERT - i);
            } else {
                // if not exist, insert with new age
                if (i%2 == 0) {
                    error("Employee " + i + " should exist.");
                } else {
                    e = employees.get(i);
                    e.setAge(NUMBER_TO_INSERT - i);
                }
            }
            emps.add(e);
            expectedEmployees.add(e);
        }
        // send the changes to the database
        List<Employee> savedEmployees = (List<Employee>)session.savePersistentAll(emps);
        if (savedEmployees.size() != NUMBER_TO_INSERT) {
            error("Wrong size for saved employees. Expected: " + NUMBER_TO_INSERT
                    + " actual: " + savedEmployees.size());
        }
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = expectedEmployees.get(i);
            // verify saved employees
            Employee saved = savedEmployees.get(i);
            if (saved != e) {
                error ("Failed saveAll: employee " + i + " did not match savedEmployees. "
                        + "Expected: " + e.toString() + " hashcode: " + e.hashCode()
                        + " actual: " + saved.toString() + " hashcode: " + saved.hashCode());
            }
        }
        tx.commit();

        // now verify that the changes were committed
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.find(Employee.class, i);
            if (e == null) {
                error("Failed saveAll: employee " + i + " does not exist.");
            } else {
                // verify age
                int expected = NUMBER_TO_INSERT - i;
                int actual = e.getAge();
                if (expected != actual) {
                    error("Failed saveAll: for employee " + i
                            + " expected age " + expected
                            + " actual age " + actual);
                }
            }
        }
        tx.commit();
        failOnError();
    }
}
