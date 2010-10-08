/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

import java.util.Properties;
import java.util.List;
import java.util.Set;
import java.util.HashSet;
import java.util.ArrayList;
import java.util.Date;
import java.text.SimpleDateFormat;

import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.InputStream;


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

    // benchmark settings
    protected boolean renewOperations;
    protected boolean logSumOfOps;
    protected boolean allowExtendedPC;
    protected int aStart;
    protected int bStart;
    protected int aEnd;
    protected int bEnd;
    protected int aScale;
    protected int bScale;
    protected int maxVarbinaryBytes;
    protected int maxVarcharChars;
    protected int maxBlobBytes;
    protected int maxTextChars;
    protected final Set<String> exclude = new HashSet<String>();

    // ----------------------------------------------------------------------
    // benchmark intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        super.initProperties();

        out.print("setting crund properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        renewOperations = parseBoolean("renewOperations", false);
        logSumOfOps = parseBoolean("logSumOfOps", true);
        allowExtendedPC = parseBoolean("allowExtendedPC", false);

        aStart = parseInt("aStart", 256);
        if (aStart < 1) {
            msg.append("[ignored] aStart:               " + aStart + eol);
            aStart = 256;
        }
        aEnd = parseInt("aEnd", aStart);
        if (aEnd < aStart) {
            msg.append("[ignored] aEnd:                 "+ aEnd + eol);
            aEnd = aStart;
        }
        aScale = parseInt("aScale", 2);
        if (aScale < 2) {
            msg.append("[ignored] aScale:               " + aScale + eol);
            aScale = 2;
        }

        bStart = parseInt("bStart", 256);
        if (bStart < 1) {
            msg.append("[ignored] bStart:               " + bStart + eol);
            bStart = 256;
        }
        bEnd = parseInt("bEnd", bStart);
        if (bEnd < bStart) {
            msg.append("[ignored] bEnd:                 " + bEnd + eol);
            bEnd = bStart;
        }
        bScale = parseInt("bScale", 2);
        if (bScale < 2) {
            msg.append("[ignored] bScale:               " + bScale + eol);
            bScale = 2;
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
        final String[] e = props.getProperty("exclude", "").split(",");
        for (int i = 0; i < e.length; i++) {
            exclude.add(e[i]);
        }

        if (msg.length() == 0) {
            out.println("    [ok: "
                        + "A=" + aStart + ".." + aEnd
                        + ", B=" + bStart + ".." + bEnd + "]");
        } else {
            out.println();
            out.print(msg.toString());
        }
    }

    protected void printProperties() {
        super.printProperties();

        out.println();
        out.println("crund settings ...");
        out.println("renewOperations:                " + renewOperations);
        out.println("logSumOfOps:                    " + logSumOfOps);
        out.println("allowExtendedPC:                " + allowExtendedPC);
        out.println("aStart:                         " + aStart);
        out.println("bStart:                         " + bStart);
        out.println("aEnd:                           " + aEnd);
        out.println("bEnd:                           " + bEnd);
        out.println("aScale:                         " + aScale);
        out.println("bScale:                         " + bScale);
        out.println("maxVarbinaryBytes:              " + maxVarbinaryBytes);
        out.println("maxVarcharChars:                " + maxVarcharChars);
        out.println("maxBlobBytes:                   " + maxBlobBytes);
        out.println("maxTextChars:                   " + maxTextChars);
        out.println("exclude:                        " + exclude);
    }

    // ----------------------------------------------------------------------
    // benchmark operations
    // ----------------------------------------------------------------------

    // a database operation to be benchmarked
    protected abstract class Op {
        final protected String name;

        public Op(String name) { this.name = name; }

        public String getName() { return name; }

        public abstract void run(int countA, int countB) throws Exception;
    };

    // the list of database operations to be benchmarked
    protected final List<Op> ops = new ArrayList<Op>();

    // manages list of database operations
    abstract protected void initOperations() throws Exception;
    abstract protected void closeOperations() throws Exception;

    protected void runTests() throws Exception {
        initConnection();
        initOperations();

        assert (aStart <= aEnd && aScale > 1);
        assert (bStart <= bEnd && bScale > 1);
        for (int i = aStart; i <= aEnd; i *= aScale) {
            for (int j = bStart; j <= bEnd; j *= bScale) {
                try {
                    runOperations(i, j);
                } catch (Exception ex) {
                    // already in rollback for database/orm exceptions
                    throw ex;
                }
            }
        }

        out.println();
        out.println("------------------------------------------------------------");
        out.println();

        clearData();
        closeOperations();
        closeConnection();
    }

    protected void runOperations(int countA, int countB) throws Exception {
        out.println();
        out.println("------------------------------------------------------------");

        if (countA > countB) {
            out.println("skipping operations ..."
                        + "         [A=" + countA + ", B=" + countB + "]");
            return;
        }
        out.println("running operations ..."
                    + "          [A=" + countA + ", B=" + countB + "]");

        // log buffers
        if (logRealTime) {
            rtimes.append("A=" + countA + ", B=" + countB);
            ta = 0;
        }
        if (logMemUsage) {
            musage.append("A=" + countA + ", B=" + countB);
            ma = 0;
        }

        // pre-run cleanup
        if (renewConnection) {
            closeOperations();
            closeConnection();
            initConnection();
            initOperations();
        } else if (renewOperations) {
            closeOperations();
            initOperations();
        }
        clearData();

        // run operations
        for (Op op : ops) {
            // pre-tx cleanup
            if (!allowExtendedPC) {
                // effectively prevent caching beyond Tx scope by clearing
                // any data/result caches before the next transaction
                clearPersistenceContext();
            }
            runOp(op, countA, countB);
        }

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
            rtimes.append(endl);
            if (logSumOfOps) {
                rtimes.append("\t" + ta);
            }
        }
        if (logMemUsage) {
            if (logSumOfOps) {
                musage.append("\t" + ma);
            }
            musage.append(endl);
        }
    }

    protected void runOp(Op op, int countA, int countB) throws Exception {
        final String name = op.getName();
        if (!exclude.contains(name)) {
            begin(name);
            op.run(countA, countB);
            commit(name);
        }
    }

    // reports an error if a condition is not met
    static protected final void verify(boolean cond) {
        //assert (cond);
        if (!cond)
            throw new RuntimeException("wrong data; verification failed");
    }

    // ----------------------------------------------------------------------
    // helpers
    // ----------------------------------------------------------------------

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

    static final protected byte[] myBytes(String s) {
        final char[] c = s.toCharArray();
        final int n = c.length;
        final byte[] b = new byte[n];
        for (int i = 0; i < n; i++) b[i] = (byte)c[i];
        return b;
    }

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
}
