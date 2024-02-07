/*
   Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BrokenBarrierException;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import com.mysql.clusterj.LockMode;
import com.mysql.clusterj.Query;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;

import testsuite.clusterj.model.Employee;

public class QueryScanLockTest extends AbstractQueryTest {

    private static final int numberOfTableScanNoLockThreads = 20;

    private static final long TWO_SECONDS = 2000L;

    private List<Thread> threadList = new ArrayList<Thread>();

    @Override
    public Class<?> getInstanceType() {
        return Employee.class;
    }

    @Override
    void createInstances(int number) {
        createEmployeeInstances(10); // this test needs exactly 10 instances
        instances.addAll(employees);
    }

    @Override
    protected boolean getCleanupAfterTest() {
        return true;
    }

    @Override
    public boolean getDebug() {
        return false;
    }

    CyclicBarrier barrier;

    /* Test setting the lock mode for a scan to see the effect on multiple threads.
     * Main thread:
     *   performs a scan with a specific lock mode, 
     *   starts thread 2 to find a row,
     *   starts multiple no-lock threads to give thread 2 a chance to get the row (or not),
     *   waits until all no-lock threads have completed,
     *   updates one row,
     *   commits and closes its session,
     *   joins all threads,
     *   exits.
     *   
     * Thread 2 
     *   performs a find with a specified lock, which may conflict with the main scan thread
     *   commits and closes its session,
     *   exits.
     *   
     * No-lock threads
     *   scans the table with no lock,
     *   commits and closes its session,
     *   exits.
     */
    public void test() {
        // read thread will read the original value because there are no locks
        scanVersusFind(true, LockMode.READ_COMMITTED, LockMode.READ_COMMITTED, 0, 0);
        // read thread will read the original value because there are no locks on the read thread
        scanVersusFind(true, LockMode.EXCLUSIVE, LockMode.READ_COMMITTED, 1, 1);
        // read thread will read the updated value because the locks conflict
        scanVersusFind(true, LockMode.EXCLUSIVE, LockMode.EXCLUSIVE, 2, 12);
        // read thread will read the updated value because the locks conflict
        scanVersusFind(true, LockMode.EXCLUSIVE, LockMode.SHARED, 3, 13);
        // read thread will read the original value because there are no locks on the main thread
        scanVersusFind(true, LockMode.READ_COMMITTED, LockMode.EXCLUSIVE, 4, 4);

        // read thread will read the original value because there are no locks
        scanVersusFind(false, LockMode.READ_COMMITTED, LockMode.READ_COMMITTED, 5, 5);
        // read thread will read the original value because there are no locks on the read thread
        scanVersusFind(false, LockMode.EXCLUSIVE, LockMode.READ_COMMITTED, 6, 6);
        // read thread will read the updated value because the locks conflict
        scanVersusFind(false, LockMode.EXCLUSIVE, LockMode.EXCLUSIVE, 7, 17);
        // read thread will read the updated value because the locks conflict
        scanVersusFind(false, LockMode.EXCLUSIVE, LockMode.SHARED, 8, 18);
        // read thread will read the original value because there are no locks on the main thread
        scanVersusFind(false, LockMode.READ_COMMITTED, LockMode.EXCLUSIVE, 9, 9);
        
        failOnError();
    }

    private void scanVersusFind(boolean tableScan, LockMode scanLockMode, LockMode findLockMode, int id, int expectedAge) {
        barrier = new CyclicBarrier(numberOfTableScanNoLockThreads + 1, new Runnable() {
            public void run() {
                if (getDebug()) System.out.println("barrier completed.");
            }
        });

        String mode = tableScan?"table":"index";
        if (getDebug()) {
            System.out.println(mode + " scan using scanLockMode: " + scanLockMode +
                    "; findLockMode: " + findLockMode);
            System.out.flush();
        }
        Session session = sessionFactory.getSession();
        session.currentTransaction().begin();

        Employee[] employees = null;
        // scan in the main thread
        if (tableScan) {            
            employees = scanTable(scanLockMode, session);
        } else {
            employees = scanIndex(scanLockMode, session);
        }
        // we now have all employees locked with scanLockMode

        MyUncaughtExceptionHandler uncaughtExceptionHandler = new MyUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(uncaughtExceptionHandler);
        ThreadGroup threadGroup = Thread.currentThread().getThreadGroup();

        // start a thread to find one instance in a new session
        Find find = new Find(findLockMode, sessionFactory.getSession(), id, expectedAge);
        Thread thread1 = new Thread(threadGroup, find);
        threadList.add(thread1);
        if (getDebug()) System.out.println(mode + " main starting find thread.");
        thread1.start();

        for (int i = 0; i < numberOfTableScanNoLockThreads; ++i) {
            Runnable noLockRead = new TableScanNoLock(sessionFactory.getSession());
            Thread thread = new Thread(threadGroup, noLockRead);
            threadList.add(thread);
            if (getDebug()) System.out.println(mode + " main starting no lock thread " + i);
            thread.start();
        }
        // wait for no lock threads to finish
        await(barrier);

        // update one of the employees in the main thread
        Employee employee = employees[id];
        errorIfNotEqual(mode + " found the wrong employee ", id, employee.getId());
        employee.setAge(id + 10);
        try {
            session.updatePersistent(employee);
        } catch (Exception ex) {
            System.out.println(mode + " scan updatePersistent(" + id + ") with lockmode " + scanLockMode + " got " + ex);
        }
        // commit the update, allowing the reader thread to access the locked row
        if (getDebug()) System.out.println("Main committing update.");
        try {
            session.currentTransaction().commit();
        } catch (Exception ex) {
            System.out.println(mode + " scan commit got " + ex);
        }
        session.close();

        if (getDebug()) System.out.println("Main joining threads.\n");
        // join all threads
        try {
            for (Thread thread: threadList) {
                thread.join(TWO_SECONDS); // wait two seconds for the last one
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
            throw new RuntimeException("Interrupted while joining threads.");
        }
    }

    private void await(CyclicBarrier barrier) {
        try {
            if (getDebug()) System.out.println("Awaiting...");
            barrier.await(15L, TimeUnit.SECONDS);
        } catch (InterruptedException e1) {
            error("await Interrupted!");
            e1.printStackTrace();
        } catch (BrokenBarrierException e1) {
            error("await BrokenBarrier!");
            e1.printStackTrace();
        } catch (TimeoutException e) {
            error("await Timeout!");
            e.printStackTrace();
        }
    }

    private class TableScanNoLock implements Runnable {
        private final Session mySession;
        private TableScanNoLock(Session session) {
            this.mySession = session;
        }
        public void run() {
            scanTable(LockMode.READ_COMMITTED, mySession);
            if (getDebug()) System.out.println("TableScanNoLock thread DONE!");
            try {
                barrier.await();
            } catch (InterruptedException e) {
                error("await Interrupted!");
                e.printStackTrace();
            } catch (BrokenBarrierException e) {
                error("await BrokenBarrier!");
                e.printStackTrace();
            }
            mySession.close();
        }
    }

    private class Find implements Runnable {
        private final Session mySession;
        private final LockMode lockMode;
        private final int expectedAge;
        private final int id;
        private Find(LockMode lockMode, Session session, final int id, final int expectedAge) {
            this.mySession = session;
            this.lockMode = lockMode;
            this.expectedAge = expectedAge;
            this.id = id;
        }

        public void run() {
            mySession.currentTransaction().begin();
            mySession.setLockMode(lockMode);
            if (getDebug()) System.out.println("Find thread performing find with lockMode " + lockMode);
            Employee employee = null;
            try {
                employee = mySession.find(Employee.class, id);
            } catch (Exception ex) {
                System.out.println("Find(" + id + ") with lockmode " + lockMode + " got " + ex);
            }
            if (getDebug()) System.out.println("Find thread found employee " + id);
            int age = employee.getAge();
            if (age != expectedAge) {
                System.out.println("Find thread found employee with wrong age; expected: " + expectedAge +
                        "; actual: " + age);
            }
            errorIfNotEqual("Find thread found employee with wrong age", expectedAge, age);
            if (getDebug()) System.out.println("Find thread performing commit.");
            mySession.currentTransaction().commit();
            mySession.close();
            if (getDebug()) System.out.println("Find thread DONE!");
        }
        
    }

    private Employee[] scanTable(LockMode scanLockMode, Session session) {
        session.setLockMode(scanLockMode);
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<Employee> qd = builder.createQueryDefinition(Employee.class);
        Query<Employee> q = session.createQuery(qd);
        List<Employee> results = q.getResultList();
        Employee[] employees = new Employee[results.size()];
        for (Employee e: results) {
            int id = e.getId();
            employees[id] = e;
        }
        return employees;
    }
    
    private Employee[] scanIndex(LockMode scanLockMode, Session session) {
        session.setLockMode(scanLockMode);
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<Employee> qd = builder.createQueryDefinition(Employee.class);
        qd.get("age").between(qd.param("low"), qd.param("high"));
        Query<Employee> q = session.createQuery(qd);
        q.setParameter("low", 0);
        q.setParameter("high", 9);
        List<Employee> results = q.getResultList();
        Employee[] employees = new Employee[results.size()];
        for (Employee e: results) {
            int id = e.getId();
            employees[id] = e;
        }
        return employees;
    }

}
