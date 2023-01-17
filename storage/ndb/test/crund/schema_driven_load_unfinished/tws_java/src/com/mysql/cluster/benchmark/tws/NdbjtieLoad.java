/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2023, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.cluster.benchmark.tws;

import com.mysql.ndbjtie.ndbapi.Ndb_cluster_connection;
import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.TableConst;
import com.mysql.ndbjtie.ndbapi.NdbError;
import com.mysql.ndbjtie.ndbapi.NdbTransaction;
import com.mysql.ndbjtie.ndbapi.NdbOperation;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.CharsetEncoder;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CoderResult;
import java.nio.charset.CharacterCodingException;


class NdbjtieLoad extends TwsLoad {

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

    public NdbjtieLoad(TwsDriver driver) {
        super(driver, null);
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

        metaData = new MetaData(ndb);

        metaData.initNdbjtieModel();

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


    protected void closeNdbjtieModel() {
        assert (ndb != null);
        assert (table_t0 != null);
        assert (metaData.getColumn(0) != null);

        out.print("clearing metadata cache...");
        out.flush();


        table_t0 = null;

        out.println("      [ok]");
    }

    public void initNdbjtieBuffers() {
        assert (metaData.getColumn(0) != null);
        assert (bb == null);

        out.print("allocating buffers...");
        out.flush();

        bb = ByteBuffer.allocateDirect(metaData.getRowWidth() * driver.nRows);

        // initial order of a byte buffer is always BIG_ENDIAN
        bb.order(bo);

        out.println("           [ok]");
    }

    protected void closeNdbjtieBuffers() {
        assert (metaData.getColumn(0) != null);
        assert (bb != null);

        out.print("releasing buffers...");
        out.flush();

        bb = null;

        out.println("            [ok]");
    }


    // ----------------------------------------------------------------------

    public void runOperations() {
        out.println();
        out.println("running NDB JTie operations ..."
                    + " [nRows=" + driver.nRows + "]");

        if (driver.doSingle) {
            if (driver.doInsert) runNdbjtieInsert(TwsDriver.XMode.SINGLE);
            if (driver.doLookup) runNdbjtieLookup(TwsDriver.XMode.SINGLE);
            if (driver.doUpdate) runNdbjtieUpdate(TwsDriver.XMode.SINGLE);
            if (driver.doDelete) runNdbjtieDelete(TwsDriver.XMode.SINGLE);
        }
        if (driver.doBulk) {
            if (driver.doInsert) runNdbjtieInsert(TwsDriver.XMode.BULK);
            if (driver.doLookup) runNdbjtieLookup(TwsDriver.XMode.BULK);
            if (driver.doUpdate) runNdbjtieUpdate(TwsDriver.XMode.BULK);
            if (driver.doDelete) runNdbjtieDelete(TwsDriver.XMode.BULK);
        }
        if (driver.doBatch) {
            if (driver.doInsert) runNdbjtieInsert(TwsDriver.XMode.BATCH);
            if (driver.doLookup) runNdbjtieLookup(TwsDriver.XMode.BATCH);
            if (driver.doUpdate) runNdbjtieUpdate(TwsDriver.XMode.BATCH);
            if (driver.doDelete) runNdbjtieDelete(TwsDriver.XMode.BATCH);
        }
    }

    // ----------------------------------------------------------------------

    protected void runNdbjtieInsert(TwsDriver.XMode mode) {
        final String name = "insert_" + mode.toString().toLowerCase();
        driver.begin(name);

        initTable();

        if (mode == TwsDriver.XMode.SINGLE) {
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

                if (mode == TwsDriver.XMode.BULK)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            ndbjtieCloseTransaction();
        }

        driver.finish(name);
    }

    protected void initTable() {
        if(table_t0 == null) {

            final Dictionary dict = ndb.getDictionary();

            if ((table_t0 = dict.getTable("mytable")) == null)
                throw new RuntimeException(TwsUtils.toStr(dict.getNdbError()));
        }
    }

    protected void ndbjtieInsert(int c0) {

        // get an insert operation for the table
        NdbOperation op = tx.getNdbOperation(table_t0);
        if (op == null)
            throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
        if (op.insertTuple() != 0)
            throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

        // include exception handling as part of transcoding pattern
        final int i = c0;
        final CharBuffer str = CharBuffer.wrap(Integer.toString(i));
        try {
            // set values; key attribute needs to be set first
            //str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.equal(metaData.getAttr(0), bb) != 0) // key
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(bb.position() + metaData.getColumnWidth(0));

            str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.setValue(metaData.getAttr(1), bb) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(bb.position() + metaData.getColumnWidth(1));

            if (op.setValue(metaData.getAttr(2), i) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

            if (op.setValue(metaData.getAttr(3), i) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

            // XXX
            if (op.setValue(metaData.getAttr(4), null) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

            str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.setValue(metaData.getAttr(5), bb) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(bb.position() + metaData.getColumnWidth(5));

            str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.setValue(metaData.getAttr(6), bb) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(bb.position() + metaData.getColumnWidth(6));

            str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.setValue(metaData.getAttr(7), bb) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(bb.position() + metaData.getColumnWidth(7));

            str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.setValue(metaData.getAttr(8), bb) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(bb.position() + metaData.getColumnWidth(8));

            // XXX
            if (op.setValue(metaData.getAttr(9), null) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

            if (op.setValue(metaData.getAttr(10), null) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

            if (op.setValue(metaData.getAttr(11), null) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

            if (op.setValue(metaData.getAttr(12), null) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

            if (op.setValue(metaData.getAttr(13), null) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

            if (op.setValue(metaData.getAttr(14), null) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void runNdbjtieLookup(TwsDriver.XMode mode) {
        final String name = "lookup_" + mode.toString().toLowerCase();
        driver.begin(name);

        if (mode == TwsDriver.XMode.SINGLE) {
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

                if (mode == TwsDriver.XMode.BULK)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            for(int i = 0; i < driver.nRows; i++) {
                ndbjtieRead(i);
            }
            ndbjtieCloseTransaction();
        }

        driver.finish(name);
    }

    protected void ndbjtieLookup(int c0) {
        // get a lookup operation for the table
        NdbOperation op = tx.getNdbOperation(table_t0);
        if (op == null)
            throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
        if (op.readTuple(ndbOpLockMode) != 0)
            throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

        int p = bb.position();

        // include exception handling as part of transcoding pattern
        final CharBuffer str = CharBuffer.wrap(Integer.toString(c0));
        try {
            // set values; key attribute needs to be set first
            //str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.equal(metaData.getAttr(0), bb) != 0) // key
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(p += metaData.getColumnWidth(0));
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }

        // get attributes (not readable until after commit)
        for(int i = 1; i < metaData.getColumnCount(); i++) {
            if (op.getValue(metaData.getAttr(i), bb) == null)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(p += metaData.getColumnWidth(i));
        }
    }

    protected void ndbjtieRead(int c0) {
        // include exception handling as part of transcoding pattern
        //final int i = c0;
        //final CharBuffer str = CharBuffer.wrap(Integer.toString(i));
        //assert (str.position() == 0);

        try {
            int p = bb.position();
            bb.position(p += metaData.getColumnWidth(0));

            // not verifying at this time
            // (str.equals(ndbjtieTranscode(bb_c1)));
            // (i == bb_c2.asIntBuffer().get());
            //CharBuffer y = ndbjtieTranscode(bb);

            ndbjtieTranscode(bb);
            bb.position(p += metaData.getColumnWidth(1));

            bb.asIntBuffer().get();
            bb.position(p += metaData.getColumnWidth(2));
            bb.asIntBuffer().get();
            bb.position(p += metaData.getColumnWidth(3));
            bb.asIntBuffer().get();
            bb.position(p += metaData.getColumnWidth(4));


            for(int i = 5; i < metaData.getColumnCount(); i++) {
                ndbjtieTranscode(bb);
                bb.position(p += metaData.getColumnWidth(i));
            }
        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void runNdbjtieUpdate(TwsDriver.XMode mode) {
        final String name = "update_" + mode.toString().toLowerCase();
        driver.begin(name);

        if (mode == TwsDriver.XMode.SINGLE) {
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

                if (mode == TwsDriver.XMode.BULK)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            ndbjtieCloseTransaction();
        }

        driver.finish(name);
    }

    protected void ndbjtieUpdate(int c0) {
        final CharBuffer str0 = CharBuffer.wrap(Integer.toString(c0));
        final int r = -c0;
        final CharBuffer str1 = CharBuffer.wrap(Integer.toString(r));

        // get an update operation for the table
        NdbOperation op = tx.getNdbOperation(table_t0);
        if (op == null)
            throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
        if (op.updateTuple() != 0)
            throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

        // include exception handling as part of transcoding pattern
        try {
            // set values; key attribute needs to be set first
            //str0.rewind();
            ndbjtieTranscode(bb, str0);
            if (op.equal(metaData.getAttr(0), bb) != 0) // key
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(bb.position() + metaData.getColumnWidth(0));

            //str1.rewind();
            ndbjtieTranscode(bb, str1);
            if (op.setValue(metaData.getAttr(1), bb) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(bb.position() + metaData.getColumnWidth(1));

            if (op.setValue(metaData.getAttr(2), r) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

            if (op.setValue(metaData.getAttr(3), r) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

            for(int i = 5; i < metaData.getColumnCount(); i++) {
            str1.rewind();
            ndbjtieTranscode(bb, str1);
            if (op.setValue(metaData.getAttr(i), bb) != 0)
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(bb.position() + metaData.getColumnWidth(i));
            }

        } catch (CharacterCodingException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void runNdbjtieDelete(TwsDriver.XMode mode) {
        final String name = "delete_" + mode.toString().toLowerCase();
        driver.begin(name);

        if (mode == TwsDriver.XMode.SINGLE) {
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

                if (mode == TwsDriver.XMode.BULK)
                    ndbjtieExecuteTransaction();
            }
            ndbjtieCommitTransaction();
            ndbjtieCloseTransaction();
        }

        driver.finish(name);
    }

    protected void ndbjtieDelete(int c0) {
        // get a delete operation for the table
        NdbOperation op = tx.getNdbOperation(table_t0);
        if (op == null)
            throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
        if (op.deleteTuple() != 0)
            throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

        int p = bb.position();

        // include exception handling as part of transcoding pattern
        final int i = c0;
        final CharBuffer str = CharBuffer.wrap(Integer.toString(c0));
        try {
            // set values; key attribute needs to be set first
            //str.rewind();
            ndbjtieTranscode(bb, str);
            if (op.equal(metaData.getAttr(0), bb) != 0) // key
                throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
            bb.position(p += metaData.getColumnWidth(0));
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
            throw new RuntimeException(TwsUtils.toStr(ndb.getNdbError()));
    }

    protected void ndbjtieExecuteTransaction() {
        assert (tx != null);

        // execute but don't commit the current transaction
        final int execType = NdbTransaction.ExecType.NoCommit;
        final int abortOption = NdbOperation.AbortOption.AbortOnError;
        final int force = 0;
        if (tx.execute(execType, abortOption, force) != 0
            || tx.getNdbError().status() != NdbError.Status.Success)
            throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));
    }

    protected void ndbjtieCommitTransaction() {
        assert (tx != null);

        // commit the current transaction
        final int execType = NdbTransaction.ExecType.Commit;
        final int abortOption = NdbOperation.AbortOption.AbortOnError;
        final int force = 0;
        if (tx.execute(execType, abortOption, force) != 0
            || tx.getNdbError().status() != NdbError.Status.Success)
            throw new RuntimeException(TwsUtils.toStr(tx.getNdbError()));

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
