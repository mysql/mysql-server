/*
 * HrtStopwatchTest.java
 */

package utils;

import java.io.PrintWriter;
import utils.HrtStopwatch;

public class HrtStopwatchTest {

    static protected final PrintWriter out = new PrintWriter(System.out, true);

    static protected final PrintWriter err = new PrintWriter(System.err, true);

    /**
     * Loads a dynamically linked system library and reports any failures.
     */
    static protected void loadSystemLibrary(String name) {
        out.print("loading libary ...");
        out.flush();
        try {
            System.loadLibrary(name);
        } catch (UnsatisfiedLinkError e) {
            String path;
            try {
                path = System.getProperty("java.library.path");
            } catch (Exception ex) {
                path = "<exception caught: " + ex.getMessage() + ">";
            }
            err.println("failed loading library '"
                        + name + "'; java.library.path='" + path + "'");
            throw e;
        } catch (SecurityException e) {
            err.println("failed loading library '"
                        + name + "'; caught exception: " + e);
            throw e;
        }
        out.println("          [" + name + "]");
    }

    static public volatile long dummy;
    static public void do_something()
    {
        final long loop = 100000000L;
        long i;
        for (i = 0; i < loop; i++)
            dummy = i;
    }
    
    static public void main(String[] args) 
    {
        out.println("--> HrtStopwatchTest.main()");
        loadSystemLibrary("utils");
        
        out.println("init stopwatches...");
        HrtStopwatch.init(10);
    
        out.println("marking global time...");
        int g0 = HrtStopwatch.pushmark();
        do_something();

        out.println("marking global time...");
        int g1 = HrtStopwatch.pushmark();
        do_something();

        out.println("marking global time...");
        int g2 = HrtStopwatch.pushmark();

        out.println("amount of times:");
        double rt0 = HrtStopwatch.rtmicros(g1, g0);
        double rt1 = HrtStopwatch.rtmicros(g2, g1);
        double rt2 = HrtStopwatch.rtmicros(g2, g0);
        out.println("[t0..t1] real   = " + rt0 + " us");
        out.println("[t1..t2] real   = " + rt1 + " us");
        out.println("[t0..t2] real   = " + rt2 + " us");
        double ct0 = HrtStopwatch.ctmicros(g1, g0);
        double ct1 = HrtStopwatch.ctmicros(g2, g1);
        double ct2 = HrtStopwatch.ctmicros(g2, g0);
        out.println("[t0..t1] cpu    = " + ct0 + " us");
        out.println("[t1..t2] cpu    = " + ct1 + " us");
        out.println("[t0..t2] cpu    = " + ct2 + " us");
    
        out.println("closing stopwatches...");
        HrtStopwatch.close();

        out.println("<-- HrtStopwatchTest.main()");
    }
}
