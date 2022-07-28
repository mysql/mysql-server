/*
 Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

package regression;

import java.util.Properties;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import junit.framework.TestCase;

import testsuite.clusterj.model.Employee;

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;

public class Bug54619 extends TestCase {

    public void test() {
        main(null);
    }

    public static void main(String[] args) {

        /* http://bugs.mysql.com/bug.php?id=54619
         * 
         * MySQL Cluster generic Linux and Windows 7.1.3
         * 
         * Session factory in a different thread than sessions crashes JVM.
         * 
         * If SessionFactory is initialized in a different thread than where
         * sessions are created, session.newInstance call will crash JVM. This
         * happens on Windows and Linux. In Windows the crash seems to be coming
         * from DomainFieldHandlerImpl and from line table.getColumn(columnName);.
         * 
         * Workaround is to insert a dummy row (most likely any db access works)
         * from same thread where the SessionFactory is created. After this the
         * session seem to be working from other thread.
         */
        ExecutorService pool = Executors.newFixedThreadPool(4);
        // 1. Manager initializes session factory
        Manager.createClusterJSession();
        // 2. Remove the comment to get this to work.
        //insert(1); 

        // 3. Insert rows from different threads.
        for (int i = 0; i < 4; ++i) {
            final int id = i;
            pool.submit(new Runnable() {
                
                public void run() {
                    Session session = Manager.getSessionFactory().getSession();
                    Employee entity = session.newInstance(Employee.class); // crash here?
                    entity.setId(id);
                    entity.setAge(id);
                    entity.setMagic(id);
                    entity.setName("Employee " + id);
                    session.currentTransaction().begin();
                    session.persist(entity);
                    session.currentTransaction().commit();
                    return;
                }
            });
        }
        // wait for all threads to complete
        pool.shutdown();
        try {
            pool.awaitTermination(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            fail("Interrupted pool.awaitTermination");
        }
        pool.shutdownNow();
    }

    private static void insert(int number) {
        Session session = Manager.getSessionFactory().getSession();
        Employee entity = session.newInstance(Employee.class);
        entity.setId(number);
        session.currentTransaction().begin();
        session.persist(entity);
        session.currentTransaction().commit();

        return;
    }

}

class Manager {

    static SessionFactory sessionfactory;

    Manager() {
        createClusterJSession();
        return;
    }

    static void createClusterJSession() {
        Properties properties = new Properties();
        properties.put("com.mysql.clusterj.connectstring", "localhost:1186");
        properties.put("com.mysql.clusterj.database", "test");
        sessionfactory = ClusterJHelper.getSessionFactory(properties);

        return;
    }

    public static SessionFactory getSessionFactory() {
        return sessionfactory;
    }
}
