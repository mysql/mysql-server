/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.DynamicObject;

import testsuite.clusterj.model.Employee;

public class LoadTest extends AbstractClusterJModelTest {

    private static final String tablename = "t_basic";

    private static final int NUMBER_TO_INSERT = 3;

    List<DynamicEmployee> loaded = new ArrayList<DynamicEmployee>();

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createEmployeeInstances(NUMBER_TO_INSERT);
        tx = session.currentTransaction();
        tx.begin();
        session.deletePersistentAll(DynamicEmployee.class);
        tx.commit();
    }

    public void test() {
        foundIllegalType();
        foundNull();
        create();
        findFound();
        findFoundAutocommit();
        load();
        loadAutocommit();
        loadNoFlush();
        loadNotFound();
        loadFindNoFlush();
        failOnError();
    }

    private void create() {
        // create instances to find
        tx = session.currentTransaction();
        int count = 0;
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            session.makePersistent(employees.get(i));
            ++count;
        }
    }

    private void load() {
        DynamicEmployee e;
        loaded.clear();
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            e = session.newInstance(DynamicEmployee.class, i);
            errorIfNotEqual("load after create new employee id mismatch", i, e.getId());
            errorIfNotEqual("load before load found mismatch", null, session.found(e));
            session.load(e);
            errorIfNotEqual("load after load newInstance employee id mismatch", i, e.getId());
            errorIfNotEqual("load after load found mismatch", null, session.found(e));
            loaded.add(e);
        }
        session.flush();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            e = loaded.get(i);
            errorIfNotEqual("load after flush found mismatch", true, session.found(e));
            // see if it is the right Employee
            errorIfNotEqual("load after flush employee id mismatch", i, e.getId());
            // make sure all fields were fetched
            consistencyCheckDynamicEmployee("load after flush", e);
        }
        tx.commit();
    }

    private void loadFindNoFlush() {
        DynamicEmployee e;
        loaded.clear();
        tx.begin();
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            e = session.newInstance(DynamicEmployee.class, i);
            errorIfNotEqual("loadFindNoFlush after newInstance employee id mismatch", i, e.getId());
            errorIfNotEqual("loadFindNoFlush after newInstance found mismatch", null, session.found(e));
            session.load(e);
            errorIfNotEqual("loadFindNoFlush after load employee id mismatch", i, e.getId());
            errorIfNotEqual("loadFindNoFlush after load found mismatch", null, session.found(e));
            loaded.add(e);
        }
        session.find(Employee.class, 0); // causes flush
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            e = loaded.get(i);
            errorIfNotEqual("loadFindNoFlush after find found mismatch", true, session.found(e));
            // see if it is the right Employee
            errorIfNotEqual("loadFindNoFlush after find employee id mismatch", i, e.getId());
            // make sure all fields were fetched
            consistencyCheckDynamicEmployee("loadFindNoFlush", e);
        }
        tx.commit();
    }

    private void consistencyCheckDynamicEmployee(String where, DynamicEmployee e) {
        int id = e.getId();
        String name = e.getName();
        errorIfNotEqual(where + " consistencyCheckDynamicEmployee name mismatch", "Employee number " + id, name);
        errorIfNotEqual(where + " consistencyCheckDynamicEmployee age mismatch", id, e.getAge());
        errorIfNotEqual(where + " consistencyCheckDynamicEmployee magic mismatch", id, e.getMagic());
    }

    private void loadNoFlush() {
        DynamicEmployee e;
        loaded.clear();
        tx.begin();
        e = session.newInstance(DynamicEmployee.class, 0);
        // default value is 0 for int
        errorIfNotEqual("loadNoFlush after newInstance employee id mismatch", 0, e.getId());
        // default value is null for String
        errorIfNotEqual("loadNoFlush after newInstance employee name mismatch", null, e.getName());
        session.load(e);
        errorIfNotEqual("loadNoFlush after load employee id mismatch", 0, e.getId());
        errorIfNotEqual("loadNoFlush after load employee name mismatch", null, e.getName());
        tx.commit();
    }

    /** A transaction must be in progress for load
     * 
     */
    private void loadAutocommit() {
        DynamicEmployee e = session.newInstance(DynamicEmployee.class, 0);
        try {
            session.load(e);
            error("loadAutocommit expected exception not thrown: a transaction must be in progress for load.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
    }

    /** Found should return false if row does not exist.
     * 
     */
    private void loadNotFound() {
        tx.begin();
        // Employee 1000 does not exist
        DynamicEmployee e = session.newInstance(DynamicEmployee.class, 10000);
        session.load(e);
        errorIfNotEqual("loadNotFound dynamic after load session.found(e)", null, session.found(e));
        errorIfNotEqual("loadNofFound dynamic after load mismatch in id", 10000, e.get(0));
        session.flush();
        errorIfNotEqual("loadNotFound dynamic after load, flush session.found(e)", false, session.found(e));
        errorIfNotEqual("loadNofFound dynamic after load mismatch in id", 10000, e.get(0));
        tx.commit();
        errorIfNotEqual("loadNotFound dynamic after load, flush, commit session.found(e)", false, session.found(e));
        errorIfNotEqual("loadNofFound dynamic after load, flush, commit mismatch in id", 10000, e.get(0));
    }

    /** Found should return true if row exists.
     * 
     */
    private void findFound() {
        tx.begin();
        // Employee 1000 does not exist
        DynamicEmployee e = session.find(DynamicEmployee.class, 0);
        errorIfNotEqual("findFound dynamic existing found mismatch", true, session.found(e));
        Employee emp = session.find(Employee.class, 0);
        errorIfNotEqual("findFound existing found mismatch", true, session.found(emp));
        tx.commit();
    }

    /** Found should return true if row exists.
     * 
     */
    private void findFoundAutocommit() {
        // Employee 0 exists
        DynamicEmployee e = session.find(DynamicEmployee.class, 0);
        errorIfNotEqual("findFoundAutocommit dynamic existing found mismatch", true, session.found(e));
        Employee emp = session.find(Employee.class, 0);
        errorIfNotEqual("findFoundAutocommit existing found mismatch", true, session.found(emp));
    }

    private void foundIllegalType() {
        try {
            // should throw ClusterJUserException
            session.found(Integer.valueOf(0));
            error("foundIllegalType expected exception not thrown: ClusterJUserException");
        } catch(ClusterJUserException ex) {
            // good catch
        }
    }

    private void foundNull() {
        errorIfNotEqual("foundNull found mismatch", null, session.found(null));
    }

    public static class DynamicEmployee extends DynamicObject {

        public DynamicEmployee() {}
        
        @Override
        public String table() {
            return tablename;
        }
        public int getId() {
            return (Integer)get(0);
        }
        public String getName() {
            return (String)get(1);
        }
        public int getAge() {
            return (Integer)get(2);
        }
        public int getMagic() {
            return (Integer)get(3);
        }
    }


}
