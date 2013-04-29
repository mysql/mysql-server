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


class NdbjtieTwsLoad extends TwsLoad {

    // NDB settings
    protected String mgmdConnect;
    protected String catalog;
    protected String schema;

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
    protected int width_row; // sum of {width_c0 .. width_c14}

    // NDB JTie data resources
    protected ByteBuffer bb;

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

    public NdbjtieTwsLoad(TwsDriver driver) {
        super(driver);
    }

    // ----------------------------------------------------------------------
    // NDB Base intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        out.println();
        out.print("setting ndb properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        // the hostname and port number of NDB mgmd
        mgmdConnect = driver.props.getProperty("ndb.mgmdConnect", "localhost");
        assert mgmdConnect != null;

        // the database
        catalog = driver.props.getProperty("ndb.catalog", "crunddb");
        assert catalog != null;

        // the schema
        schema = driver.props.getProperty("ndb.schema", "def");
        assert schema != null;

        if (msg.length() == 0) {
            out.println("      [ok]");
        } else {
            out.println();
            out.print(msg.toString());
        }

        // have mgmdConnect initialized first
        descr = "ndbjtie(" + mgmdConnect + ")";
    }

    protected void printProperties() {
        out.println("ndb.mgmdConnect:                \"" + mgmdConnect + "\"");
        out.println("ndb.catalog:                    \"" + catalog + "\"");
        out.println("ndb.schema:                     \"" + schema + "\"");
    }

    public void init() throws Exception {
        super.init();
        assert mgmd == null;

        // load native library (better diagnostics doing it explicitely)
        Driver.loadSystemLibrary("ndbclient");

        // instantiate NDB cluster singleton
        out.print("creating cluster connection ...");
        out.flush();
        mgmd = Ndb_cluster_connection.create(mgmdConnect);
        assert mgmd != null;
        out.println(" [ok: mgmd@" + mgmdConnect + "]");
    }

    public void close() throws Exception {
        assert mgmd != null;

        out.println();
        out.print("closing cluster connection ...");
        out.flush();
        Ndb_cluster_connection.delete(mgmd);
        mgmd = null;
        out.println("  [ok]");

        super.close();
    }

    // ----------------------------------------------------------------------
    // NDB JTie datastore operations
    // ----------------------------------------------------------------------

    public void initConnection() {
        assert (mgmd != null);
        assert (ndb == null);

        out.println();
        out.println("initializing ndbjtie resources ...");

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
        out.println("       [ok: " + mgmdConnect + "]");

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
        out.println("      [ok]");

        // connect to database
        out.print("connecting to database ...");
        ndb = Ndb.create(mgmd, catalog, schema);
        final int max_no_tx = 10; // maximum number of parallel tx (<=1024)
        // note each scan or index scan operation uses one extra transaction
        if (ndb.init(max_no_tx) != 0) {
            String msg = "Error caught: " + ndb.getNdbError().message();
            throw new RuntimeException(msg);
        }
        out.println("      [ok: " + catalog + "." + schema + "]");

        initNdbjtieModel();

        initNdbjtieBuffers();

        out.print("using lock mode for reads ...");
        out.flush();
        final String lm;
        switch (driver.lockMode) {
        case READ_COMMITTED:
            ndbOpLockMode = NdbOperation.LockMode.LM_CommittedRead;
            lm = "LM_CommittedRead";
            break;
        case SHARED:
            ndbOpLockMode = NdbOperation.LockMode.LM_Read;
            lm = "LM_Read";
            break;
        case EXCLUSIVE:
            ndbOpLockMode = NdbOperation.LockMode.LM_Exclusive;
            lm = "LM_Exclusive";
            break;
        default:
            ndbOpLockMode = NdbOperation.LockMode.LM_CommittedRead;
            lm = "LM_CommittedRead";
            assert false;
        }
        out.println("   [ok: " + lm + "]");
    }

