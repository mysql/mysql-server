/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.lang.annotation.Annotation;
import java.util.ArrayList;
import java.util.HashSet;
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

    private static void usage(String message) {
        System.out.println(message);
        System.out.println();
        System.out.println("Usage: java -cp <jar file>:... AllTests <jar file> [options]");
        System.out.println("Will run all tests in the given jar file.");
        System.out.println("  Options: ");
        System.out.println("     --print-cases / -l   : List test cases");
        System.out.println("     --enable-debug-tests : Run extra debug-only tests");
        System.out.println("     -n TestName [...]    : Run named tests");
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
            && !klass.getName().contains("ClearSmokeTest")
            && Test.class.isAssignableFrom(klass)
            && ! Test.class.equals(klass);
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
    public static TestSuite suite(HashSet<String> namedTests) throws IllegalAccessException, IOException, ClassNotFoundException {
        TestSuite suite = new TestSuite("Cluster/J");

        if (jarFile.equals("")) {
            throw new IOException("Jar file to look for not given");
        }

        List<Class<?>> classes = getClasses(new File(jarFile));
        for (Class<?> klass : classes) {
            if (isTestClass(klass)) {
                if(! namedTests.isEmpty()) {
                    String longName = klass.getName();
                    String shortName = longName.substring(longName.lastIndexOf(".") + 1);
                    if(namedTests.contains(shortName) || namedTests.contains(longName))
                        suite.addTestSuite((Class)klass);
                } else if(! isTestDisabled(klass)) {
                    suite.addTestSuite((Class)klass);
                }
            }
        }

        /* The ClearSmokeTest closes the SessionFactory and can perform any
           other final cleanup for the test suite.
        */
        if(suite.testCount() > 0)
            suite.addTestSuite(ClearSmokeTest.class);

        return suite;
    }

    public static void printCases() throws IOException, ClassNotFoundException {
        List<Class<?>> classes = getClasses(new File(jarFile));
        for (Class<?> cls : classes) {
            if (isTestClass(cls)) {
                String note = "";
                for(Annotation a : cls.getDeclaredAnnotations())
                    note = note.concat(a.toString()).concat(" ");
                String name = cls.getName();
                String shortName = name.substring(name.lastIndexOf(".") + 1);
                System.out.printf("  %-36s  %s\n", shortName, note);
            }
        }
    }


    /**
     * Usage: java -cp ... AllTests file.jar [-l] [--print-cases]
     *                                       [--enable-debug-tests]
     *                                       [-n test ...]
     *
     * --enable-debug-tests parameter additionally runs tests annotated
     * with @DebugTest, else these tests will be skipped.
     *
     * @param args the command line arguments
     */
    public static void main(String[] args) throws Exception {

        if (args.length < 1)
            usage("JAR file required");

        // First argument is the jarfile
        jarFile = args[0];

        boolean runNamedTests = false;
        boolean printTestList = false;
        HashSet<String> namedTests = new HashSet<String>();

        for(int i = 1 ; i < args.length ; i++) {
            if (args[i].equals("-l") || args[i].equals("--print-cases"))
              printTestList = true;

            else if (args[i].equalsIgnoreCase("--enable-debug-tests"))
                enableDebugTests = true;

            else if(runNamedTests)
              namedTests.add(args[i]);

            else if (args[i].equals("-n"))
              runNamedTests = true;

            else
              usage("Unrecognized option");
        }

        if(printTestList && runNamedTests)
            usage("Incompatible options");

        if(printTestList) {
            printCases();
            System.exit(0);
        }

        TestSuite suite = suite(namedTests);

        System.out.println("Running " + (runNamedTests ? "named" : "all") +
                           " tests in '" + jarFile + "'");
        System.out.println("Found " + suite.testCount() + " test classes.");
        TestResult res = junit.textui.TestRunner.run(suite);
        System.out.println("Finished running tests in '" + jarFile + "'");
        System.exit(res.wasSuccessful() ? 0 : 1);
    }
}
