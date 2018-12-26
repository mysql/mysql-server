/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

public class UpdateTest extends AbstractClusterJModelTest {

    private static final int NUMBER_TO_INSERT = 4;
    
    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();
        tx.begin();
        session.deletePersistentAll(Employee.class);
        tx.commit();
        createEmployeeInstances(NUMBER_TO_INSERT);
        tx.begin();
        session.makePersistentAll(employees);
        tx.commit();
        addTearDownClasses(Employee.class);
    }

    public void testUpdate() {
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
        failOnError();
    }

    public void testBlindUpdate() {
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.newInstance(Employee.class);
            // set primary key (required for blind update)
            e.setId(i);
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
        failOnError();
    }

    public void testUpdateAll() {
        List<Employee> employees = new ArrayList<Employee>();
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.find(Employee.class, i);
            // change age 
            e.setAge(NUMBER_TO_INSERT - i);
            employees.add(e);
        }
        // send the changes to the database
        session.updatePersistentAll(employees);
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
        failOnError();
    }

    public void testBlindUpdateAll() {
        List<Employee> employees = new ArrayList<Employee>();
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.newInstance(Employee.class);
            // set primary key (required for blind update)
            e.setId(i);
            // change age 
            e.setAge(NUMBER_TO_INSERT - i);
            employees.add(e);
        }
        // send the changes to the database
        session.updatePersistentAll(employees);
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
        failOnError();
    }

    public void testUpdateAllAutocommit() {
        List<Employee> employees = new ArrayList<Employee>();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.find(Employee.class, i);
            // change age 
            e.setAge(NUMBER_TO_INSERT - i);
            employees.add(e);
        }
        // send the changes to the database in a single autocommit transaction
        session.updatePersistentAll(employees);
        
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
        failOnError();
    }

    public void testBlindUpdateAllAutocommit() {
        List<Employee> employees = new ArrayList<Employee>();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            Employee e = session.newInstance(Employee.class);
            // set primary key (required for blind update)
            e.setId(i);
            // change age 
            e.setAge(NUMBER_TO_INSERT - i);
            employees.add(e);
        }
        // send the changes to the database in a single autocommit transaction
        session.updatePersistentAll(employees);
        
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
        failOnError();
    }

}
