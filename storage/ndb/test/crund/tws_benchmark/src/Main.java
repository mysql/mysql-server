/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (C) 2010 MySQL
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

//package com.mysql.cluster.crund.tws;

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.LockMode;
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

import com.mysql.ndbjtie.ndbapi.Ndb_cluster_connection;
import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Table;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.ColumnConst;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.IndexConst;
import com.mysql.ndbjtie.ndbapi.NdbErrorConst;
import com.mysql.ndbjtie.ndbapi.NdbError;
import com.mysql.ndbjtie.ndbapi.NdbTransaction;
import com.mysql.ndbjtie.ndbapi.NdbOperation;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbRecAttr;

import java.io.PrintWriter;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.nio.IntBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.CharsetEncoder;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CoderResult;
import java.nio.charset.CharacterCodingException;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.PreparedStatement;
import java.sql.ResultSet;

import java.util.Properties;

public class Main
{
    // console
    static protected final PrintWriter out = new PrintWriter(System.out, true);
    static protected final PrintWriter err = new PrintWriter(System.err, true);

    // benchmark settings
    static protected final String propsFileName  = "run.properties";
    static protected final Properties props = new Properties();
    static protected boolean doJdbc;
    static protected boolean doClusterj;
    static protected boolean doNdbjtie;
    static protected boolean doInsert;
    static protected boolean doLookup;
    static protected boolean doUpdate;
    static protected boolean doDelete;
    static protected boolean doSingle;
    static protected boolean doBulk;
    static protected boolean doBatch;
    static protected LockMode lockMode;
    static protected int nRows;
    static protected int nRuns;

    // JDBC resources
    protected Connection connection;
    protected PreparedStatement ins0;
    protected PreparedStatement sel0;
    protected PreparedStatement upd0;
    protected PreparedStatement del0;

    // ClusterJ resources
    protected SessionFactory sessionFactory;
    protected Session session;

    // NDB JTie resources
    protected Ndb_cluster_connection mgmd;
    protected Ndb ndb;
    protected NdbTransaction tx;
    protected int ndbOpLockMode;

    // NDB JTie metadata resources
    protected TableConst table_t0;
    protected ColumnConst column_c0;
    protected ColumnConst column_c1;
    protected ColumnConst column_c2;
    protected ColumnConst column_c3;
    protected ColumnConst column_c4;
    protected ColumnConst column_c5;
    protected ColumnConst column_c6;
    protected ColumnConst column_c7;
    protected ColumnConst column_c8;
    protected ColumnConst column_c9;
    protected ColumnConst column_c10;
    protected ColumnConst column_c11;
    protected ColumnConst column_c12;
    protected ColumnConst column_c13;
    protected ColumnConst column_c14;
    protected int attr_c0;
    protected int attr_c1;
    protected int attr_c2;
    protected int attr_c3;
    protected int attr_c4;
    protected int attr_c5;
    protected int attr_c6;
    protected int attr_c7;
    protected int attr_c8;
    protected int attr_c9;
    protected int attr_c10;
    protected int attr_c11;
    protected int attr_c12;
    protected int attr_c13;
    protected int attr_c14;
    protected int width_c0;
    protected int width_c1;
    protected int width_c2;
    protected int width_c3;
    protected int width_c4;
    protected int width_c5;
    protected int width_c6;
    protected int width_c7;
    protected int width_c8;
    protected int width_c9;
    protected int width_c10;
    protected int width_c11;
    protected int width_c12;
    protected int width_c13;
    protected int width_c14;

    // NDB JTie data resources
    protected ByteBuffer bb_r;

