package testsuite.clusterj;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestResult;
import junit.framework.TestSuite;

/**
 * Run all JUnit tests in a jar file.
 */
public class AllTests {

    private static String jarFile = "";

    private static void usage() {
        System.out.println("Usage: java -cp <jar file>:... AllTests <jar file>");
        System.out.println("Will run all tests in the given jar file.");
        System.exit(2);
    }

    private static boolean isPossibleTestClass(String fileName) {
        return fileName.endsWith("Test.class");
    }

    private static boolean isTestClass(Class klass) {
        // TODO: Could check if klass inherits junit.framework.Test in some way,
        //       but this will also be cought in the cast later
        return klass.getName().endsWith("Test")
            && !klass.getName().contains("Abstract");
    }

    private static List<Class> getClasses(File jarFile) throws IOException, ClassNotFoundException {
        List<Class> classes = new ArrayList<Class>();

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
                        Class testClass = Class.forName(className);
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

    public static Test suite() throws IllegalAccessException, IOException, ClassNotFoundException {
        TestSuite suite = new TestSuite("Cluster/J");

        if (jarFile.equals("")) {
            throw new IOException("Jar file to look for not given");
        }

        List<Class> classes = getClasses(new File(jarFile));
        for (Class klass : classes) {
	    if (isTestClass(klass)) {
		suite.addTestSuite(klass);
	    }
        }
        return suite;
    }

    /**
     * Usage: java -cp ... AllTests file.jar
     *
     * @param args the command line arguments
     */
    public static void main(String[] args) throws Exception {
        if (args.length == 1) {
            jarFile = args[0];
            System.out.println("Running all tests in '" + jarFile + "'");
	    TestSuite suite = (TestSuite) suite();
            System.out.println("Found '" + suite.testCount() + "' tests in jar file.");
            TestResult res = junit.textui.TestRunner.run(suite);
            System.exit(res.wasSuccessful() ? 0 : 1);
        } else {
            usage();
        }
    }
}
