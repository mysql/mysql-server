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

package com.mysql.cluster.crund;

import java.util.Set;
import java.util.List;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.ArrayList;

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
public class CrundDriver extends Driver {

    enum XMode { indy, each, bulk }
    enum LockMode { none, shared, exclusive };

    // benchmark settings
    protected final EnumSet<XMode> xModes = EnumSet.noneOf(XMode.class);
    protected LockMode lockMode;
    protected boolean renewConnection;
    protected int nOpsStart;
    protected int nOpsEnd;
    protected int nOpsScale;
    protected int maxVarbinaryBytes;
    protected int maxVarcharChars;
    protected int maxBlobBytes;
    protected int maxTextChars;
    protected final Set<String> include = new HashSet<String>();
    protected final Set<String> exclude = new HashSet<String>();

    static public void main(String[] args) throws Exception {
        parseArguments(args);
        CrundDriver driver = new CrundDriver();
        driver.run();
    }

    // ----------------------------------------------------------------------
    // benchmark intializers/finalizers
    // ----------------------------------------------------------------------

    protected void init() throws Exception {
        out.println();
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        out.println("initializing benchmark ...");
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        super.init();
    }

    protected void close() throws Exception {
        out.println();
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        out.println("closing benchmark ...");
        out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        super.close();
    }

    protected void initProperties() {
        super.initProperties();
        out.println();
        out.print("reading crund properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        final String[] xm
            = props.getProperty("xMode", "indy,each,bulk").split(",");
        for (int i = 0; i < xm.length; i++) {
            try {
                if (!xm[i].isEmpty())
                    xModes.add(XMode.valueOf(XMode.class, xm[i]));
            } catch (IllegalArgumentException e) {
                msg.append("[IGNORED] xMode:                " + xm[i] + eol);
            }
        }

        final String lm = props.getProperty("lockMode", "none");
        try {
            lockMode = LockMode.valueOf(lm);
        } catch (IllegalArgumentException e) {
            msg.append("[IGNORED] lockMode:             " + lm + eol);
            lockMode = LockMode.none;
        }

        renewConnection = parseBoolean("renewConnection", false);

        nOpsStart = parseInt("nOpsStart", 1000);
        if (nOpsStart < 1) {
            msg.append("[IGNORED] nOpsStart:            " + nOpsStart + eol);
            nOpsStart = 1000;
        }
        nOpsEnd = parseInt("nOpsEnd", nOpsStart);
        if (nOpsEnd < nOpsStart) {
            msg.append("[IGNORED] nOpsEnd:              "+ nOpsEnd + eol);
            nOpsEnd = nOpsStart;
        }
        nOpsScale = parseInt("nOpsScale", 10);
        if (nOpsScale < 2) {
            msg.append("[IGNORED] nOpsScale:            " + nOpsScale + eol);
            nOpsScale = 10;
        }

        maxVarbinaryBytes = parseInt("maxVarbinaryBytes", 100);
        if (maxVarbinaryBytes < 0) {
            msg.append("[IGNORED] maxVarbinaryBytes:    "
                       + maxVarbinaryBytes + eol);
            maxVarbinaryBytes = 100;
        }
        maxVarcharChars = parseInt("maxVarcharChars", 100);
        if (maxVarcharChars < 0) {
            msg.append("[IGNORED] maxVarcharChars:      "
                       + maxVarcharChars + eol);
            maxVarcharChars = 100;
        }

        maxBlobBytes = parseInt("maxBlobBytes", 1000);
        if (maxBlobBytes < 0) {
            msg.append("[IGNORED] maxBlobBytes:         "
                       + maxBlobBytes + eol);
            maxBlobBytes = 1000;
        }
        maxTextChars = parseInt("maxTextChars", 1000);
        if (maxTextChars < 0) {
            msg.append("[IGNORED] maxTextChars:         "
                       + maxTextChars + eol);
            maxTextChars = 1000;
        }

        final String[] ip = props.getProperty("include", "").split(",");
        for (String s : ip)
            if (!s.isEmpty())
                include.add(s);

        final String[] ep = props.getProperty("exclude", "").split(",");
        for (String s : ep)
            if (!s.isEmpty())
                exclude.add(s);

        if (msg.length() == 0) {
            out.println("    [ok: "
                        + "nOps=" + nOpsStart + ".." + nOpsEnd + "]");
        } else {
            hasIgnoredSettings = true;
            out.println();
            out.print(msg.toString());
        }
    }

    protected void printProperties() {
        super.printProperties();
        out.println();
        out.println("crund settings ...");
        out.println("xModes:                         " + xModes);
        out.println("lockMode:                       " + lockMode);
        out.println("renewConnection:                " + renewConnection);
        out.println("nOpsStart:                      " + nOpsStart);
        out.println("nOpsEnd:                        " + nOpsEnd);
        out.println("nOpsScale:                      " + nOpsScale);
        out.println("maxVarbinaryBytes:              " + maxVarbinaryBytes);
        out.println("maxVarcharChars:                " + maxVarcharChars);
        out.println("maxBlobBytes:                   " + maxBlobBytes);
        out.println("maxTextChars:                   " + maxTextChars);
        out.println("include:                        " + include);
        out.println("exclude:                        " + exclude);
    }

    // ----------------------------------------------------------------------
    // benchmark operations
    // ----------------------------------------------------------------------

    protected void runLoad(Load load) throws Exception {
        connectDB(load);

        assert (nOpsStart <= nOpsEnd && nOpsScale > 1);
        for (int i = nOpsStart; i <= nOpsEnd; i *= nOpsScale) {
            out.println();
            out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
            out.println("running load ...                [nOps=" + i + "]"
                        + load.getName());
            out.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
            runSeries(load, i);
        }

        disconnectDB(load);
    }

    protected void connectDB(Load load) throws Exception {
        out.println();
        out.println("------------------------------------------------------------");
        out.println("init connection ... ");
        out.println("------------------------------------------------------------");
        load.initConnection();
    }

    protected void disconnectDB(Load load) throws Exception {
        out.println();
        out.println("------------------------------------------------------------");
        out.println("close connection ... ");
        out.println("------------------------------------------------------------");
        load.closeConnection();
    }

    protected void reconnectDB(Load load) throws Exception {
        out.println();
        out.println("------------------------------------------------------------");
        out.println("renew connection ... ");
        out.println("------------------------------------------------------------");
        load.closeConnection();
        load.initConnection();
    }

    protected void runSeries(Load load, int nOps) throws Exception {
        if (nRuns == 0)
            return; // nothing to do

        for (int i = 1; i <= nRuns; i++) {
            // pre-run cleanup
            if (renewConnection)
                reconnectDB(load);

            out.println();
            out.println("------------------------------------------------------------");
            out.println("run " + i + " of " + nRuns + " [nOps=" + nOps + "]");
            out.println("------------------------------------------------------------");
            runOperations(load, nOps);
        }

        writeLogBuffers(load.getName());
    }

    protected void runOperations(Load load, int nOps) throws Exception {
        beginOps(nOps);
        load.clearData();
        load.runOperations(nOps);
        finishOps(nOps);
    }
}
