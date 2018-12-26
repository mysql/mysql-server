/*
   Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

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

/*
 * This assortment of classes is a mock http://en.wikipedia.org/wiki/Mock_object
 * implementation of junit http://en.wikipedia.org/wiki/Junit. It contains annotations,
 * classes, and interfaces that mock junit for use with test classes 
 * that use a subset of junit functionality. 
 * <p>
 * In clusterj, test classes can use either the real junit or this mock junit.
 * The mock can be used stand-alone or invoked by the maven surefire junit plugin.
 * Other test runners and harnesses might not have been tested and might not work.
 * <p>
 * There is no code copied from Junit itself. Only concepts and names of
 * annotations, interfaces, classes, and methods are copied, which must exactly match
 * the corresponding items from junit in order to be mocked.
 */

package junit.framework;

import java.util.List;
import java.util.ArrayList;

/** This class maintains the results of running a series of tests. It is the primary
 * way for a test class to return results of tests.
 */
public class TestResult {
    public final List<String> successes = new ArrayList<String>();
    public final List<String> failures = new ArrayList<String>();
    public final List<Throwable> throwables = new ArrayList<Throwable>();
    public TestListener listener = newListener();

    public void addListener(TestListener listener) {
        this.listener = listener;
    }

    public synchronized boolean wasSuccessful() {
        return failures.isEmpty() && throwables.isEmpty();
    }

    public TestListener newListener() {
        // This is a dummy listener in case the test runner doesn't set a listener
        return new TestListener() {

            public void addError(Test test, Throwable t) {
            }

            public void addFailure(Test test, AssertionFailedError t) {
            }

            public void endTest(Test test) {
            }

            public void startTest(Test test) {
            }
            
        };
    }
}
