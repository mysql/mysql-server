/*
 Copyright (C) 2009 Sun Microsystems, Inc.
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
 * DemojTest.java
 */

package test;

import java.io.PrintWriter;

import demoj.A;

public class DemojTest {

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


    static public void test() {
        out.println("--> DemojTest.test()");

        out.println("\ncalling A.simple()");
        final double d = A.simple(2.0);
        out.println("... d = " + d);
        assert (d == 2.0);
    
        out.println("\ncalling A.print(null)...");
        A.print(null);

        out.println("\ncalling A.print(\"...\")...");
        A.print("this is a Java ascii string literal");

        out.println("\ncalling A.getA()...");
        A a = A.getA();
        out.println("... a = " + a);

        out.println("\ncalling a.print()...");
        a.print();

        out.println();
        out.println("<-- DemojTest.test()");
    };

    static public void main(String[] args) 
    {
        out.println("--> DemojTest.main()");

        out.println();
        loadSystemLibrary("demoj");

        out.println();
        test();

        out.println();
        out.println("<-- DemojTest.main()");
    }
}