    public void closeConnection() {
        assert (ndb != null);

        out.println();
        out.println("releasing ndbjtie resources ...");

        closeNdbjtieBuffers();

        closeNdbjtieModel();

        out.print("closing database connection ...");
        out.flush();
        Ndb.delete(ndb);
        ndb = null;
        out.println(" [ok]");
    }

    protected void initNdbjtieModel() {
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

        width_row = (
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

        out.println("            [ok]");
    }

    protected void closeNdbjtieModel() {
        assert (ndb != null);
        assert (table_t0 != null);
        assert (column_c0 != null);

        out.print("clearing metadata cache...");
        out.flush();

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

        out.println("      [ok]");
    }

    public void initNdbjtieBuffers() {
        assert (column_c0 != null);
        assert (bb == null);

        out.print("allocating buffers...");
        out.flush();

        bb = ByteBuffer.allocateDirect(width_row * driver.nRows);

        // initial order of a byte buffer is always BIG_ENDIAN
        bb.order(bo);

        out.println("           [ok]");
    }

    protected void closeNdbjtieBuffers() {
        assert (column_c0 != null);
        assert (bb != null);

        out.print("releasing buffers...");
        out.flush();

        bb = null;

        out.println("            [ok]");
    }

    static protected String toStr(NdbErrorConst e) {
        return "NdbError[" + e.code() + "]: " + e.message();
    }

    static protected int ndbjtieColumnWidth(ColumnConst c) {
        int s = c.getSize(); // size of type or of base type
        int al = c.getLength(); // length or max length, 1 for scalars
        int at = c.getArrayType(); // size of length prefix, practically
        return (s * al) + at;
    }

    // ----------------------------------------------------------------------

    public void runOperations() {
        out.println();
        out.println("running NDB JTie operations ..."
                    + " [nRows=" + driver.nRows + "]");

        if (driver.doBulk) {
            if (driver.doInsert) runNdbjtieInsert(TwsDriver.XMode.BULK);
            if (driver.doLookup) runNdbjtieLookup(TwsDriver.XMode.BULK);
            if (driver.doUpdate) runNdbjtieUpdate(TwsDriver.XMode.BULK);
            if (driver.doDelete) runNdbjtieDelete(TwsDriver.XMode.BULK);
        }
        if (driver.doEach) {
            if (driver.doInsert) runNdbjtieInsert(TwsDriver.XMode.EACH);
            if (driver.doLookup) runNdbjtieLookup(TwsDriver.XMode.EACH);
            if (driver.doUpdate) runNdbjtieUpdate(TwsDriver.XMode.EACH);
            if (driver.doDelete) runNdbjtieDelete(TwsDriver.XMode.EACH);
        }
        if (driver.doIndy) {
            if (driver.doInsert) runNdbjtieInsert(TwsDriver.XMode.INDY);
            if (driver.doLookup) runNdbjtieLookup(TwsDriver.XMode.INDY);
            if (driver.doUpdate) runNdbjtieUpdate(TwsDriver.XMode.INDY);
            if (driver.doDelete) runNdbjtieDelete(TwsDriver.XMode.INDY);
        }
    }

    // ----------------------------------------------------------------------

    protected void runNdbjtieInsert(TwsDriver.XMode mode) {
        final String name = "insert_" + mode.toString().toLowerCase();
        driver.beginOp(name);

        if (mode == TwsDriver.XMode.INDY) {
            for(int i = 0; i < driver.nRows; i++) {
                ndbjtieBeginTransaction();
                ndbjtieInsert(i);
                ndbjtieCommitTransaction();
                ndbjtieCloseTransaction();
            }
        } else {
            ndbjtieBeginTransaction();
            for(int i = 0; i < driver.nRows; i++) {
                ndbjtieInsert(i);

                if (mode == TwsDriver.XMode.EACH)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            ndbjtieCloseTransaction();
        }

        driver.finishOp(name, driver.nRows);
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
            ndbjtieTranscode(bb, str);
            if (op.equal(attr_c0, bb) != 0) // key
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c0);

            str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.setValue(attr_c1, bb) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c1);

            if (op.setValue(attr_c2, i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            if (op.setValue(attr_c3, i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // XXX
            if (op.setValue(attr_c4, null) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.setValue(attr_c5, bb) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c5);

            str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.setValue(attr_c6, bb) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c6);

            str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.setValue(attr_c7, bb) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c7);

            str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.setValue(attr_c8, bb) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c8);

            // XXX
            if (op.setValue(attr_c9, null) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            if (op.setValue(attr_c10, null) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            if (op.setValue(attr_c11, null) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            if (op.setValue(attr_c12, null) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            if (op.setValue(attr_c13, null) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            if (op.setValue(attr_c14, null) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void runNdbjtieLookup(TwsDriver.XMode mode) {
        final String name = "lookup_" + mode.toString().toLowerCase();
        driver.beginOp(name);

        if (mode == TwsDriver.XMode.INDY) {
            for(int i = 0; i < driver.nRows; i++) {
                ndbjtieBeginTransaction();
                ndbjtieLookup(i);
                ndbjtieCommitTransaction();
                ndbjtieRead(i);
                ndbjtieCloseTransaction();
            }
        } else {
            ndbjtieBeginTransaction();
            for(int i = 0; i < driver.nRows; i++) {
                ndbjtieLookup(i);

                if (mode == TwsDriver.XMode.EACH)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            for(int i = 0; i < driver.nRows; i++) {
                ndbjtieRead(i);
            }
            ndbjtieCloseTransaction();
        }

        driver.finishOp(name, driver.nRows);
    }

    protected void ndbjtieLookup(int c0) {
        // get a lookup operation for the table
        NdbOperation op = tx.getNdbOperation(table_t0);
        if (op == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if (op.readTuple(ndbOpLockMode) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));

        int p = bb.position();

        // include exception handling as part of transcoding pattern
        final CharBuffer str = CharBuffer.wrap(Integer.toString(c0));
        try {
            // set values; key attribute needs to be set first
            //str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.equal(attr_c0, bb) != 0) // key
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(p += width_c0);
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }

        // get attributes (not readable until after commit)
        if (op.getValue(attr_c1, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c1);
        if (op.getValue(attr_c2, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c2);
        if (op.getValue(attr_c3, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c3);
        if (op.getValue(attr_c4, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c4);
        if (op.getValue(attr_c5, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c5);
        if (op.getValue(attr_c6, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c6);
        if (op.getValue(attr_c7, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c7);
        if (op.getValue(attr_c8, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c8);
        if (op.getValue(attr_c9, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c9);
        if (op.getValue(attr_c10, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c10);
        if (op.getValue(attr_c11, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c11);
        if (op.getValue(attr_c12, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c12);
        if (op.getValue(attr_c13, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c13);
        if (op.getValue(attr_c14, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += width_c14);
    }

    protected void ndbjtieRead(int c0) {
        // include exception handling as part of transcoding pattern
        //final int i = c0;
        //final CharBuffer str = CharBuffer.wrap(Integer.toString(i));
        //assert (str.position() == 0);

        try {
            int p = bb.position();
            bb.position(p += width_c0);

            // not verifying at this time
            // (str.equals(ndbjtieTranscode(bb_c1)));
            // (i == bb_c2.asIntBuffer().get());
            //CharBuffer y = ndbjtieTranscode(bb);

            ndbjtieTranscode(bb);
            bb.position(p += width_c1);

            bb.asIntBuffer().get();
            bb.position(p += width_c2);
            bb.asIntBuffer().get();
            bb.position(p += width_c3);
            bb.asIntBuffer().get();
            bb.position(p += width_c4);

            ndbjtieTranscode(bb);
            bb.position(p += width_c5);
            ndbjtieTranscode(bb);
            bb.position(p += width_c6);
            ndbjtieTranscode(bb);
            bb.position(p += width_c7);
            ndbjtieTranscode(bb);
            bb.position(p += width_c8);
            ndbjtieTranscode(bb);
            bb.position(p += width_c9);
            ndbjtieTranscode(bb);
            bb.position(p += width_c10);
            ndbjtieTranscode(bb);
            bb.position(p += width_c11);
            ndbjtieTranscode(bb);
            bb.position(p += width_c12);
            ndbjtieTranscode(bb);
            bb.position(p += width_c13);
            ndbjtieTranscode(bb);
            bb.position(p += width_c14);
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void runNdbjtieUpdate(TwsDriver.XMode mode) {
        final String name = "update_" + mode.toString().toLowerCase();
        driver.beginOp(name);

        if (mode == TwsDriver.XMode.INDY) {
            for(int i = 0; i < driver.nRows; i++) {
                ndbjtieBeginTransaction();
                ndbjtieUpdate(i);
                ndbjtieCommitTransaction();
                ndbjtieCloseTransaction();
            }
        } else {
            ndbjtieBeginTransaction();
            for(int i = 0; i < driver.nRows; i++) {
                ndbjtieUpdate(i);

                if (mode == TwsDriver.XMode.EACH)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            ndbjtieCloseTransaction();
        }

        driver.finishOp(name, driver.nRows);
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
            ndbjtieTranscode(bb, str0);
            if (op.equal(attr_c0, bb) != 0) // key
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c0);

            //str1.rewind();
            ndbjtieTranscode(bb, str1);
            if (op.setValue(attr_c1, bb) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c1);

            if (op.setValue(attr_c2, r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            if (op.setValue(attr_c3, r) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            str1.rewind();
            ndbjtieTranscode(bb, str1);
            if (op.setValue(attr_c5, bb) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c5);

            str1.rewind();
            ndbjtieTranscode(bb, str1);
            if (op.setValue(attr_c6, bb) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c6);

            str1.rewind();
            ndbjtieTranscode(bb, str1);
            if (op.setValue(attr_c7, bb) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c7);

            str1.rewind();
            ndbjtieTranscode(bb, str1);
            if (op.setValue(attr_c8, bb) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(bb.position() + width_c8);
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void runNdbjtieDelete(TwsDriver.XMode mode) {
        final String name = "delete_" + mode.toString().toLowerCase();
        driver.beginOp(name);

        if (mode == TwsDriver.XMode.INDY) {
            for(int i = 0; i < driver.nRows; i++) {
                ndbjtieBeginTransaction();
                ndbjtieDelete(i);
                ndbjtieCommitTransaction();
                ndbjtieCloseTransaction();
            }
        } else {
            ndbjtieBeginTransaction();
            for(int i = 0; i < driver.nRows; i++) {
                ndbjtieDelete(i);

                if (mode == TwsDriver.XMode.EACH)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            ndbjtieCloseTransaction();
        }

        driver.finishOp(name, driver.nRows);
    }

    protected void ndbjtieDelete(int c0) {
        // get a delete operation for the table
        NdbOperation op = tx.getNdbOperation(table_t0);
        if (op == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if (op.deleteTuple() != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));

        int p = bb.position();

        // include exception handling as part of transcoding pattern
        final int i = c0;
        final CharBuffer str = CharBuffer.wrap(Integer.toString(c0));
        try {
            // set values; key attribute needs to be set first
            //str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.equal(attr_c0, bb) != 0) // key
                throw new RuntimeException(toStr(tx.getNdbError()));
            bb.position(p += width_c0);
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void ndbjtieBeginTransaction() {
        assert (tx == null);

        // prepare buffer for writing
        bb.clear();

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

        // prepare buffer for reading
        bb.rewind();
    }

    protected void ndbjtieCloseTransaction() {
        assert (tx != null);

        // close the current transaction (required after commit, rollback)
        ndb.closeTransaction(tx);
        tx = null;
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
}