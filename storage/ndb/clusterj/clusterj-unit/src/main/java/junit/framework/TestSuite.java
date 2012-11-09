/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

import org.junit.Ignore;

public class TestSuite implements Test {
    public final String name;
    public final List<String> testClasses = new ArrayList<String>();
    public final List<TestCase> tests = new ArrayList<TestCase>();

    public Ignore ignoreTypeAnnotation = null;
    public String ignoreTypeReason = null;

    /** Create a new test suite; add tests later.
     * @param name the name of the test suite
     */
    public TestSuite(String name) {
        this.name = name;
    }

    /** Create a new test suite with a single test class.
     * @param cls the test class
     */
    @SuppressWarnings("unchecked") // addTestSuite((Class<? extends TestCase>) cls);
    public TestSuite(Class<?> cls) {
        this.name = cls.getName();
        if (TestCase.class.isAssignableFrom(cls)) {
            addTestSuite((Class<? extends TestCase>) cls);
        } else {
            throw new RuntimeException("TestSuite<init>: " + cls.getName());
        }
    }

    /** Add a test class to this suite. If the class is annotated with @Ignore,
     * skip running any test methods. If a method is annotated with @Ignore, 
     * skip running that test.
     * @param testClass the test class
     */
    public void addTestSuite(Class<? extends TestCase> testClass) {
        ignoreTypeAnnotation = testClass.getAnnotation(Ignore.class);
        ignoreTypeReason = ignoreTypeAnnotation == null? null: ignoreTypeAnnotation.value();
        testClasses.add(testClass.getName());
        final Method[] methods = testClass.getMethods();
        Ignore ignoreMethodAnnotation = null;
        String ignoreMethodReason = null;
        for (Method m : methods) {
            ignoreMethodAnnotation = m.getAnnotation(Ignore.class);
            ignoreMethodReason = ignoreMethodAnnotation == null? null: ignoreMethodAnnotation.value();
            // public void methods that begin with "test" and have no parameters are considered to be tests
            if (m.getName().startsWith("test")
                    && m.getParameterTypes().length == 0
                    && m.getReturnType().equals(Void.TYPE)
                    && Modifier.isPublic(m.getModifiers())) {
                try {
//                    System.out.println("TestSuite found " + m.getName());
                    if (ignoreTypeAnnotation != null || ignoreMethodAnnotation != null) {
                        System.out.println(m.getName() + 
                                " @Ignore: " + ignoreTypeReason + ":" + ignoreMethodReason);
                    } else {
                        TestCase t = testClass.newInstance();
                        t.name = testClass.getSimpleName() + "." + m.getName();
                        t.method = m;
                        tests.add(t);
                    }
                } catch (Exception ex) {
                    throw new RuntimeException(ex);
                }
            }
        }
    }

    public int testCount() {
        return tests.size();
    }

    public int countTestCases() {
        return tests.size();
    }

    /** Run all tests in this suite. For each test, call the run method.
     * @param result the result to receive the outcome of the test
     */
    public void run(TestResult result) {
//        System.out.println("--> TestSuite.run(TestResult)");
//        System.out.println("    test suite:   " + name);
//        System.out.println("    test classes: " + testClasses.size());
//        System.out.println("    test cases:   " + tests.size());
        for (TestCase test : tests) {
            test.run(result);
        }
//        System.out.println("<-- TestSuite.run(TestResult)");
    }

}
