/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

package com.mysql.cluster.crund;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * This class benchmarks standard database operations over a series
 * of transactions on an increasing data set.
 * <p>
 * The abstract database operations are variations of: Create,
 * Read, Update, Navigate, and Delete -- hence, the benchmark's name: CRUND.
 * <p>
 * The actual operations are defined by subclasses to allow measuring the
 * operation performance across different datastore implementations.
 *
 * @see <a href="http://www.urbandictionary.com/define.php?term=crund">Urban Dictionary: crund</a>
 * <ol>
 * <li> used to debase people who torture others with their illogical
 * attempts to make people laugh;
 * <li> reference to cracking obsolete jokes;
 * <li> a dance form;
 * <li> to hit hard or smash.
 * </ol>
 */
abstract public class CrundDriver extends Driver {

    enum XMode { INDY, EACH, BULK }

    // benchmark settings
    protected final EnumSet< XMode > xMode = EnumSet.noneOf(XMode.class);
    protected boolean renewConnection;
    protected boolean renewOperations;
    protected boolean logSumOfOps;
    protected boolean allowExtendedPC;
    protected int nOpsStart;
    protected int nOpsEnd;
    protected int nOpsScale;
    protected int maxVarbinaryBytes;
    protected int maxVarcharChars;
    protected int maxBlobBytes;
    protected int maxTextChars;
    protected final Set<String> exclude = new HashSet<String>();
    protected final Set<String> include = new HashSet<String>();

    // the name of the test currently being performed
    protected String operationName;

    /** The errors for the current test */
    protected StringBuilder errorBuffer;

    /** Throw an exception if an error is reported */
    protected boolean failOnError;

    // ----------------------------------------------------------------------
    // benchmark intializers/finalizers
    // ----------------------------------------------------------------------

    protected void init() throws Exception {
        out.println();
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        out.println("initializing benchmark ...");
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        super.init();

/*
        // XXX support multiple load instances
        // initialize load classes
        if (doJdbc) {
            assert (jdbcLoad == null);
            jdbcLoad = new JdbcLoad(this);
            jdbcLoad.init();
        }
        if (doClusterj) {
            assert (clusterjLoad == null);
            clusterjLoad = new ClusterjLoad(this);
            clusterjLoad.init();
        }
        if (doNdbjtie) {
            assert (ndbjtieLoad == null);
            ndbjtieLoad = new NdbjtieLoad(this);
            ndbjtieLoad.init();
        }
*/
        initLoad();
    }    

    protected void close() throws Exception {
        out.println();
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        out.println("closing benchmark ...");
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

/*
        // XXX support multiple load instances
        // close load classes
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
*/
        closeLoad();

        super.close();
    }

