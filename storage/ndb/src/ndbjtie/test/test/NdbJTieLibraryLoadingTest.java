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
 * NdbJTieLibraryLoadingTest.java
 */

package test;

import myjapi.MyJapi;
import com.mysql.ndbjtie.ndbapi.NDBAPI;

/**
 * Tests the loading of the NdbJTie libary in presence of other native libs.
 */
public class NdbJTieLibraryLoadingTest extends JTieTestBase {

    public void test() {
        out.println("--> NdbJTieLibraryLoadingTest.test()");

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

        out.println();
        out.println("<-- NdbJTieLibraryLoadingTest.test()");
    };
    
    static public void main(String[] args) throws Exception {
        out.println("--> NdbJTieLibraryLoadingTest.main()");

        out.println();
        NdbJTieLibraryLoadingTest test = new NdbJTieLibraryLoadingTest();
        test.test();
        
        out.println();
        out.println("<-- NdbJTieLibraryLoadingTest.main()");
    }
}
