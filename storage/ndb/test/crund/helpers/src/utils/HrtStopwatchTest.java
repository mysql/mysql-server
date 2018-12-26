/*
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
 * HrtStopwatchTest.java
 */

package utils;

import java.io.PrintWriter;
import utils.HrtStopwatch;
import utils.HrtProfiler;

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
    
    static public void test0() 
    {
        out.println("--> HrtStopwatchTest.test0()");

        out.println("marking global time...");
        int g0 = HrtStopwatch.pushmark();
        do_something();

        out.println("marking global time...");
        int g1 = HrtStopwatch.pushmark();
        do_something();

        out.println("marking global time...");
        int g2 = HrtStopwatch.pushmark();

        assert (HrtStopwatch.top() == 2);

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
    
        out.println("popping timemarks");
        HrtStopwatch.popmark();
        assert (HrtStopwatch.top() == 1);
        HrtStopwatch.popmark();
        assert (HrtStopwatch.top() == 0);
        HrtStopwatch.popmark();
        assert (HrtStopwatch.top() == -1);

        out.println("<-- HrtStopwatchTest.test0()");
    }
    
    static public void test1() 
    {
        HrtProfiler.enter("test1");
        out.println("--> HrtStopwatchTest.test1()");
        do_something();
        test11();
        test11();
        out.println("<-- HrtStopwatchTest.test1()");
        HrtProfiler.leave("test1");
    }

    static public void test11() 
    {
        HrtProfiler.enter("test11");
        out.println("--> HrtStopwatchTest.test11()");
        do_something();
        test111();
        test111();
        out.println("<-- HrtStopwatchTest.test11()");
        HrtProfiler.leave("test11");
    }

    static public void test111() 
    {
        HrtProfiler.enter("test111");
        out.println("--> HrtStopwatchTest.test111()");
        do_something();
        out.println("<-- HrtStopwatchTest.test111()");
        HrtProfiler.leave("test111");
    }
    
    static public void main(String[] args) 
    {
        out.println("--> HrtStopwatchTest.main()");
        loadSystemLibrary("utils");
        
        out.println("init stopwatch...");
        HrtStopwatch.init(10);
        assert (HrtStopwatch.top() == -1);

        out.println();
        out.println("testing stopwatch...");
        test0();

        out.println();
        out.println("testing profiler...");
        test1();

        HrtProfiler.report();

        out.println("closing stopwatch...");
        HrtStopwatch.close();

        out.println("<-- HrtStopwatchTest.main()");
    }
}
