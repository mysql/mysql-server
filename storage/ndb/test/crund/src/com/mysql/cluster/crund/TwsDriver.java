/*
  Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.cluster.crund;

import java.lang.reflect.Constructor;
import com.mysql.cluster.crund.CrundDriver.LockMode;


public class TwsDriver extends Driver {

    // benchmark settings
    protected boolean renewConnection;
    protected boolean doJdbc;
    protected boolean doClusterj;
    protected boolean doNdbjtie;
    protected boolean doInsert;
    protected boolean doLookup;
    protected boolean doUpdate;
    protected boolean doDelete;
    protected boolean doBulk;
    protected boolean doEach;
    protected boolean doIndy;
    protected LockMode lockMode;
    protected int nRows;
    protected int nRuns;

    // benchmark resources
    protected TwsLoad jdbcLoad;
    protected TwsLoad clusterjLoad;
    protected TwsLoad ndbjtieLoad;

    static public void main(String[] args) throws Exception {
        parseArguments(args);
        TwsDriver main = new TwsDriver();
        main.run();
    }

    // ----------------------------------------------------------------------
    // benchmark intializers/finalizers
    // ----------------------------------------------------------------------

    protected TwsLoad createLoad(String className) throws Exception {
        TwsLoad load;
        // use proper generics against unchecked warnings, varargs
        Class<?> a = Class.forName(className);
        Class<? extends TwsLoad> c = a.asSubclass(TwsLoad.class);
        Constructor<? extends TwsLoad> ct = c.getConstructor(TwsDriver.class);
        load = ct.newInstance(this);
        return load;
    }

    protected void init() throws Exception {
        super.init();

        if (doJdbc) {
            assert (jdbcLoad == null);
            jdbcLoad = createLoad("com.mysql.cluster.crund.JdbcTwsLoad");
            jdbcLoad.init();
        }
        if (doClusterj) {
            assert (clusterjLoad == null);
            clusterjLoad = createLoad("com.mysql.cluster.crund.ClusterjTwsLoad");
            clusterjLoad.init();
        }
        if (doNdbjtie) {
            assert (ndbjtieLoad == null);
            ndbjtieLoad = createLoad("com.mysql.cluster.crund.NdbjtieTwsLoad");
            ndbjtieLoad.init();
        }

        initConnections();
    }

    protected void close() throws Exception {
        closeConnections();

        if (doJdbc) {
            assert (jdbcLoad != null);
            jdbcLoad.close();
            jdbcLoad = null;
        }
        if (doClusterj) {
            assert (clusterjLoad != null);
            clusterjLoad.close();
            clusterjLoad = null;
        }
        if (doNdbjtie) {
            assert (ndbjtieLoad != null);
            ndbjtieLoad.close();
            ndbjtieLoad = null;
        }

        super.close();
    }

    protected void initProperties() {
        super.initProperties();

        out.print("setting tws properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        renewConnection = parseBoolean("renewConnection", false);
        doJdbc = parseBoolean("doJdbc", false);
        doClusterj = parseBoolean("doClusterj", false);
        doNdbjtie = parseBoolean("doNdbjtie", false);
        doInsert = parseBoolean("doInsert", false);
        doLookup = parseBoolean("doLookup", false);
        doUpdate = parseBoolean("doUpdate", false);
        doDelete = parseBoolean("doDelete", false);
        doBulk = parseBoolean("doBulk", false);
        doEach = parseBoolean("doEach", false);
        doIndy = parseBoolean("doIndy", false);

        final String lm = props.getProperty("lockMode");
        try {
            lockMode = (lm == null
                        ? LockMode.READ_COMMITTED : LockMode.valueOf(lm));
        } catch (IllegalArgumentException e) {
            msg.append("[ignored] lockMode:             " + lm + eol);
            lockMode = LockMode.READ_COMMITTED;
        }

        nRows = parseInt("nRows", 256);
        if (nRows < 1) {
            msg.append("[ignored] nRows:            '"
                       + props.getProperty("nRows") + "'" + eol);
            nRows = 256;
        }

        nRuns = parseInt("nRuns", 1);
        if (nRuns < 0) {
            msg.append("[ignored] nRuns:                " + nRuns + eol);
            nRuns = 1;
        }

        if (msg.length() == 0) {
            out.println("      [ok]");
        } else {
            out.println();
            out.print(msg.toString());
        }
    }

    protected void printProperties() {
        super.printProperties();
        out.println();
        out.println("tws settings ...");
        out.println("renewConnection:                " + renewConnection);
        out.println("doJdbc:                         " + doJdbc);
        out.println("doClusterj:                     " + doClusterj);
        out.println("doNdbjtie:                      " + doNdbjtie);
        out.println("doInsert:                       " + doInsert);
        out.println("doLookup:                       " + doLookup);
        out.println("doUpdate:                       " + doUpdate);
        out.println("doDelete:                       " + doDelete);
        out.println("doBulk:                         " + doBulk);
        out.println("doEach:                         " + doEach);
        out.println("doIndy:                         " + doIndy);
        out.println("lockMode:                       " + lockMode);
        out.println("nRows:                          " + nRows);
        out.println("nRuns:                          " + nRuns);
    }

    // ----------------------------------------------------------------------
    // benchmark operations
    // ----------------------------------------------------------------------

    protected void runTests() throws Exception {
        //initConnection();

        //assert(rStart <= rEnd && rScale > 1);
        //for (int i = rStart; i <= rEnd; i *= rScale)
        runLoads();

        //closeConnection();
    }

    protected void runLoads() throws Exception {
        if (doJdbc)
            runSeries(jdbcLoad);
        if (doClusterj)
            runSeries(clusterjLoad);
        if (doNdbjtie)
            runSeries(ndbjtieLoad);
    }

    protected void runSeries(TwsLoad load) throws Exception {
        if (nRuns == 0)
            return; // nothing to do

        out.println();
        out.println("------------------------------------------------------------");
        out.print("running " + nRuns + " iterations on load: "
                  + load.getDescriptor());

        for (int i = 0; i < nRuns; i++) {
            out.println();
            out.println("------------------------------------------------------------");
            runOperations(load);
        }

        writeLogBuffers(load.getDescriptor());
        clearLogBuffers();
    }
    
    enum XMode { INDY, EACH, BULK }

    protected void runOperations(TwsLoad load) throws Exception {
        //out.println("running operations ..."
        //            + "          [nRows=" + nRows + "]");

        beginOpSeq(nRows);

        // pre-run cleanup
        if (renewConnection) {
            load.closeConnection();
            load.initConnection();
        }
        //clearData(); // not used

        load.runOperations();

        finishOpSeq(nRows);
    }

    // ----------------------------------------------------------------------
    // datastore operations
    // ----------------------------------------------------------------------

    protected void initConnections() throws Exception {
        if (doJdbc) {
            assert (jdbcLoad != null);
            jdbcLoad.initConnection();
        }
        if (doClusterj) {
            assert (clusterjLoad != null);
            clusterjLoad.initConnection();
        }
        if (doNdbjtie) {
            assert (ndbjtieLoad != null);
            ndbjtieLoad.initConnection();
        }
    }

    protected void closeConnections() throws Exception {
        if (doJdbc) {
            assert (jdbcLoad != null);
            jdbcLoad.closeConnection();
        }
        if (doClusterj) {
            assert (clusterjLoad != null);
            clusterjLoad.closeConnection();
        }
        if (doNdbjtie) {
            assert (ndbjtieLoad != null);
            ndbjtieLoad.closeConnection();
        }
    }

    //abstract protected void clearPersistenceContext() throws Exception;
    //abstract protected void clearData() throws Exception;
}