    // NDB JTie static resources
    static protected final ByteOrder bo = ByteOrder.nativeOrder();
    static protected final Charset cs;
    static protected final CharsetEncoder csEncoder;
    static protected final CharsetDecoder csDecoder;
    static {
        // default charset for mysql is "ISO-8859-1" ("US-ASCII", "UTF-8")
        cs = Charset.forName("ISO-8859-1");
        csDecoder = cs.newDecoder();
        csEncoder = cs.newEncoder();

        // report any unclean transcodings
        csEncoder
            .onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT);
        csDecoder
            .onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT);
    }

    static public void main(String[] args) throws SQLException, IOException {
        parseProperties();

        Main main = new Main();
        main.init();
        main.run();
        main.close();
    }

    // ----------------------------------------------------------------------

    static public void parseProperties() throws IOException {
        out.println("reading properties file " + propsFileName + " ...");
        InputStream is = null;
        try {
            is = new FileInputStream(propsFileName);
            props.load(is);
        } finally {
            if (is != null)
                is.close();
        }

        doJdbc = parseBoolean("doJdbc", false);
        doClusterj = parseBoolean("doClusterj", false);
        doNdbjtie = parseBoolean("doNdbjtie", false);
        doInsert = parseBoolean("doInsert", false);
        doLookup = parseBoolean("doLookup", false);
        doUpdate = parseBoolean("doUpdate", false);
        doDelete = parseBoolean("doDelete", false);
        doSingle = parseBoolean("doSingle", false);
        doBulk = parseBoolean("doBulk", false);
        doBatch = parseBoolean("doBatch", false);
        lockMode = parseLockMode("lockMode", LockMode.READ_COMMITTED);
        nRows = parseInt("nRows", 50000);
        nRuns = parseInt("nRuns", 5);

        out.println("doJdbc     : " + doJdbc);
        out.println("doClusterj : " + doClusterj);
        out.println("doNdbjtie  : " + doNdbjtie);
        out.println("doInsert   : " + doInsert);
        out.println("doLookup   : " + doLookup);
        out.println("doUpdate   : " + doUpdate);
        out.println("doDelete   : " + doDelete);
        out.println("doSingle   : " + doSingle);
        out.println("doBulk     : " + doBulk);
        out.println("doBatch    : " + doBatch);
        out.println("lockMode   : " + lockMode);
        out.println("nRows      : " + nRows);
        out.println("nRuns      : " + nRuns);
    }

    static protected boolean parseBoolean(String k, boolean vdefault) {
        final String v = props.getProperty(k);
        return (v == null ? vdefault : Boolean.parseBoolean(v));
    }

    static protected int parseInt(String k, int vdefault) {
        final String v = props.getProperty(k);
        try {
            return (v == null ? vdefault : Integer.parseInt(v));
        } catch (NumberFormatException e) {
            final NumberFormatException nfe = new NumberFormatException(
                "invalid value of benchmark property ('" + k + "', '"
                + v + "').");
            nfe.initCause(e);
            throw nfe;
        }
    }

    static protected LockMode parseLockMode(String k, LockMode vdefault) {
        final String v = props.getProperty(k);
        try {
            return (v == null ? vdefault : LockMode.valueOf(v));
        } catch (IllegalArgumentException e) {
            final IllegalArgumentException iae = new IllegalArgumentException(
                "invalid value of benchmark property ('" + k + "', '"
                + v + "').");
            iae.initCause(e);
            throw iae;
        }
    }

    // ----------------------------------------------------------------------

    public void init() throws SQLException {
        if (doJdbc)
            initJdbcConnection();
        if (doClusterj)
            initClusterjConnection();
        if (doNdbjtie)
            initNdbjtieConnection();
    }

    public void close() throws SQLException {
        if (doJdbc)
            closeJdbcConnection();
        if (doClusterj)
            closeClusterjConnection();
        if (doNdbjtie)
            closeNdbjtieConnection();
    }

    // ----------------------------------------------------------------------

    protected void initJdbcConnection() throws SQLException {
        assert (connection == null);

        out.println();
        out.println("initializing jdbc resources ...");

        // load the JDBC driver class
        out.print("loading jdbc driver ...");
        out.flush();
        final String driver
            = props.getProperty("jdbc.driver", "com.mysql.jdbc.Driver");
        final Class cls;
        try {
            cls = Class.forName(driver);
        } catch (ClassNotFoundException e) {
            out.println("Cannot load JDBC driver '" + driver
                        + "' from classpath '"
                        + System.getProperty("java.class.path") + "'");
            throw new RuntimeException(e);
        }
        out.println("          [ok: " + cls.getName() + "]");

        // create a connection to the database
        out.print("starting jdbc connection ...");
        out.flush();
        final String url
            = props.getProperty("jdbc.url",
                                "jdbc:mysql://localhost/testdb");
        final String username
            = props.getProperty("jdbc.user", "root");
        final String password
            = props.getProperty("jdbc.password", "");
        try {
            connection = DriverManager.getConnection(url, username, password);
        } catch (SQLException e) {
            out.println("Cannot connect to database '" + url + "'");
            throw new RuntimeException(e);
        }
        out.println("     [ok: " + url + "]");

        out.print("setting isolation level ...");
        out.flush();
        // ndb storage engine only supports READ_COMMITTED
        final int il = Connection.TRANSACTION_READ_COMMITTED;
        connection.setTransactionIsolation(il);
        out.print("      [ok: ");
        switch (connection.getTransactionIsolation()) {
        case Connection.TRANSACTION_READ_UNCOMMITTED:
            out.print("READ_UNCOMMITTED");
            break;
        case Connection.TRANSACTION_READ_COMMITTED:
            out.print("READ_COMMITTED");
            break;
        case Connection.TRANSACTION_REPEATABLE_READ:
            out.print("REPEATABLE_READ");
            break;
        case Connection.TRANSACTION_SERIALIZABLE:
            out.print("SERIALIZABLE");
            break;
        default:
            assert false;
        }
        out.println("]");
        
        initJdbcPreparedStatements();
    }

    protected void closeJdbcConnection() throws SQLException {
        assert (connection != null);

        out.println();
        out.println("releasing jdbc resources ...");

        closeJdbcPreparedStatements();

        out.print("closing jdbc connection ...");
        out.flush();
        connection.close();
        connection = null;
        out.println("      [ok]");
    }

    protected void initJdbcPreparedStatements() throws SQLException {
        assert (connection != null);
        assert (ins0 == null);
        assert (sel0 == null);
        assert (upd0 == null);
        assert (del0 == null);

        out.print("using lock mode for reads ...    [ok: ");
        final String lm;
        switch (lockMode) {
        case READ_COMMITTED:
            lm = "";
            break;
        case SHARED:
            lm = " LOCK IN share mode";
            break;
        case EXCLUSIVE:
            lm = " FOR UPDATE";
            break;
        default:
            lm = "";
            assert false;
        }
        out.println("SELECT ..." + lm + ";]");

        out.print("compiling jdbc statements ...");
        out.flush();

        final String sqlIns0 = "INSERT INTO mytable (c0, c1, c2, c3, c5, c6, c7, c8) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        ins0 = connection.prepareStatement(sqlIns0);

        final String sqlSel0 = ("SELECT * FROM mytable where c0=?" + lm);
        sel0 = connection.prepareStatement(sqlSel0);

        final String sqlUpd0 = "UPDATE mytable SET c1 = ?, c2 = ?, c3 = ?, c5 = ?, c6 = ?, c7 = ?, c8 = ? WHERE c0=?";
        upd0 = connection.prepareStatement(sqlUpd0);

        final String sqlDel0 = "DELETE FROM mytable WHERE c0=?";
        del0 = connection.prepareStatement(sqlDel0);

        out.println("    [ok]");
    }

    protected void closeJdbcPreparedStatements() throws SQLException {
        assert (ins0 != null);
        assert (sel0 != null);
        assert (upd0 != null);
        assert (del0 != null);

        out.print("closing jdbc statements ...");
        out.flush();

        ins0.close();
        ins0 = null;

        sel0.close();
        sel0 = null;

        upd0.close();
        upd0 = null;

        del0.close();
        del0 = null;

        out.println("      [ok]");
    }

    // ----------------------------------------------------------------------

    protected void initClusterjConnection() {
        assert (sessionFactory == null);
        assert (session == null);

        out.println();
        out.println("initializing clusterj resources ...");

        out.print("starting clusterj session ...");
        out.flush();
        sessionFactory = ClusterJHelper.getSessionFactory(props);
        session = sessionFactory.getSession();
        out.println("    [ok]");

        out.print("setting session lock mode ...");
        session.setLockMode(lockMode);
        out.println("    [ok: " + lockMode + "]");
    }

    protected void closeClusterjConnection() {
        assert (session != null);
        assert (sessionFactory != null);

        out.println();
        out.println("releasing clusterj resources ...");

        out.print("closing clusterj session ...");
        out.flush();
        session.close();
        session = null;
        sessionFactory.close();
        sessionFactory = null;
        out.println("     [ok]");
    }

    // ----------------------------------------------------------------------

    protected void initNdbjtieConnection() {
        assert (mgmd == null);
        assert (ndb == null);

        out.println();
        out.println("initializing ndbjtie resources ...");

        // load native library (better diagnostics doing it explicitely)
        loadSystemLibrary("ndbclient");

        // read connection properties
        final String mgmdConnect
            = props.getProperty("com.mysql.clusterj.connectstring",
                                "localhost:1186");
        final String catalog
            = props.getProperty("com.mysql.clusterj.database",
                                "testdb");
        final String schema
            = "def";
        assert (mgmdConnect != null);
        assert (catalog != null);
        assert (schema != null);

        // instantiate NDB cluster singleton
        out.print("creating cluster connection ...");
        out.flush();
        mgmd = Ndb_cluster_connection.create(mgmdConnect);
        assert mgmd != null;
        out.println("  [ok]");

        // connect to cluster management node (ndb_mgmd)
        out.print("connecting to cluster ...");
        out.flush();
        final int retries = 0;        // retries (< 0 = indefinitely)
        final int delay = 0;          // seconds to wait after retry
        final int verbose = 1;        // print report of progess
        // 0 = success, 1 = recoverable error, -1 = non-recoverable error
        if (mgmd.connect(retries, delay, verbose) != 0) {
            final String msg = ("mgmd@" + mgmdConnect
                                + " was not ready within "
                                + (retries * delay) + "s.");
            out.println(msg);
            throw new RuntimeException("!!! " + msg);
        }
        out.println("        [ok: " + mgmdConnect + "]");

        // connect to data nodes (ndbds)
        out.print("waiting for data nodes ...");
        out.flush();
        final int initial_wait = 10; // secs to wait until first node detected
        final int final_wait = 0;    // secs to wait after first node detected
        // returns: 0 all nodes live, > 0 at least one node live, < 0 error
        if (mgmd.wait_until_ready(initial_wait, final_wait) < 0) {
            final String msg = ("data nodes were not ready within "
                                + (initial_wait + final_wait) + "s.");
            out.println(msg);
            throw new RuntimeException(msg);
        }
        out.println("       [ok]");

        // connect to database
        out.print("connecting to database ...");
        ndb = Ndb.create(mgmd, catalog, schema);
        final int max_no_tx = 10; // maximum number of parallel tx (<=1024)
        // note each scan or index scan operation uses one extra transaction
        if (ndb.init(max_no_tx) != 0) {
            String msg = "Error caught: " + ndb.getNdbError().message();
            throw new RuntimeException(msg);
        }
        out.println("       [ok: " + catalog + "." + schema + "]");

        initNdbjtieMeta();

        initNdbjtieBuffers();

        out.print("using lock mode for reads ...    [ok: ");
        switch (lockMode) {
        case READ_COMMITTED:
            ndbOpLockMode = NdbOperation.LockMode.LM_CommittedRead;
            out.print("LM_CommittedRead");
            break;
        case SHARED:
            ndbOpLockMode = NdbOperation.LockMode.LM_Read;
            out.print("LM_Read");
            break;
        case EXCLUSIVE:
            ndbOpLockMode = NdbOperation.LockMode.LM_Exclusive;
            out.print("LM_Exclusive");
            break;
        default:
            ndbOpLockMode = NdbOperation.LockMode.LM_CommittedRead;
            assert false;
        }
        out.println("]");
    }

    protected void closeNdbjtieConnection() {
        assert (mgmd != null);
        assert (ndb != null);

        out.println();
        out.println("releasing ndbjtie resources ...");

        closeNdbjtieBuffers();

        closeNdbjtieMeta();

        out.print("closing database connection ...");
        out.flush();
        Ndb.delete(ndb);
        ndb = null;
        out.println("  [ok]");

        out.print("closing cluster connection ...");
        out.flush();
        if (mgmd != null)
            Ndb_cluster_connection.delete(mgmd);
        mgmd = null;
        out.println("   [ok]");
    }

    protected void initNdbjtieMeta() {
        assert (ndb != null);
        assert (table_t0 == null);
        assert (column_c0 == null);

        out.print("caching metadata ...");
        out.flush();

        final Dictionary dict = ndb.getDictionary();

        if ((table_t0 = dict.getTable("mytable")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));

        if ((column_c0 = table_t0.getColumn("c0")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c1 = table_t0.getColumn("c1")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c2 = table_t0.getColumn("c2")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c3 = table_t0.getColumn("c3")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c4 = table_t0.getColumn("c4")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c5 = table_t0.getColumn("c5")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c6 = table_t0.getColumn("c6")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c7 = table_t0.getColumn("c7")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c8 = table_t0.getColumn("c8")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c9 = table_t0.getColumn("c9")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c10 = table_t0.getColumn("c10")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c11 = table_t0.getColumn("c11")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c12 = table_t0.getColumn("c12")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c13 = table_t0.getColumn("c13")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));
        if ((column_c14 = table_t0.getColumn("c14")) == null)
            throw new RuntimeException(toStr(dict.getNdbError()));

        attr_c0 = column_c0.getColumnNo();
        attr_c1 = column_c1.getColumnNo();
        attr_c2 = column_c2.getColumnNo();
        attr_c3 = column_c3.getColumnNo();
        attr_c4 = column_c4.getColumnNo();
        attr_c5 = column_c5.getColumnNo();
        attr_c6 = column_c6.getColumnNo();
        attr_c7 = column_c7.getColumnNo();
        attr_c8 = column_c8.getColumnNo();
        attr_c9 = column_c9.getColumnNo();
        attr_c10 = column_c10.getColumnNo();
        attr_c11 = column_c11.getColumnNo();
        attr_c12 = column_c12.getColumnNo();
        attr_c13 = column_c13.getColumnNo();
        attr_c14 = column_c14.getColumnNo();

        width_c0 = ndbjtieColumnWidth(column_c0);
        width_c1 = ndbjtieColumnWidth(column_c1);
        width_c2 = ndbjtieColumnWidth(column_c2);
        width_c3 = ndbjtieColumnWidth(column_c3);
        width_c4 = ndbjtieColumnWidth(column_c4);
        width_c5 = ndbjtieColumnWidth(column_c5);
        width_c6 = ndbjtieColumnWidth(column_c6);
        width_c7 = ndbjtieColumnWidth(column_c7);
        width_c8 = ndbjtieColumnWidth(column_c8);
        width_c9 = ndbjtieColumnWidth(column_c9);
        width_c10 = ndbjtieColumnWidth(column_c10);
        width_c11 = ndbjtieColumnWidth(column_c11);
        width_c12 = ndbjtieColumnWidth(column_c12);
        width_c13 = ndbjtieColumnWidth(column_c13);
        width_c14 = ndbjtieColumnWidth(column_c14);

        out.println("             [ok]");
    }

    protected void closeNdbjtieMeta() {
        assert (ndb != null);
        assert (table_t0 != null);
        assert (column_c0 != null);

        out.print("clearing metadata cache...");
        out.flush();

        width_c14 = 0;
        width_c13 = 0;
        width_c12 = 0;
        width_c11 = 0;
        width_c10 = 0;
        width_c9 = 0;
        width_c8 = 0;
        width_c7 = 0;
        width_c6 = 0;
        width_c5 = 0;
        width_c4 = 0;
        width_c3 = 0;
        width_c2 = 0;
        width_c1 = 0;
        width_c0 = 0;

        column_c14 = null;
        column_c13 = null;
        column_c12 = null;
        column_c11 = null;
        column_c10 = null;
        column_c9 = null;
        column_c8 = null;
        column_c7 = null;
        column_c6 = null;
        column_c5 = null;
        column_c4 = null;
        column_c3 = null;
        column_c2 = null;
        column_c1 = null;
        column_c0 = null;

        table_t0 = null;

        out.println("       [ok]");
    }

    protected void initNdbjtieBuffers() {
        assert (column_c0 != null);
        assert (bb_r == null);

        out.print("allocating buffers...");
        out.flush();

        final int width_row = (
            + width_c0
            + width_c1
            + width_c2
            + width_c3
            + width_c4
            + width_c5
            + width_c6
            + width_c7
            + width_c8
            + width_c9
            + width_c10
            + width_c11
            + width_c12
            + width_c13
            + width_c14);

        //bb_r = ByteBuffer.allocateDirect(width_row);
        bb_r = ByteBuffer.allocateDirect(width_row * nRows);

        // initial order of a byte buffer is always BIG_ENDIAN
        bb_r.order(bo);

        out.println("            [ok]");
    }

    protected void closeNdbjtieBuffers() {
        assert (column_c0 != null);
        assert (bb_r != null);

        out.print("releasing buffers...");
        out.flush();

        bb_r = null;

        out.println("             [ok]");
    }

    static protected void loadSystemLibrary(String name) {
        out.print("loading native libary ...");
        out.flush();
        try {
            System.loadLibrary(name);
        } catch (UnsatisfiedLinkError e) {
            String path;
            try {
                path = System.getProperty("java.library.path");
            } catch (Exception ex) {
                path = "<exception caught: " + ex.getMessage() + ">";
            }
            err.println("NdbBase: failed loading library '"
                        + name + "'; java.library.path='" + path + "'");
            throw e;
        } catch (SecurityException e) {
            err.println("NdbBase: failed loading library '"
                        + name + "'; caught exception: " + e);
            throw e;
        }
        out.println("        [ok: " + name + "]");
    }

    static protected String toStr(NdbErrorConst e) {
        return "NdbError[" + e.code() + "]: " + e.message();
    }

    static protected int ndbjtieColumnWidth(ColumnConst c) {
        final int s = c.getSize(); // size of type or of base type
        final int al = c.getLength(); // length or max length, 1 for scalars
        final int at = c.getArrayType(); // size of length prefix, practically
        return (s * al) + at;
    }

    // ----------------------------------------------------------------------

    enum XMode { SINGLE, BULK, BATCH }

    public void run() throws SQLException {
        for (int i = 0; i < nRuns; i++) {
            if (doJdbc) {
                out.println();
                out.println("handle " + nRows + " rows by JDBC ...");
                if (doSingle) {
                    out.println();
                    if (doInsert) runJdbcInsert(XMode.SINGLE);
                    if (doLookup) runJdbcLookup(XMode.SINGLE);
                    if (doUpdate) runJdbcUpdate(XMode.SINGLE);
                    if (doDelete) runJdbcDelete(XMode.SINGLE);
                }
                if (doBulk) {
                    out.println();
                    if (doInsert) runJdbcInsert(XMode.BULK);
                    if (doLookup) runJdbcLookup(XMode.BULK);
                    if (doUpdate) runJdbcUpdate(XMode.BULK);
                    if (doDelete) runJdbcDelete(XMode.BULK);
                }
                if (doBatch) {
                    out.println();
                    if (doInsert) runJdbcInsert(XMode.BATCH);
                    if (doLookup) runJdbcLookup(XMode.BATCH);
                    if (doUpdate) runJdbcUpdate(XMode.BATCH);
                    if (doDelete) runJdbcDelete(XMode.BATCH);
                }
            }
            if (doClusterj) {
                out.println();
                out.println("handle " + nRows + " rows by ClusterJ ...");
                if (doSingle) {
                    out.println();
                    if (doInsert) runClusterjInsert(XMode.SINGLE);
                    if (doLookup) runClusterjLookup(XMode.SINGLE);
                    if (doUpdate) runClusterjUpdate(XMode.SINGLE);
                    if (doDelete) runClusterjDelete(XMode.SINGLE);
                }
                if (doBulk) {
                    out.println();
                    if (doInsert) runClusterjInsert(XMode.BULK);
                    if (doLookup) runClusterjLookup(XMode.BULK);
                    if (doUpdate) runClusterjUpdate(XMode.BULK);
                    if (doDelete) runClusterjDelete(XMode.BULK);
                }
            }
            if (doNdbjtie) {
                out.println();
                out.println("handle " + nRows + " rows by NDB JTie ...");
                if (doSingle) {
                    out.println();
                    if (doInsert) runNdbjtieInsert(XMode.SINGLE);
                    if (doLookup) runNdbjtieLookup(XMode.SINGLE);
                    if (doUpdate) runNdbjtieUpdate(XMode.SINGLE);
                    if (doDelete) runNdbjtieDelete(XMode.SINGLE);
                }
                if (doBulk) {
                    out.println();
                    if (doInsert) runNdbjtieInsert(XMode.BULK);
                    if (doLookup) runNdbjtieLookup(XMode.BULK);
                    if (doUpdate) runNdbjtieUpdate(XMode.BULK);
                    if (doDelete) runNdbjtieDelete(XMode.BULK);
                }
                if (doBatch) {
                    out.println();
                    if (doInsert) runNdbjtieInsert(XMode.BATCH);
                    if (doLookup) runNdbjtieLookup(XMode.BATCH);
                    if (doUpdate) runNdbjtieUpdate(XMode.BATCH);
                    if (doDelete) runNdbjtieDelete(XMode.BATCH);
                }
            }
        }
    }

    // ----------------------------------------------------------------------

    public void runJdbcInsert(XMode mode) throws SQLException {
        final String m = mode.toString().toLowerCase();
        //out.println("insert " + nRows + " rows by JDBC " + m + " tx ...");

        long time = -System.currentTimeMillis();
        connection.setAutoCommit(mode == XMode.SINGLE);
        for(int i = 0; i < nRows; i++) {
            jdbcInsert(i, mode);
        }
        if (mode == XMode.BATCH)
            ins0.executeBatch();
        if (mode != XMode.SINGLE)
            connection.commit();
        time += System.currentTimeMillis();

        out.println("jdbc_insert_" + m + "    \t: " + time + " ms");
    }

    public void runClusterjInsert(XMode mode) {
        final String m = mode.toString().toLowerCase();
        //out.println("insert " + nRows + " rows by ClusterJ " + m + " tx ...");

        long time = -System.currentTimeMillis();
        if (mode != XMode.SINGLE)
            session.currentTransaction().begin();
        for(int i = 0; i < nRows; i++) {
            clusterjInsert(i);
        }
        if (mode != XMode.SINGLE)
            session.currentTransaction().commit();
        time += System.currentTimeMillis();

        out.println("clusterj_insert_" + m + "  \t: " + time + " ms");
    }

    public void runNdbjtieInsert(XMode mode) {
        final String m = mode.toString().toLowerCase();
        //out.println("insert " + nRows + " rows by NDB JTie " + m + " tx ...");

        long time = -System.currentTimeMillis();
        if (mode == XMode.SINGLE) {
            for(int i = 0; i < nRows; i++) {
                ndbjtieBeginTransaction();
                ndbjtieInsert(i);
                ndbjtieCommitTransaction();
                ndbjtieCloseTransaction();
            }
        } else {
            ndbjtieBeginTransaction();
            for(int i = 0; i < nRows; i++) {
                ndbjtieInsert(i);

                if (mode == XMode.BULK)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            ndbjtieCloseTransaction();
        }
        time += System.currentTimeMillis();

        out.println("ndbjtie_insert_" + m + "  \t: " + time + " ms");
    }

    public void jdbcInsert(int c0, XMode mode) {
        // include exception handling as part of jdbc pattern
        try {
            final int i = c0;
            final String str = Integer.toString(i);
            ins0.setString(1, str); // key
            ins0.setString(2, str);
            ins0.setInt(3, i);
            ins0.setInt(4, i);
            ins0.setString(5, str);
            ins0.setString(6, str);
            ins0.setString(7, str);
            ins0.setString(8, str);
            if (mode == XMode.BATCH) {
                ins0.addBatch();
            } else {
                int cnt = ins0.executeUpdate();
                assert (cnt == 1);
            }
        } catch (SQLException e) {
            throw new RuntimeException(e);
        }
    }

    public void clusterjInsert(int c0) {
        final CJSubscriber o = session.newInstance(CJSubscriber.class);
        final int i = c0;
        final String str = Integer.toString(i);
        //final String oneChar = Integer.toString(1);
        o.setC0(str);
        o.setC1(str);
        o.setC2(i);
        o.setC3(i);
        //o.setC4(i);
        o.setC5(str);
        o.setC6(str);
        o.setC7(str);
        o.setC8(str);
        //o.setC9(oneChar);
        //o.setC10(oneChar);
        //o.setC11(str);
        //o.setC12(str);
        //o.setC13(oneChar);
        //o.setC14(str);
        session.persist(o);
    }

    protected void ndbjtieInsert(int c0) {
        // get an insert operation for the table
        NdbOperation op = tx.getNdbOperation(table_t0);
        if (op == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if (op.insertTuple() != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));

        // include exception handling as part of transcoding pattern
        final int i = c0;
        final CharBuffer str = CharBuffer.wrap(Integer.toString(i));
        try {
            // set values; key attribute needs to be set first
            //str.rewind();
            ndbjtieTranscode(bb_r, str);
            if (op.equal(attr_c0, bb_r) != 0) // key
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c0);

            str.rewind();
            ndbjtieTranscode(bb_r, str);
            if (op.setValue(attr_c1, bb_r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c1);

            if (op.setValue(attr_c2, i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            if (op.setValue(attr_c3, i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            str.rewind();
            ndbjtieTranscode(bb_r, str);
            if (op.setValue(attr_c5, bb_r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c5);

            str.rewind();
            ndbjtieTranscode(bb_r, str);
            if (op.setValue(attr_c6, bb_r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c6);

            str.rewind();
            ndbjtieTranscode(bb_r, str);
            if (op.setValue(attr_c7, bb_r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c7);

            str.rewind();
            ndbjtieTranscode(bb_r, str);
            if (op.setValue(attr_c8, bb_r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c8);
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    public void runJdbcLookup(XMode mode) throws SQLException {
        final String m = mode.toString().toLowerCase();
        //out.println("lookup " + nRows + " rows by JDBC " + m + " tx ...");

        if(mode == XMode.BATCH) {
            out.println("jdbc_lookup_" + m + "    \t: " + 0 + " n/a");
            return;
        }
        
        long time = -System.currentTimeMillis();
        connection.setAutoCommit(mode == XMode.SINGLE);
        for(int i = 0; i < nRows; i++) {
            jdbcLookup(i);
        }
        if (mode != XMode.SINGLE)
            connection.commit();
        time += System.currentTimeMillis();

        out.println("jdbc_lookup_" + m + "    \t: " + time + " ms");
    }

    public void runClusterjLookup(XMode mode) {
        final String m = mode.toString().toLowerCase();
        //out.println("lookup " + nRows + " rows by ClusterJ " + m + " tx ...");

        long time = -System.currentTimeMillis();
        if (mode != XMode.SINGLE)
            session.currentTransaction().begin();
        for(int i = 0; i < nRows; i++) {
            clusterjLookup(i);
        }
        if (mode != XMode.SINGLE)
            session.currentTransaction().commit();
        time += System.currentTimeMillis();

        out.println("clusterj_lookup_" + m + "  \t: " + time + " ms");
    }

    public void runNdbjtieLookup(XMode mode) {
        final String m = mode.toString().toLowerCase();
        //out.println("lookup " + nRows + " rows by NDB JTie " + m + " tx ...");

        long time = -System.currentTimeMillis();
        if (mode == XMode.SINGLE) {
            for(int i = 0; i < nRows; i++) {
                ndbjtieBeginTransaction();
                ndbjtieLookup(i);
                ndbjtieCommitTransaction();
                ndbjtieCloseTransaction();
                ndbjtieRead(i);
            }
        } else {
            ndbjtieBeginTransaction();
            for(int i = 0; i < nRows; i++) {
                ndbjtieLookup(i);

                if (mode == XMode.BULK)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            ndbjtieCloseTransaction();
            for(int i = 0; i < nRows; i++) {
                ndbjtieRead(i);
            }
        }
        time += System.currentTimeMillis();

        out.println("ndbjtie_lookup_" + m + "  \t: " + time + " ms");
    }

    public void jdbcLookup(int c0) {
        // include exception handling as part of jdbc pattern
        try {
            sel0.setString(1, Integer.toString(c0)); // key
            ResultSet resultSet = sel0.executeQuery();

            if (resultSet.next()) {
                // not verifying at this time
                String ac0 = resultSet.getString(1);
                String c1 = resultSet.getString(2);
                int c2 = resultSet.getInt(3);
                int c3 = resultSet.getInt(4);
                int c4 = resultSet.getInt(5);
                String c5 = resultSet.getString(6);
                String c6 = resultSet.getString(7);
                String c7 = resultSet.getString(8);
                String c8 = resultSet.getString(9);
                String c9 = resultSet.getString(10);
                String c10 = resultSet.getString(11);
                String c11 = resultSet.getString(12);
                String c12 = resultSet.getString(13);
                String c13 = resultSet.getString(14);
                String c14 = resultSet.getString(15);
            }
            assert (!resultSet.next());

            resultSet.close();
        } catch (SQLException e) {
            throw new RuntimeException(e);
        }
    }

    public void clusterjLookup(int c0) {
        final CJSubscriber o
            = session.find(CJSubscriber.class, Integer.toString(c0));
        if (o != null) {
            // not verifying at this time
            String ac0 = o.getC0();
            String c1 = o.getC1();
            int c2 = o.getC2();
            int c3 = o.getC3();
            int c4 = o.getC4();
            String c5 = o.getC5();
            String c6 = o.getC6();
            String c7 = o.getC7();
            String c8 = o.getC8();
            String c9 = o.getC9();
            String c10 = o.getC10();
            String c11 = o.getC11();
            String c12 = o.getC12();
            String c13 = o.getC13();
            String c14 = o.getC14();
        }
    }

    protected void ndbjtieLookup(int c0) {
        // get a lookup operation for the table
        NdbOperation op = tx.getNdbOperation(table_t0);
        if (op == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if (op.readTuple(ndbOpLockMode) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));

        int p = bb_r.position();

        // include exception handling as part of transcoding pattern
        final CharBuffer str = CharBuffer.wrap(Integer.toString(c0));
        try {
            // set values; key attribute needs to be set first
            //str.rewind();
            ndbjtieTranscode(bb_r, str);
            if (op.equal(attr_c0, bb_r) != 0) // key
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(p += width_c0);
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }

        // get attributes (not readable until after commit)
        if (op.getValue(attr_c1, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c1);
        if (op.getValue(attr_c2, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c2);
        if (op.getValue(attr_c3, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c3);
        if (op.getValue(attr_c4, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c4);
        if (op.getValue(attr_c5, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c5);
        if (op.getValue(attr_c6, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c6);
        if (op.getValue(attr_c7, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c7);
        if (op.getValue(attr_c8, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c8);
        if (op.getValue(attr_c9, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c9);
        if (op.getValue(attr_c10, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c10);
        if (op.getValue(attr_c11, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c11);
        if (op.getValue(attr_c12, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c12);
        if (op.getValue(attr_c13, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c13);
        if (op.getValue(attr_c14, bb_r) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb_r.position(p += width_c14);
    }

    protected void ndbjtieRead(int c0) {
        // include exception handling as part of transcoding pattern
        final int i = c0;
        final CharBuffer str = CharBuffer.wrap(Integer.toString(i));
        assert (str.position() == 0);

        try {
            int p = bb_r.position();
            bb_r.position(p += width_c0);

            // not verifying at this time
            // (str.equals(ndbjtieTranscode(bb_c1)));
            // (i == bb_c2.asIntBuffer().get());
            CharBuffer y = ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c1);

            bb_r.asIntBuffer().get();
            bb_r.position(p += width_c2);
            bb_r.asIntBuffer().get();
            bb_r.position(p += width_c3);
            bb_r.asIntBuffer().get();
            bb_r.position(p += width_c4);

            ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c5);
            ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c6);
            ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c7);
            ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c8);
            ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c9);
            ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c10);
            ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c11);
            ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c12);
            ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c13);
            ndbjtieTranscode(bb_r);
            bb_r.position(p += width_c14);
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    public void runJdbcUpdate(XMode mode) throws SQLException {
        final String m = mode.toString().toLowerCase();
        //out.println("update " + nRows + " rows by JDBC " + m + " tx ...");

        long time = -System.currentTimeMillis();
        connection.setAutoCommit(mode == XMode.SINGLE);
        for(int i = 0; i < nRows; i++) {
            jdbcUpdate(i, mode);
        }
        if (mode == XMode.BATCH)
            upd0.executeBatch();
        if (mode != XMode.SINGLE)
            connection.commit();
        time += System.currentTimeMillis();

        out.println("jdbc_update_" + m + "    \t: " + time + " ms");
    }

    public void runClusterjUpdate(XMode mode) {
        final String m = mode.toString().toLowerCase();
        //out.println("update " + nRows + " rows by ClusterJ " + m + " tx ...");

        long time = -System.currentTimeMillis();
        if (mode != XMode.SINGLE)
            session.currentTransaction().begin();
        for(int i = 0; i < nRows; i++) {
            clusterjUpdate(i);
        }
        if (mode != XMode.SINGLE)
            session.currentTransaction().commit();
        time += System.currentTimeMillis();

        out.println("clusterj_update_" + m + "  \t: " + time + " ms");
    }

    public void runNdbjtieUpdate(XMode mode) {
        final String m = mode.toString().toLowerCase();
        //out.println("update " + nRows + " rows by NDB JTie " + m + " tx ...");

        long time = -System.currentTimeMillis();
        if (mode == XMode.SINGLE) {
            for(int i = 0; i < nRows; i++) {
                ndbjtieBeginTransaction();
                ndbjtieUpdate(i);
                ndbjtieCommitTransaction();
                ndbjtieCloseTransaction();
            }
        } else {
            ndbjtieBeginTransaction();
            for(int i = 0; i < nRows; i++) {
                ndbjtieUpdate(i);

                if (mode == XMode.BULK)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            ndbjtieCloseTransaction();
        }
        time += System.currentTimeMillis();

        out.println("ndbjtie_update_" + m + "  \t: " + time + " ms");
    }

    public void jdbcUpdate(int c0, XMode mode) {
        final String str0 = Integer.toString(c0);
        final int r = -c0;
        final String str1 = Integer.toString(r);

        // include exception handling as part of jdbc pattern
        try {
            upd0.setString(1, str1);
            upd0.setInt(2, r);
            upd0.setInt(3, r);
            upd0.setString(4, str1);
            upd0.setString(5, str1);
            upd0.setString(6, str1);
            upd0.setString(7, str1);
            upd0.setString(8, str0); // key
            if (mode == XMode.BATCH) {
                upd0.addBatch();
            } else {
                int cnt = upd0.executeUpdate();
                assert (cnt == 1);
            }
        } catch (SQLException e) {
            throw new RuntimeException(e);
        }
    }

    public void clusterjUpdate(int c0) {
        final String str0 = Integer.toString(c0);
        final int r = -c0;
        final String str1 = Integer.toString(r);

        // blind update
        final CJSubscriber o = session.newInstance(CJSubscriber.class);
        o.setC0(str0);
        //final CJSubscriber o = session.find(CJSubscriber.class, str0);
        //String oneChar = Integer.toString(2);
        o.setC1(str1);
        o.setC2(r);
        o.setC3(r);
        //o.setC4(r);
        o.setC5(str1);
        o.setC6(str1);
        o.setC7(str1);
        o.setC8(str1);
        //o.setC9(oneChar);
        //o.setC10(oneChar);
        //o.setC11(str);
        //o.setC12(str);
        //o.setC13(oneChar);
        //o.setC14(str);
        session.updatePersistent(o);
    }

    protected void ndbjtieUpdate(int c0) {
        final CharBuffer str0 = CharBuffer.wrap(Integer.toString(c0));
        final int r = -c0;
        final CharBuffer str1 = CharBuffer.wrap(Integer.toString(r));

        // get an update operation for the table
        NdbOperation op = tx.getNdbOperation(table_t0);
        if (op == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if (op.updateTuple() != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));

        // include exception handling as part of transcoding pattern
        try {
            // set values; key attribute needs to be set first
            //str0.rewind();
            ndbjtieTranscode(bb_r, str0);
            if (op.equal(attr_c0, bb_r) != 0) // key
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c0);

            //str1.rewind();
            ndbjtieTranscode(bb_r, str1);
            if (op.setValue(attr_c1, bb_r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c1);

            if (op.setValue(attr_c2, r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            if (op.setValue(attr_c3, r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            str1.rewind();
            ndbjtieTranscode(bb_r, str1);
            if (op.setValue(attr_c5, bb_r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c5);

            str1.rewind();
            ndbjtieTranscode(bb_r, str1);
            if (op.setValue(attr_c6, bb_r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c6);

            str1.rewind();
            ndbjtieTranscode(bb_r, str1);
            if (op.setValue(attr_c7, bb_r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c7);

            str1.rewind();
            ndbjtieTranscode(bb_r, str1);
            if (op.setValue(attr_c8, bb_r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(bb_r.position() + width_c8);
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    public void runJdbcDelete(XMode mode) throws SQLException {
        final String m = mode.toString().toLowerCase();
        //out.println("delete " + nRows + " rows by JDBC " + m + " tx ...");

        long time = -System.currentTimeMillis();
        connection.setAutoCommit(mode == XMode.SINGLE);
        for(int i = 0; i < nRows; i++) {
            jdbcDelete(i, mode);
        }
        if (mode == XMode.BATCH)
            del0.executeBatch();
        if (mode != XMode.SINGLE)
            connection.commit();
        time += System.currentTimeMillis();

        out.println("jdbc_delete_" + m + "    \t: " + time + " ms");
    }

    public void runClusterjDelete(XMode mode) {
        final String m = mode.toString().toLowerCase();
        //out.println("delete " + nRows + " rows by ClusterJ " + m + " tx ...");

        long time = -System.currentTimeMillis();
        if (mode != XMode.SINGLE)
            session.currentTransaction().begin();
        for(int i = 0; i < nRows; i++) {
            clusterjDelete(i);
        }
        if (mode != XMode.SINGLE)
            session.currentTransaction().commit();
        time += System.currentTimeMillis();

        out.println("clusterj_delete_" + m + "  \t: " + time + " ms");
    }

    public void runNdbjtieDelete(XMode mode) {
        final String m = mode.toString().toLowerCase();
        //out.println("delete " + nRows + " rows by NDB JTie " + m + " tx ...");

        long time = -System.currentTimeMillis();
        if (mode == XMode.SINGLE) {
            for(int i = 0; i < nRows; i++) {
                ndbjtieBeginTransaction();
                ndbjtieDelete(i);
                ndbjtieCommitTransaction();
                ndbjtieCloseTransaction();
            }
        } else {
            ndbjtieBeginTransaction();
            for(int i = 0; i < nRows; i++) {
                ndbjtieDelete(i);

                if (mode == XMode.BULK)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            ndbjtieCloseTransaction();
        }
        time += System.currentTimeMillis();

        out.println("ndbjtie_delete_" + m + "  \t: " + time + " ms");
    }

    public void jdbcDelete(int c0, XMode mode) {
        // include exception handling as part of jdbc pattern
        try {
            final String str = Integer.toString(c0);
            del0.setString(1, str);
            if (mode == XMode.BATCH) {
                del0.addBatch();
            } else {
                int cnt = del0.executeUpdate();
                assert (cnt == 1);
            }
        } catch (SQLException e) {
            throw new RuntimeException(e);
        }
    }

    public void clusterjDelete(int c0) {
        // can do a blind delete
        final CJSubscriber o = session.newInstance(CJSubscriber.class);
        o.setC0(Integer.toString(c0));
        assert o != null;
        session.remove(o);
    }

    protected void ndbjtieDelete(int c0) {
        // get an delete operation for the table
        NdbOperation op = tx.getNdbOperation(table_t0);
        if (op == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if (op.deleteTuple() != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));

        int p = bb_r.position();

        // include exception handling as part of transcoding pattern
        final int i = c0;
        final CharBuffer str = CharBuffer.wrap(Integer.toString(c0));
        try {
            // set values; key attribute needs to be set first
            //str.rewind();
            ndbjtieTranscode(bb_r, str);
            if (op.equal(attr_c0, bb_r) != 0) // key
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb_r.position(p += width_c0);
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void ndbjtieBeginTransaction() {
        assert (tx == null);

        // prepare buffer for writing
        bb_r.clear();

        // start a transaction
        // must be closed with NdbTransaction.close
        final TableConst table  = null;
        final ByteBuffer keyData = null;
        final int keyLen = 0;
        if ((tx = ndb.startTransaction(table, keyData, keyLen)) == null)
            throw new RuntimeException(toStr(ndb.getNdbError()));
    }

    protected void ndbjtieExecuteTransaction() {
        assert (tx != null);

        // execute but don't commit the current transaction
        final int execType = NdbTransaction.ExecType.NoCommit;
        final int abortOption = NdbOperation.AbortOption.AbortOnError;
        final int force = 0;
        if (tx.execute(execType, abortOption, force) != 0
            || tx.getNdbError().status() != NdbError.Status.Success)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void ndbjtieCommitTransaction() {
        assert (tx != null);

        // commit the current transaction
        final int execType = NdbTransaction.ExecType.Commit;
        final int abortOption = NdbOperation.AbortOption.AbortOnError;
        final int force = 0;
        if (tx.execute(execType, abortOption, force) != 0
            || tx.getNdbError().status() != NdbError.Status.Success)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void ndbjtieRollbackTransaction() {
        assert (tx != null);

        // abort the current transaction
        final int execType = NdbTransaction.ExecType.Rollback;
        final int abortOption = NdbOperation.AbortOption.DefaultAbortOption;
        final int force = 0;
        if (tx.execute(execType, abortOption, force) != 0
            || tx.getNdbError().status() != NdbError.Status.Success)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void ndbjtieCloseTransaction() {
        assert (tx != null);

        // close the current transaction (required after commit, rollback)
        ndb.closeTransaction(tx);
        tx = null;

        // prepare buffer for reading
        bb_r.rewind();
    }

    // ----------------------------------------------------------------------

    protected CharBuffer ndbjtieTranscode(ByteBuffer from)
        throws CharacterCodingException {
        // mark position
        final int p = from.position();

        // read 1-byte length prefix
        final int l = from.get();
        assert ((0 <= l) && (l < 256)); // or (l <= 256)?

        // prepare buffer for reading
        from.limit(from.position() + l);

        // decode
        final CharBuffer to = csDecoder.decode(from);
        assert (!from.hasRemaining());

        // allow for repositioning
        from.limit(from.capacity());

        assert (to.position() == 0);
        assert (to.limit() == to.capacity());
        return to;
    }

    protected void ndbjtieTranscode(ByteBuffer to, CharBuffer from)
        throws CharacterCodingException {
        // mark position
        final int p = to.position();

        // advance 1-byte length prefix
        to.position(p + 1);
        //to.put((byte)0);

        // encode
        final boolean endOfInput = true;
        final CoderResult cr = csEncoder.encode(from, to, endOfInput);
        if (!cr.isUnderflow())
            cr.throwException();
        assert (!from.hasRemaining());

        // write 1-byte length prefix
        final int l = (to.position() - p) - 1;
        assert (0 <= l && l < 256); // or (l <= 256)?
        to.put(p, (byte)l);

        // reset position
        to.position(p);
    }

    // ----------------------------------------------------------------------

    @PersistenceCapable(table="mytable")
    //@Index(name="c0_UNIQUE")
    static public interface CJSubscriber
    {
        @PrimaryKey
        String getC0();
        void setC0(String c0);

        @Index(name="c1_UNIQUE")
        String getC1();
        void setC1(String c1);

        @Index(name="c2_UNIQUE")
        int getC2();
        void setC2(int c2);

        int getC3();
        void setC3(int c3);

        int getC4();
        void setC4(int c4);

        String getC5();
        void setC5(String c5);

        String getC6();
        void setC6(String c6);

        @Index(name="c7_UNIQUE")
        String getC7();
        void setC7(String c7);

        @Index(name="c8_UNIQUE")
        String getC8();
        void setC8(String c8);

        String getC9();
        void setC9(String c9);

        String getC10();
        void setC10(String c10);

        String getC11();
        void setC11(String c11);

        String getC12();
        void setC12(String c12);

        String getC13();
        void setC13(String c13);

        String getC14();
        void setC14(String c14);
    }

    // ----------------------------------------------------------------------
}