    protected void initProperties() {
        super.initProperties();

        out.print("setting crund properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        // parse execution modes
        final String[] xm = props.getProperty("xMode", "").split(",");
        for (int i = 0; i < xm.length; i++) {
            if (!"".equals(xm[i]))
                xMode.add(XMode.valueOf(XMode.class, xm[i]));
        }

        renewConnection = parseBoolean("renewConnection", false);
        renewOperations = parseBoolean("renewOperations", false);
        logSumOfOps = parseBoolean("logSumOfOps", true);
        allowExtendedPC = parseBoolean("allowExtendedPC", false);
        failOnError = parseBoolean("failOnError", false);

        nOpsStart = parseInt("nOpsStart", 256);
        if (nOpsStart < 1) {
            msg.append("[ignored] nOpsStart:            " + nOpsStart + eol);
            nOpsStart = 256;
        }
        nOpsEnd = parseInt("nOpsEnd", nOpsStart);
        if (nOpsEnd < nOpsStart) {
            msg.append("[ignored] nOpsEnd:              "+ nOpsEnd + eol);
            nOpsEnd = nOpsStart;
        }
        nOpsScale = parseInt("nOpsScale", 2);
        if (nOpsScale < 2) {
            msg.append("[ignored] nOpsScale:            " + nOpsScale + eol);
            nOpsScale = 2;
        }

        maxVarbinaryBytes = parseInt("maxVarbinaryBytes", 100);
        if (maxVarbinaryBytes < 0) {
            msg.append("[ignored] maxVarbinaryBytes:    "
                       + maxVarbinaryBytes + eol);
            maxVarbinaryBytes = 100;
        }
        maxVarcharChars = parseInt("maxVarcharChars", 100);
        if (maxVarcharChars < 0) {
            msg.append("[ignored] maxVarcharChars:      "
                       + maxVarcharChars + eol);
            maxVarcharChars = 100;
        }

        maxBlobBytes = parseInt("maxBlobBytes", 1000);
        if (maxBlobBytes < 0) {
            msg.append("[ignored] maxBlobBytes:         "
                       + maxBlobBytes + eol);
            maxBlobBytes = 1000;
        }
        maxTextChars = parseInt("maxTextChars", 1000);
        if (maxTextChars < 0) {
            msg.append("[ignored] maxTextChars:         "
                       + maxTextChars + eol);
            maxTextChars = 1000;
        }

        // initialize exclude set
        final String[] excludeProperty = props.getProperty("exclude", "").split(",");
        for (int i = 0; i < excludeProperty.length; i++) {
            String excludeTest = excludeProperty[i];
            if (!excludeTest.isEmpty()) {
                exclude.add(excludeTest);
            }
        }

        // initialize include set
        final String[] includeProperty = props.getProperty("include", "").split(",");
        for (int i = 0; i < includeProperty.length; ++i) {
            String includeTest = includeProperty[i];
            if (!includeTest.isEmpty()) {
                include.add(includeTest);
            }
        }

        if (msg.length() == 0) {
            out.println("    [ok: "
                        + "nOps=" + nOpsStart + ".." + nOpsEnd + "]");
        } else {
            out.println();
            out.print(msg.toString());
        }
    }

    protected void printProperties() {
        super.printProperties();

        out.println();
        out.println("crund settings ...");
        out.println("xMode:                          " + xMode);
        out.println("renewConnection:                " + renewConnection);
        out.println("renewOperations:                " + renewOperations);
        out.println("logSumOfOps:                    " + logSumOfOps);
        out.println("allowExtendedPC:                " + allowExtendedPC);
        out.println("nOpsStart:                      " + nOpsStart);
        out.println("nOpsEnd:                        " + nOpsEnd);
        out.println("nOpsScale:                      " + nOpsScale);
        out.println("maxVarbinaryBytes:              " + maxVarbinaryBytes);
        out.println("maxVarcharChars:                " + maxVarcharChars);
        out.println("maxBlobBytes:                   " + maxBlobBytes);
        out.println("maxTextChars:                   " + maxTextChars);
        out.println("exclude:                        " + exclude);
        out.println("include:                        " + include);
    }

    // ----------------------------------------------------------------------
    // benchmark operations
    // ----------------------------------------------------------------------

    // XXX move to generic load class
    // a database operation to be benchmarked
    protected abstract class Op {
        final protected String name;

        public Op(String name) { this.name = name; }

        public String getName() { return name; }

        public abstract void run(int nOps) throws Exception;
    };

    // XXX move to generic load class
    // the list of database operations to be benchmarked
    protected final List<Op> ops = new ArrayList<Op>();

    // manages list of database operations
    abstract protected void initOperations() throws Exception;
    abstract protected void closeOperations() throws Exception;

    protected void runTests() throws Exception {
        initConnections();
        runLoads();
        closeConnections();
    }
    
    protected void runLoads() throws Exception {
/*
        // XXX support multiple load instances
        if (doJdbc)
            runLoads(jdbcLoad);
        if (doClusterj)
            runLoads(clusterjLoad);
        if (doNdbjtie)
            runLoads(ndbjtieLoad);
*/
        runLoad();
    }
    
    protected void runLoad() throws Exception {
        assert (nOpsStart <= nOpsEnd && nOpsScale > 1);
        for (int i = nOpsStart; i <= nOpsEnd; i *= nOpsScale) {
            try {
                out.println();
                out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
                // XXX support multiple load instances
                //out.print("running load nOps = " + i + " on "
                //          + load.getDescriptor());
                out.println("running load [" + i + " nOps] on " + descr);
                out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
                runSeries(i);
            } catch (Exception ex) {
                // already in rollback for database/orm exceptions
                throw ex;
            }
        }
    }

    protected void runSeries(int nOps) throws Exception {
        if (nRuns == 0)
            return; // nothing to do

        for (int i = 1; i <= nRuns; i++) {
            out.println();
            out.println("------------------------------------------------------------");
            out.println("run " + i + " of " + nRuns + " [" + nOps + " nOps]");
            out.println("------------------------------------------------------------");
            // XXX runLoad(load);
            runLoad(nOps);
        }

        // XXX support multiple load instances
        //writeLogBuffers(load.getDescriptor());
        writeLogBuffers(descr);
        clearLogBuffers();
    }

    protected void runLoad(int nOps) throws Exception {
        // log buffers
        if (logRealTime) {
            rtimes.append(nOps);
            ta = 0;
        }
        if (logMemUsage) {
            musage.append(nOps);
            ma = 0;
        }

        // pre-run cleanup
        if (renewConnection) {
            // XXX move to generic load class?
            closeOperations();
            closeConnection();
            initConnection();
            initOperations();
        } else if (renewOperations) {
            closeOperations();
            initOperations();
        }
        clearData();

        runSequence(nOps);

        if (logSumOfOps) {
            out.println();
            out.println("total");
            if (logRealTime) {
                out.println("tx real time                    " + ta
                            + "\tms");
            }
            if (logMemUsage) {
                out.println("net mem usage                   "
                            + (ma >= 0 ? "+" : "") + ma
                            + "\tKiB");
            }
        }

        // log buffers
        if (logHeader) {
            if (logSumOfOps) {
                header.append("\ttotal");
            }
            logHeader = false;
        }
        if (logRealTime) {
            if (logSumOfOps) {
                rtimes.append("\t" + ta);
            }
            rtimes.append(endl);
        }
        if (logMemUsage) {
            if (logSumOfOps) {
                musage.append("\t" + ma);
            }
            musage.append(endl);
        }
    }

    // XXX move to generic load class
    protected void runSequence(int nOps) throws Exception {
        for (Op op : ops) {
            // pre-tx cleanup
            if (!allowExtendedPC) {
                // effectively prevent caching beyond Tx scope by clearing
                // any data/result caches before the next transaction
                clearPersistenceContext();
            }
            runOperation(op, nOps);
            reportErrors();
        }
    }

    // XXX move to generic load class
    protected void runOperation(Op op, int nOps) throws Exception {
        operationName = op.getName();
        // if there is an include list and this test is included, or
        // there is not an include list and this test is not excluded
        if ((include.size() != 0 && include.contains(operationName))
                || (include.size() == 0 && !exclude.contains(operationName))) {
            begin(operationName);
            op.run(nOps);
            finish(operationName);
        }
    }

    /** Add an error to the existing errors */
    protected void appendError(String where) {
        if (errorBuffer == null) {
            errorBuffer = new StringBuilder();
        }
        errorBuffer.append("Error in operation ");
        errorBuffer.append(operationName);
        errorBuffer.append(": ");
        errorBuffer.append(where);
        errorBuffer.append('\n');
    }

    /** Report errors and reset the error buffer */
    protected void reportErrors() {
        if (errorBuffer != null) {
            if (failOnError) {
                throw new RuntimeException(errorBuffer.toString());
            }
            System.out.println(errorBuffer.toString());
            errorBuffer = null;
        }
    }
    // XXX move to generic load class
    // reports an error if a condition is not met
    static protected final void verify(boolean cond) {
        //assert (cond);
        if (!cond)
            throw new RuntimeException("data verification failed.");
    }

    // XXX move to generic load class
    static protected final void verify(int exp, int act) {
        if (exp != act)
            throw new RuntimeException("data verification failed:"
                                       + " expected = " + exp
                                       + ", actual = " + act);
    }

    // XXX move to generic load class
    protected final void verify(String where, int exp, int act) {
        if (exp != act)
            appendError(" data verification failed:"
                    + " expected = " + exp
                    + ", actual = " + act);
    }

    // XXX move to generic load class
    static protected final void verify(String exp, String act) {
        if ((exp == null && act != null)
            || (exp != null && !exp.equals(act)))
            throw new RuntimeException("data verification failed:"
                                       + " expected = '" + exp + "'"
                                       + ", actual = '" + act + "'");
    }

    // ----------------------------------------------------------------------
    // helpers
    // ----------------------------------------------------------------------

    // XXX move to generic load class
    static final protected String myString(int n) {
        final StringBuilder s = new StringBuilder();
        switch (n) {
        case 1:
            s.append('i');
            break;
        case 2:
            for (int i = 0; i < 10; i++) s.append('x');
            break;
        case 3:
            for (int i = 0; i < 100; i++) s.append('c');
            break;
        case 4:
            for (int i = 0; i < 1000; i++) s.append('m');
            break;
        case 5:
            for (int i = 0; i < 10000; i++) s.append('X');
            break;
        case 6:
            for (int i = 0; i < 100000; i++) s.append('C');
            break;
        case 7:
            for (int i = 0; i < 1000000; i++) s.append('M');
            break;
        default:
            throw new IllegalArgumentException("unsupported 10**n = " + n);
        }
        return s.toString();
    }

    // XXX move to generic load class
    static final protected byte[] myBytes(String s) {
        final char[] c = s.toCharArray();
        final int n = c.length;
        final byte[] b = new byte[n];
        for (int i = 0; i < n; i++) b[i] = (byte)c[i];
        return b;
    }

    // XXX move to generic load class
    // some string and byte constants
    static final protected String string1 = myString(1);
    static final protected String string2 = myString(2);
    static final protected String string3 = myString(3);
    static final protected String string4 = myString(4);
    static final protected String string5 = myString(5);
    static final protected String string6 = myString(6);
    static final protected String string7 = myString(7);
    static final protected byte[] bytes1 = myBytes(string1);
    static final protected byte[] bytes2 = myBytes(string2);
    static final protected byte[] bytes3 = myBytes(string3);
    static final protected byte[] bytes4 = myBytes(string4);
    static final protected byte[] bytes5 = myBytes(string5);
    static final protected byte[] bytes6 = myBytes(string6);
    static final protected byte[] bytes7 = myBytes(string7);
    static final protected String[] strings
        = { string1, string2, string3, string4, string5, string6, string7 };
    static final protected byte[][] bytes
        = { bytes1, bytes2, bytes3, bytes4, bytes5, bytes6, bytes7 };

    // ----------------------------------------------------------------------
    // datastore operations
    // ----------------------------------------------------------------------

    protected void initConnections() throws Exception {
        out.println();
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        out.println("initializing connections ...");
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

/*
        // XXX support multiple load instances
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
*/
        initConnection();

        // XXX move to generic load class
        initOperations();
    }

    protected void closeConnections() throws Exception {
        out.println();
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        out.println("closing connections ...");
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

        // XXX move to generic load class
        closeOperations();

        closeConnection();
/*
        // XXX support multiple load instances
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
*/
    }

    abstract protected void initLoad() throws Exception;
    abstract protected void closeLoad() throws Exception;
    abstract protected void initConnection() throws Exception;
    abstract protected void closeConnection() throws Exception;
    abstract protected void clearPersistenceContext() throws Exception;
    abstract protected void clearData() throws Exception;
}
