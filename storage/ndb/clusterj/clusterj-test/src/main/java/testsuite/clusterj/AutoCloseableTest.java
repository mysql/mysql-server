/*
   Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

import com.mysql.clusterj.Session;

import testsuite.clusterj.model.Employee;

public class AutoCloseableTest extends AbstractClusterJTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        session.close();
        addTearDownClasses(Employee.class);
    }

    public void test() {
        testFind();
        testGoodPersistCommit();
        testBadPersistIllegalNullFlush();
        testBadPersistFlushNoCommit();
        testBadPersistIllegalNullCommit();
        testBadPersistDuplicateKeyCommit();
        testBadPersistDuplicateKeyFlush();
        testBadPersistAutoCommit();
        testBadDeleteFlush();
        testBadDeleteCommit();
        testBadDeleteAutoCommit();
        failOnError();
    }

    protected void testFind() {
        runTest(new RunWithSession("testFind", null) {
            @Override
            public void run(Session s) {
                s.currentTransaction().begin();
                s.find(Employee.class, 10000);
                // no error
            }
        });
    }

    protected void testGoodPersistCommit() {
        runTest(new RunWithSession("testGoodPersistCommit", null) {
            @Override
            public void run(Session s) {
                s.currentTransaction().begin();
                Employee instance = s.newInstance(Employee.class, 170);
                instance.setMagic(170);
                s.persist(instance);
                s.currentTransaction().commit();
                // should persist instance
            }
            @Override
            public void check(Session s) {
                Employee e = s.find(Employee.class, 170);
                if (e == null) {
                    errorMessage = "failed to find Employee 170";
                }
                s.deletePersistent(Employee.class, 170);
            }
        });
    }

    protected void testBadPersistIllegalNullFlush() {
        runTest(new RunWithSession("testBadPersistIllegalNullFlush", "ClusterJDatastoreException") {
            @Override
            public void run(Session s) {
                s.currentTransaction().begin();
                Employee instance = s.newInstance(Employee.class, 171);
                s.persist(instance);
                s.flush();
                // error should not persist instance because magic is null
            }
            public void check(Session s) {
                Employee e = s.find(Employee.class, 171);
                if (e != null) {
                    errorMessage = "unexpected found Employee 171";
                }
            }
        });
    }

    protected void testBadPersistFlushNoCommit() {
        runTest(new RunWithSession("testBadPersistFlushNoCommit", null) {
            @Override
            public void run(Session s) {
                s.currentTransaction().begin();
                Employee instance = s.newInstance(Employee.class, 172);
                instance.setMagic(172);
                s.persist(instance);
                s.flush();
                // error should not persist instance because no commit
            }
            public void check(Session s) {
                Employee e = s.find(Employee.class, 172);
                if (e != null) {
                    errorMessage = "unexpected found Employee 172";
                }
            }
        });
    }

    protected void testBadPersistDuplicateKeyCommit() {
        runTest(new RunWithSession("testBadPersistDuplicateKeyCommit", "ClusterJDatastoreException") {
            @Override
            public void run(Session s) {
                s.currentTransaction().begin();
                Employee instance1 = s.newInstance(Employee.class, 173);
                instance1.setMagic(173);
                Employee instance2 = s.newInstance(Employee.class, 173);
                instance2.setMagic(173);
                s.persist(instance1);
                s.persist(instance2);
                s.currentTransaction().commit();
                // error should not persist instance because duplicate key
            }
            public void check(Session s) {
                Employee e = s.find(Employee.class, 173);
                if (e != null) {
                    errorMessage = "unexpected found Employee 173";
                }
            }
        });
    }

    protected void testBadPersistDuplicateKeyFlush() {
        runTest(new RunWithSession("testBadPersistDuplicateKeyFlush", "ClusterJDatastoreException") {
            @Override
            public void run(Session s) {
                s.currentTransaction().begin();
                Employee instance1 = s.newInstance(Employee.class, 174);
                instance1.setMagic(174);
                Employee instance2 = s.newInstance(Employee.class, 174);
                instance2.setMagic(174);
                s.persist(instance1);
                s.persist(instance2);
                s.flush();
                // error should not persist instance because duplicate key
            }
            public void check(Session s) {
                Employee e = s.find(Employee.class, 174);
                if (e != null) {
                    errorMessage = "unexpected found Employee 174";
                }
            }
        });
    }

    protected void testBadPersistIllegalNullCommit() {
        runTest(new RunWithSession("testBadPersistIllegalNullCommit", "ClusterJDatastoreException") {
            @Override
            public void run(Session s) {
                s.currentTransaction().begin();
                Employee instance = s.newInstance(Employee.class, 175);
                s.persist(instance);
                s.currentTransaction().commit();
                // error should not persist instance because magic is null
            }
            @Override
            public void check(Session s) {
                Employee e = s.find(Employee.class, 175);
                if (e != null) {
                    errorMessage = "unexpected found Employee 175";
                }
            }
        });
    }

    protected void testBadPersistAutoCommit() {
        runTest(new RunWithSession("testBadPersistAutoCommit", "ClusterJDatastoreException") {
            @Override
            public void run(Session s) {
                Employee instance = s.newInstance(Employee.class, 176);
                s.persist(instance);
                // error should not persist instance because magic is null
            }
            @Override
            public void check(Session s) {
                Employee e = s.find(Employee.class, 176);
                if (e != null) {
                    errorMessage = "unexpected found Employee 176";
                }
            }
        });
    }

    protected void testBadDeleteFlush() {
        // TODO: Need to investigate why flush does not cause ClusterJDatastoreException
        runTest(new RunWithSession("testBadDeleteFlush", null) {
            @Override
            public void run(Session s) {
                s.currentTransaction().begin();
                s.deletePersistent(Employee.class, 177);
                s.flush();
                // error should not delete instance
            }
        });
    }

    protected void testBadDeleteCommit() {
        runTest(new RunWithSession("testBadDeleteCommit", "ClusterJDatastoreException") {
            @Override
            public void run(Session s) {
                s.currentTransaction().begin();
                s.deletePersistent(Employee.class, 178);
                s.currentTransaction().commit();
                // error should not delete instance
            }
        });
    }

    protected void testBadDeleteAutoCommit() {
        runTest(new RunWithSession("testBadDeleteAutoCommit", "ClusterJDatastoreException") {
            @Override
            public void run(Session s) {
                s.deletePersistent(Employee.class, 179);
                // error should not delete instance
            }
        });
    }

    protected void checkException(Exception e, RunWithSession r) {
        if (r.expectedException == null) {
            error(r.where + " caught unexpected exception: " + e.getClass().getName() + " " + e.getMessage());
        }
        if (r.expectedException != null && !(e.getClass().getName().contains(r.expectedException))) {
            error(r.where + " caught wrong exception in run: " + e.getClass().getName() + " " + e.getMessage());
        }
    }
    protected void runTest(RunWithSession r) {
        try (Session s = sessionFactory.getSession()) {
            session = s;
            r.run(s);
            if (r.expectedException != null) {
                error(r.where + " failed to throw exception " + r.expectedException);
            }
        } catch (Exception e) {
            checkException(e, r);
        }
        if (!session.isClosed()) {
            error(r.where + " session was not closed in run");
        }
        try(Session s = sessionFactory.getSession()) {
            session = s;
            r.check(s);
            String m = r.errorMessage;
            if (m != null) {
                error(r.where + " " + m);
            }
        } catch (Exception e) {
            checkException(e, r);
        }
        if (!session.isClosed()) {
            error(r.where + " session was not closed in check");
        }
        try {
            session.close();
            session = null;
        } catch (Exception e) {
            error(r.where + " session.close threw exception: " + e.getClass().getName() + " " + e.getMessage());
        }
    }

    protected abstract class RunWithSession {
        RunWithSession(String where, String e) {this.where = where; expectedException = e;}
        String expectedException;
        String where;
        String errorMessage = null;
        void run(Session s) {}
        void check(Session s) {}
    }
}
