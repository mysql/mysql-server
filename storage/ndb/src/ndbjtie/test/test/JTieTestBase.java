/*
 Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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
 * JTieTestBase.java
 */

package test;

import java.io.PrintWriter;

/**
 * Provides some common features for dervied JTie tests.
 */
public abstract class JTieTestBase {

    static protected final PrintWriter out = new PrintWriter(System.out, true);
    static protected final PrintWriter err = new PrintWriter(System.err, true);

    // ensure that asserts are enabled
    static {
        boolean assertsEnabled = false;
        assert assertsEnabled = true; // intentional side effect
        if (!assertsEnabled) {
            throw new RuntimeException("Asserts must be enabled for this test to be effective!");
        }
    }

    /**
     * Loads a dynamically linked library.
     */
    static protected void loadSystemLibrary(String name) {
        out.println("--> JTieTestBase.loadSystemLibrary(String)");

        final Class cls = JTieTestBase.class;
        out.println("    " + cls + " <" + cls.getClassLoader() + ">");
        out.println("    loading libary " + name + " ...");
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
        out.println("    ... loaded " + name);

        out.println("<-- JTieTestBase.loadSystemLibrary(String)");
    }

    public abstract void test();
}
