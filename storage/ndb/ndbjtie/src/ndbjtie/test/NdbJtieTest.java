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
 * NdbJtieTest.java
 */

package test;

import java.io.PrintWriter;

//import java.math.BigInteger;
//import java.math.BigDecimal;
//import java.nio.ByteBuffer;
//import java.nio.ByteOrder;
//import java.nio.CharBuffer;
//import java.nio.ShortBuffer;
//import java.nio.IntBuffer;
//import java.nio.LongBuffer;
//import java.nio.FloatBuffer;
//import java.nio.DoubleBuffer;

import ndbjtie.Ndb_cluster_connection;
import ndbjtie.Ndb;
import ndbjtie.NdbError;

public class NdbJtieTest {

    static protected final PrintWriter out = new PrintWriter(System.out, true);

    static protected final PrintWriter err = new PrintWriter(System.err, true);

    private String mgmdConnect = "localhost";
    private String catalog = "crunddb";
    private String schema = "def";

    private Ndb_cluster_connection mgmd;
    private Ndb ndb;

    /**
     * Loads a dynamically linked system library and reports any failures.
     */
    static protected void loadSystemLibrary(String name) {
        out.println("loading libary...");
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
        out.println("... [" + name + "]");
    }

    protected void init() {
        // load native library (better diagnostics doing it explicitely)
        loadSystemLibrary("ndbjtie");

        // instantiate NDB cluster singleton
        out.println();
        out.println("creating cluster conn...");

        mgmd = Ndb_cluster_connection.create(mgmdConnect);
        //assert mgmd != null;
        out.println("... [ok, mgmd=" + mgmd + "]");

        // connect to cluster management node (ndb_mgmd)
        out.println();
        out.println("connecting to mgmd ...");
        final int retries = 0;        // retries (< 0 = indefinitely)
        final int delay = 0;          // seconds to wait after retry
        final int verbose = 1;        // print report of progess
        // 0 = success, 1 = recoverable error, -1 = non-recoverable error
        //if (Ndb_cluster_connection.connect(mgmd, retries, delay, verbose) != 0) {
        if (mgmd.connect(retries, delay, verbose) != 0) {
            final String msg = ("mgmd@" + mgmdConnect
                                + " was not ready within "
                                + (retries * delay) + "s.");
            out.println(msg);
            throw new RuntimeException(msg);
        }
        out.println("... [ok: " + mgmdConnect + "]");
    }

    protected void close() {
        out.println();
        out.println("closing mgmd conn ...");
        if (mgmd != null)
            Ndb_cluster_connection.delete(mgmd);
        out.println("... [ok, mgmd=" + mgmd + "]");
        mgmd = null;

/*
    cout << "closing NDBAPI ...   " << flush;
    // ndb_close must be called last
    ndb_end(0);
    cout << "       [ok]" << endl;
*/
    }

    void
    initConnection(String catalog, String schema) {
        // optionally, connect and wait for reaching the data nodes (ndbds)
        out.println();
        out.println("waiting until ready...");
        final int initial_wait = 10; // seconds to wait until first node detected
        final int final_wait = 0;    // seconds to wait after first node detected
        // returns: 0 all nodes live, > 0 at least one node live, < 0 error
        if (mgmd.wait_until_ready(initial_wait, final_wait) < 0) {
            final String msg = ("data nodes were not ready within "
                                + (initial_wait + final_wait) + "s.");
            out.println(msg);
            throw new RuntimeException(msg);
        }
        out.println("... [ok]");

        // connect to database
        out.println();
        out.println("connecting to database...");
        ndb = Ndb.create(mgmd, catalog, schema);
        final int max_no_tx = 10; // maximum number of parallel tx (<=1024)
        // note each scan or index scan operation uses one extra transaction
        if (ndb.init(max_no_tx) != 0) {
            String msg = "Error caught: " + ndb.getNdbError().message();
            throw new RuntimeException(msg);
        }
        out.println("... [ok]");
    }

    void
    closeConnection() {
        out.println();
        out.println("closing database conn ...");
        Ndb.delete(ndb);
        ndb = null;
        out.println("... [ok]");
    }

    public void test() {
        out.println("--> NdbJtieTest.test()");

        init();
        initConnection(catalog, schema);
        closeConnection();
        close();

        out.println();
        out.println("<-- NdbJtieTest.test()");
    };
    
    static public void main(String[] args) throws Exception {
        out.println("--> NdbJtieTest.main()");

        out.println();
        NdbJtieTest test = new NdbJtieTest();
        test.test();
        
        out.println();
        out.println("<-- NdbJtieTest.main()");
    }
}
