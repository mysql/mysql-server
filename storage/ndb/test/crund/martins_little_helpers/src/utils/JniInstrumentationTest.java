/*
 Copyright 2010 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

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
 * JniInstrumentationTest.java
 */

package utils;

import java.io.PrintWriter;
import utils.HrtStopwatch;

import java.lang.management.ThreadMXBean;
import java.lang.management.ManagementFactory;

public class JniInstrumentationTest {

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

    static public native void aNativeMethod();

    static public volatile long dummy;
    static public void do_something(long count)
    {
        long i;
        for (i = 0; i < count; i++)
            dummy = i;
    }
    

    static public void main(String[] args) 
    {
        out.println("--> JniInstrumentationTest.main()");

        ThreadMXBean tmxb = ManagementFactory.getThreadMXBean();        
        assert (tmxb.isCurrentThreadCpuTimeSupported());
        long t0 = tmxb.getCurrentThreadCpuTime();

        out.println();
        loadSystemLibrary("utils");
        loadSystemLibrary("jnitest");

        out.println();
        out.println("init libutils stopwatch...");
        HrtStopwatch.init(10);

        out.println();
        out.println("marking time Java ...");
        int g0 = HrtStopwatch.pushmark();

        aNativeMethod();
    
        int g3 = HrtStopwatch.pushmark();
        out.println("... marked time Java");

        out.println();
        out.println("amount of times Java:");
        double rt0 = HrtStopwatch.rtmicros(g3, g0);
        out.println("[g" + g0 + "..g" + g3 + "] real   = " + rt0 + " us");
        double rt1 = HrtStopwatch.rtmicros(2, 1);
        out.println("[g2..g1] real   = " + rt1 + " us");
        double ct0 = HrtStopwatch.ctmicros(g3, g0);
        out.println("[g" + g0 + "..g" + g3 + "] cpu    = " + ct0 + " us");
        double ct1 = HrtStopwatch.ctmicros(2, 1);
        out.println("[g2..g1] cpu    = " + ct1 + " us");

        do_something(10000000);
        
        long t1 = tmxb.getCurrentThreadCpuTime();
        out.println();
        out.println("amount of times TMB:");
        out.println("[t0..t1] cpu    = " + ((t1 - t0)/1000.0) + " us");

        out.println();
        out.println("closing libutils stopwatch...");
        HrtStopwatch.close();

        out.println("<-- JniInstrumentationTest.main()");
    }
}
