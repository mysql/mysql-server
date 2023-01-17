/*
 Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.lang.annotation.Annotation;
import java.util.ArrayList;
import java.util.List;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import junit.framework.Test;
import junit.framework.TestResult;
import junit.framework.TestSuite;

/**
 * Run all JUnit tests in a jar file.
 */
public class AllTests {

    private static String jarFile = "";

    private static boolean enableDebugTests = false;

    private static void usage() {
        System.out.println("Usage: java -cp <jar file>:... AllTests <jar file> [--enable-debug-tests]");
        System.out.println("Will run all tests in the given jar file.");
        System.exit(2);
    }

    private static boolean isPossibleTestClass(String fileName) {
        return fileName.endsWith("Test.class");
    }

    private static boolean isAnnotationPresent(Class<?> candidate, String requiredAnnotation) {
        for (Annotation annotation: candidate.getAnnotations()) {
            if (annotation.toString().contains(requiredAnnotation)) {
                return true;
            }
        }
        return false;
    }

    private static boolean isIgnoreAnnotationPresent(Class<?> candidate) {
        return isAnnotationPresent(candidate, "Ignore");
    }

    private static boolean isTestClass(Class<?> klass) {
        return klass.getName().endsWith("Test")
            && !klass.getName().contains("Abstract")
            && Test.class.isAssignableFrom(klass);
    }

    private static boolean isDebugTestAnnotationPresent(Class<?> candidate)  {
        return isAnnotationPresent(candidate, "DebugTest");
    }

    private static boolean isTestDisabled(Class<?> klass) {
        return (isIgnoreAnnotationPresent(klass) || (!enableDebugTests && isDebugTestAnnotationPresent(klass)));
    }

    private static List<Class<?>> getClasses(File jarFile) throws IOException, ClassNotFoundException {
        List<Class<?>> classes = new ArrayList<Class<?>>();

        JarInputStream jarStream = new JarInputStream(new FileInputStream(jarFile));
        try {
            JarEntry jarEntry = jarStream.getNextJarEntry();
            while (jarEntry != null) {
                try {
                    String fileName = jarEntry.getName();
                    if (isPossibleTestClass(fileName)) {
                        String className = fileName.replaceAll("/", "\\.");
                        className = className.substring(0, className.length() - ".class".length());
                        // System.out.println("Found possible test class: '" + className + "'");
                        Class<?> testClass = Class.forName(className);
                        classes.add(testClass);
                    }
                } finally {
                    jarStream.closeEntry();
                }
                jarEntry = jarStream.getNextJarEntry();
            }
        } finally {
            jarStream.close();
        }

        return classes;
    }

    @SuppressWarnings("unchecked") // addTestSuite requires non-template Class argument
    public static Test suite() throws IllegalAccessException, IOException, ClassNotFoundException {
        TestSuite suite = new TestSuite("Cluster/J");

        if (jarFile.equals("")) {
            throw new IOException("Jar file to look for not given");
        }

        List<Class<?>> classes = getClasses(new File(jarFile));
        for (Class<?> klass : classes) {
            if (isTestClass(klass) && !isTestDisabled(klass)) {
                suite.addTestSuite((Class)klass);
            }
        }
        return suite;
    }

    /**
     * Usage: java -cp ... AllTests file.jar [--enable-debug-tests]
     *
     * --enable-debug-tests parameter additionally runs tests annotated
     * with @DebugTest, else these tests will be skipped.
     *
     * @param args the command line arguments
     */
    public static void main(String[] args) throws Exception {

        if (args.length < 0 || args.length > 2) {
            usage();
            return;
        }

        // First argument is the jarfile
        jarFile = args[0];

        // Optional debug-build argument
        if (args.length == 2) {
            if (args[1].equalsIgnoreCase("--enable-debug-tests")) {
                enableDebugTests = true;
            } else {
                usage();
                return;
            }
        }

        System.out.println("Running all tests in '" + jarFile + "'");
        TestSuite suite = (TestSuite) suite();
        System.out.println("Found '" + suite.testCount() + "' test classes in jar file.");
        TestResult res = junit.textui.TestRunner.run(suite);
        System.out.println("Finished running tests in '" + jarFile + "'");
        System.exit(res.wasSuccessful() ? 0 : 1);
    }
}

