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
 * NdbJTieSmokeTest.java
 */

package test;

import com.mysql.ndbjtie.ndbapi.Ndb_cluster_connection;
import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.NdbError;

/**
 * Tests the basic functioning of the NdbJTie libary: mgmapi for starting
 * a cluster (not implemented yet) and ndbapi for connecting to a cluster.
 * XXX test requires a running cluster at this time, which is why we're
 * not running this test as a unit but mtr test.
 */
public class NdbJTieSmokeTest extends JTieTestBase {

    private String mgmdConnect = "localhost";
    private String catalog = "crunddb";
    private String schema = "def";

    private Ndb_cluster_connection mgmd;
    private Ndb ndb;

    protected void init() {
        // load native library
        loadSystemLibrary("ndbclient");

        // Get system variable for other connect string
        mgmdConnect = System.getProperty("jtie.unit.ndb.connectstring", mgmdConnect);

        // instantiate NDB cluster singleton
        out.println();
        out.println("creating cluster conn...");
        mgmd = Ndb_cluster_connection.create(mgmdConnect);
        assert mgmd != null;
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

    protected void initConnection(String catalog, String schema) {
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

    protected void closeConnection() {
        out.println();
        out.println("closing database conn ...");
        Ndb.delete(ndb);
        ndb = null;
        out.println("... [ok]");
    }

    public void test() {
        out.println("--> NdbJTieSmokeTest.test()");

        init();
        initConnection(catalog, schema);
        closeConnection();
        close();

        out.println();
        out.println("<-- NdbJTieSmokeTest.test()");
    };
    
    static public void main(String[] args) throws Exception {
        out.println("--> NdbJTieSmokeTest.main()");

        out.println();
        NdbJTieSmokeTest test = new NdbJTieSmokeTest();
        test.test();
        
        out.println();
        out.println("<-- NdbJTieSmokeTest.main()");
    }
}
