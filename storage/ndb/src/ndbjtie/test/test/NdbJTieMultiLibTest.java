/*
 Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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
 * NdbJTieMultiLibTest.java
 */

package test;

import myjapi.MyJapi;
import com.mysql.ndbjtie.ndbapi.NDBAPI;

/**
 * Tests the loading of the NdbJTie libary in presence of other native libs.
 */
public class NdbJTieMultiLibTest extends JTieTestBase {

    public void test() {
        out.println("--> NdbJTieMultiLibTest.test()");

        // load native library and class #1
        out.println();
        loadSystemLibrary("myjapi");
        out.println();
        out.println("    loaded: " + MyJapi.class);

        // load native library and class #2
        out.println();
        loadSystemLibrary("ndbclient");
        out.println();
        out.println("    loaded: " + NDBAPI.class);

        // load native library and class #3
        out.println();
        loadSystemLibrary("ndbjtie_unit_tests");
        out.println();
        out.println("    loaded: " + NDBAPI.class);

        out.println();
        out.println("<-- NdbJTieMultiLibTest.test()");
    };

    static public void main(String[] args) throws Exception {
        out.println("--> NdbJTieMultiLibTest.main()");

        out.println();
        NdbJTieMultiLibTest test = new NdbJTieMultiLibTest();
        test.test();

        out.println();
        out.println("<-- NdbJTieMultiLibTest.main()");
    }
}
