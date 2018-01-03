/*
 Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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
 * NdbJTieSmokeTest.java
 */

package test;

import com.mysql.ndbjtie.ndbapi.Ndb_cluster_connection;
import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.NdbError;

/**
 * Tests loading the NdbJTie libary and connecting to a cluster if running.
 */
public class NdbJTieSmokeTest extends JTieTestBase {

    private String mgmdConnect = "localhost";
    private String catalog = "db";
    private String schema = "def";

    private Ndb_cluster_connection mgmd;
    private Ndb ndb;

    protected int init() {
        // load native library
        loadSystemLibrary("ndbclient");

        // get system variable for connect string
        mgmdConnect
            = System.getProperty("jtie.unit.ndb.connectstring", mgmdConnect);

        // instantiate cluster singleton
        out.println();
        out.println("    creating mgmd conn...");
        mgmd = Ndb_cluster_connection.create(mgmdConnect);
        assert mgmd != null;
        out.println("    ... [ok, mgmd=" + mgmd + "]");

        // try to connect to cluster management node (ndb_mgmd)
        out.println();
        out.println("    connecting to mgmd ...");
        final int retries = 0;        // retries (< 0 = indefinitely)
        final int delay = 0;          // seconds to wait after retry
        final int verbose = 0;        // print report of progess
        // 0 = success, 1 = recoverable error, -1 = non-recoverable error
        final int status = mgmd.connect(retries, delay, verbose);
        out.println("    ... [" + (status == 0 ? "" : "NOT ")
                    + "connected to mgmd@" + mgmdConnect
                    + " within " + (retries * delay) + "s]");
        return status;
    }

    protected void close() {
        assert mgmd != null;
        out.println();
        out.println("    closing mgmd conn ...");
        Ndb_cluster_connection.delete(mgmd);
        out.println("    ... [ok, mgmd=" + mgmd + "]");
        mgmd = null;
    }

    protected void initConnection(String catalog, String schema) {
        // connect and wait for reaching the data nodes (ndbds)
        out.println();
        out.println("    waiting until ready...");
        final int initial_wait = 10; // secs to wait until first node detected
        final int final_wait = 0;    // secs to wait after first node detected
        // returns: 0 all nodes live, > 0 at least one node live, < 0 error
        if (mgmd.wait_until_ready(initial_wait, final_wait) < 0) {
            final String msg = ("data nodes were not ready within "
                                + (initial_wait + final_wait) + "s.");
            out.println(msg);
            throw new RuntimeException(msg);
        }
        out.println("    ... [ok]");

        // connect to database
        out.println();
        out.println("    connecting to database...");
        ndb = Ndb.create(mgmd, catalog, schema);
        assert ndb != null;
        final int max_no_tx = 10; // maximum number of parallel tx (<=1024)
        // note each scan or index scan operation uses one extra transaction
        if (ndb.init(max_no_tx) != 0) {
            String msg = "Error caught: " + ndb.getNdbError().message();
            throw new RuntimeException(msg);
        }
        out.println("    ... [ok]");
    }

    protected void closeConnection() {
        assert ndb != null;
        out.println();
        out.println("    closing database conn ...");
        Ndb.delete(ndb);
        ndb = null;
        out.println("    ... [ok]");
    }

    public void test() {
        out.println("--> NdbJTieSmokeTest.test()");

        if (init() == 0) {
            initConnection(catalog, schema);
            closeConnection();
        }
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
