/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;

import testsuite.clusterj.model.AutoPKInt;

public class Reconnect2Test extends AbstractClusterJTest {

    @Override
    protected boolean getDebug() {
        return false;
    }

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        // delete all rows in AutoPKInt
        session.deletePersistentAll(AutoPKInt.class);
        // setup teardown
        addTearDownClasses(AutoPKInt.class);
        session.close();
    }

    private void millisleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public void testAutoIncrementInsert() {
        runQueryAndReconnect("runAutoIncrementInsert");
    }

    /**
     * runQueryAndReconnect loads the method to be tested, runs it via
     * CodeRunner and then in parallel initiates a reconnect.
     */
    public void runQueryAndReconnect(String methodName) {
        try {
            // Start running the code
            Method testMethod = this.getClass().getMethod(methodName,
                    int.class);
            Thread t = new Thread(new CodeRunner(this, testMethod));
            t.start();
            // Reconnect session factory
            sessionFactory.reconnect(1);
            // Now wait for testThread to complete
            t.join();
        } catch (SecurityException | NoSuchMethodException
                | InterruptedException ex) {
            throw new RuntimeException("Failed to load the test method.");
        }
        failOnError();
    }

    /**
     * CodeRunner This runnable class takes in a test method and executes it
     * continuously until it comes across a SessionFactory Reconnect. This is
     * used to verify that the particular queries executed by the test method
     * continues to run fine after a reconnect.
     */
    class CodeRunner implements Runnable {
        private Object testObject;
        private Method testMethod;

        public CodeRunner(Object object, Method method) {
            testObject = object;
            testMethod = method;
        }

        @Override
        public void run() {
            boolean reconnected = false;
            for (int i = 0; i < 1000; i++) {
                try {
                    // Invoke the method
                    testMethod.invoke(testObject, i);
                    if (reconnected) {
                        // The query succeeded after reconnection
                        return;
                    }
                } catch (InvocationTargetException ex) {
                    // An exception thrown by the test method
                    String exceptionMessage = ex.getCause().getMessage();
                    if (exceptionMessage
                            .contains("SessionFactory is not open.")) {
                        // Reconnect has started. Wait until it is over.
                        do {
                            millisleep(100);
                        } while (sessionFactory
                                .currentState() != SessionFactory.State.Open);
                        reconnected = true;
                    } else {
                        error(testMethod.getName() + ": caught "
                                + exceptionMessage);
                        return;
                    }
                } catch (IllegalAccessException | IllegalArgumentException ex) {
                    error("Failed to invoke the test method.");
                    return;
                }
            }
            error("Reconnect did not happen.");
        }
    }

    // Method to test insertion of a row in a table with Auto Increment PK
    public void runAutoIncrementInsert(int val) {
        Session session = sessionFactory.getSession();
        AutoPKInt instance = session.newInstance(AutoPKInt.class);
        instance.setVal(val);
        session.makePersistent(instance);
    }
}
