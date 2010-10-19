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

import java.util.Arrays;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;

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
/*
//import com.mysql.ndbjtie.ndbapi.ExecType; // ndbj-0.7.0
import com.mysql.ndbjtie.ndbapi.NdbTransaction.ExecType; // ndbj-0.7.1
import com.mysql.ndbjtie.ndbapi.NdbOperation;
import com.mysql.ndbjtie.ndbapi.NdbOperation.LockMode;
//import com.mysql.ndbjtie.ndbapi.AbortOption; // ndbj-0.7.0
import com.mysql.ndbjtie.ndbapi.NdbOperation.AbortOption; // ndbj-0.7.1
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation.BoundType;
*/

/**
 * The CRUND benchmark implementation against NDBJTIE.
 */
public class NdbJTieLoad extends NdbBase {

    // ----------------------------------------------------------------------
    // NDB JTie resources
    // ----------------------------------------------------------------------

    // singleton object representing the NDB cluster (one per process)
    protected Ndb_cluster_connection mgmd;

    // object representing a connection to an NDB database
    protected Ndb ndb;

    // the benchmark's metadata shortcuts
    protected Model model;

    // object representing an NDB database transaction
    protected NdbTransaction tx;

    // ----------------------------------------------------------------------
    // NDB JTie intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        super.initProperties();
        descr = "->ndbjtie(" + mgmdConnect + ")";
    }

    protected void init() throws Exception {
        super.init();

        // load native library (better diagnostics doing it explicitely)
        out.println();
        loadSystemLibrary("ndbclient");

        // instantiate NDB cluster singleton
        out.print("creating cluster connection ...");
        out.flush();
        mgmd = Ndb_cluster_connection.create(mgmdConnect);
        assert mgmd != null;
        out.println(" [ok]");

        // connect to cluster management node (ndb_mgmd)
        out.print("connecting to mgmd ...");
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
        out.println("          [ok: " + mgmdConnect + "]");
    }

    protected void close() throws Exception {
        out.print("closing mgmd conn ...");
        out.flush();
        if (mgmd != null)
            Ndb_cluster_connection.delete(mgmd);
        mgmd = null;
        out.println("           [ok]");
        super.close();
    }

    // ----------------------------------------------------------------------
    // NDB JTie operations
    // ----------------------------------------------------------------------

    // returns a string representation of an NdbError
    static protected String toStr(NdbErrorConst e) {
        return "NdbError[" + e.code() + "]: " + e.message();
    }

    // holds shortcuts to the benchmark's schema information
    static protected class Model {
        public final TableConst table_A;
        public final TableConst table_B0;
        public final ColumnConst column_A_id;
        public final ColumnConst column_A_cint;
        public final ColumnConst column_A_clong;
        public final ColumnConst column_A_cfloat;
        public final ColumnConst column_A_cdouble;
        public final ColumnConst column_B0_id;
        public final ColumnConst column_B0_cint;
        public final ColumnConst column_B0_clong;
        public final ColumnConst column_B0_cfloat;
        public final ColumnConst column_B0_cdouble;
        public final ColumnConst column_B0_a_id;
        public final ColumnConst column_B0_cvarbinary_def;
        public final ColumnConst column_B0_cvarchar_def;
        public final IndexConst idx_B0_a_id;
        public final int attr_id;
        public final int attr_cint;
        public final int attr_clong;
        public final int attr_cfloat;
        public final int attr_cdouble;
        public final int attr_B0_a_id;
        public final int attr_B0_cvarbinary_def;
        public final int attr_B0_cvarchar_def;

        // initialize this instance from the dictionary
        public Model(Ndb ndb) {
            final Dictionary dict = ndb.getDictionary();

            // get columns of table A
            if ((table_A = dict.getTable("a")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_A_id = table_A.getColumn("id")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_A_cint = table_A.getColumn("cint")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_A_clong = table_A.getColumn("clong")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_A_cfloat = table_A.getColumn("cfloat")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_A_cdouble = table_A.getColumn("cdouble")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));

            // get columns of table B0
            if ((table_B0 = dict.getTable("b0")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B0_id = table_B0.getColumn("id")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B0_cint = table_B0.getColumn("cint")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B0_clong = table_B0.getColumn("clong")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B0_cfloat = table_B0.getColumn("cfloat")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B0_cdouble = table_B0.getColumn("cdouble")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B0_a_id = table_B0.getColumn("a_id")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B0_cvarbinary_def = table_B0.getColumn("cvarbinary_def")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B0_cvarchar_def = table_B0.getColumn("cvarchar_def")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));

            // get indexes of table B0
            if ((idx_B0_a_id = dict.getIndex("I_B0_FK", "b0")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));

            // get common attribute ids for tables A, B0
            attr_id = column_A_id.getColumnNo();
            if (attr_id != column_B0_id.getColumnNo())
                throw new RuntimeException("attribute id mismatch");
            attr_cint = column_A_cint.getColumnNo();
            if (attr_cint != column_B0_cint.getColumnNo())
                throw new RuntimeException("attribute id mismatch");
            attr_clong = column_A_clong.getColumnNo();
            if (attr_clong != column_B0_clong.getColumnNo())
                throw new RuntimeException("attribute id mismatch");
            attr_cfloat = column_A_cfloat.getColumnNo();
            if (attr_cfloat != column_B0_cfloat.getColumnNo())
                throw new RuntimeException("attribute id mismatch");
            attr_cdouble = column_A_cdouble.getColumnNo();
            if (attr_cdouble != column_B0_cdouble.getColumnNo())
                throw new RuntimeException("attribute id mismatch");

            // get attribute ids for table B0
            attr_B0_a_id = column_B0_a_id.getColumnNo();
            attr_B0_cvarbinary_def = column_B0_cvarbinary_def.getColumnNo();
            attr_B0_cvarchar_def = column_B0_cvarchar_def.getColumnNo();
        }
    };

    protected void initOperations() {
        out.print("initializing operations ...");
        out.flush();

        //out.println("default charset: "
        //    + java.nio.charset.Charset.defaultCharset().displayName());

        for (boolean f = false, done = false; !done; done = f, f = true) {
            // inner classes can only refer to a constant
            final boolean batch = f;
            final boolean forceSend = f;
            final boolean setAttrs = true;

            ops.add(
                new Op("insA" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        ins(model.table_A, 1, countA, !setAttrs, batch);
                    }
                });

            ops.add(
                new Op("insB0" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        ins(model.table_B0, 1, countB, !setAttrs, batch);
                    }
                });

            ops.add(
                new Op("setAByPK" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        setByPK(model.table_A, 1, countA, batch);
                    }
                });

            ops.add(
                new Op("setB0ByPK" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        setByPK(model.table_B0, 1, countB, batch);
                    }
                });

            ops.add(
                new Op("getAByPK_bb" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        getByPK_bb(model.table_A, 1, countA, batch);
                    }
                });

            ops.add(
                new Op("getAByPK_ar" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        getByPK_ar(model.table_A, 1, countA, batch);
                    }
                });

            ops.add(
                new Op("getB0ByPK_bb" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        getByPK_bb(model.table_B0, 1, countB, batch);
                    }
                });

            ops.add(
                new Op("getB0ByPK_ar" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        getByPK_ar(model.table_B0, 1, countB, batch);
                    }
                });

            for (int i = 0, l = 1; l <= maxVarbinaryBytes; l *= 10, i++) {
                final byte[] b = bytes[i];
                assert l == b.length;

                ops.add(
                    new Op("setVarbinary" + l + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            setVarbinary(model.table_B0, 1, countB, batch, b);
                        }
                    });

                ops.add(
                    new Op("getVarbinary" + l + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            getVarbinary(model.table_B0, 1, countB, batch, b);
                        }
                    });

                ops.add(
                    new Op("clearVarbinary" + l + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            setVarbinary(model.table_B0, 1, countB, batch, null);
                        }
                    });
            }

            for (int i = 0, l = 1; l <= maxVarcharChars; l *= 10, i++) {
                final String s = strings[i];
                assert l == s.length();

                ops.add(
                    new Op("setVarchar" + l + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            setVarchar(model.table_B0, 1, countB, batch, s);
                        }
                    });

                ops.add(
                    new Op("getVarchar" + l + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            getVarchar(model.table_B0, 1, countB, batch, s);
                        }
                    });

                ops.add(
                    new Op("clearVarchar" + l + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            setVarchar(model.table_B0, 1, countB, batch, null);
                        }
                    });
            }

            ops.add(
                new Op("setB0->A" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        setB0ToA(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("navB0->A" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        navB0ToA(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("navB0->A_alt" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        navB0ToAalt(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("navA->B0" + (forceSend ? "_forceSend" : "")) {
                    public void run(int countA, int countB) {
                        navAToB0(countA, countB, forceSend);
                    }
                });

            ops.add(
                new Op("navA->B0_alt" + (forceSend ? "_forceSend" : "")) {
                    public void run(int countA, int countB) {
                        navAToB0alt(countA, countB, forceSend);
                    }
                });

            ops.add(
                new Op("nullB0->A" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        nullB0ToA(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("delB0ByPK" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        delByPK(model.table_B0, 1, countB, batch);
                    }
                });

            ops.add(
                new Op("delAByPK" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        delByPK(model.table_A, 1, countA, batch);
                    }
                });

            ops.add(
                new Op("insA_attr" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        ins(model.table_A, 1, countA, setAttrs, batch);
                    }
                });

            ops.add(
                new Op("insB0_attr" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        ins(model.table_B0, 1, countB, setAttrs, batch);
                    }
                });

            ops.add(
                new Op("delAllB0" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        final int count = delByScan(model.table_B0, batch);
                        assert count == countB;
                    }
                });

            ops.add(
                new Op("delAllA" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        final int count = delByScan(model.table_A, batch);
                        assert count == countA;
                    }
                });
        }

        out.println("     [Op: " + ops.size() + "]");
    }

    protected void closeOperations() {
        out.print("closing operations ...");
        out.flush();
        ops.clear();
        out.println("          [ok]");
    }

    protected void beginTransaction() {
        // start a transaction
        // must be closed with NdbTransaction.close
        final TableConst table  = null;
        final ByteBuffer keyData = null;
        final int keyLen = 0;
        if ((tx = ndb.startTransaction(table, keyData, keyLen)) == null)
            throw new RuntimeException(toStr(ndb.getNdbError()));
    }

    protected void executeOperations() {
        // execute but don't commit the current transaction
        final int execType = NdbTransaction.ExecType.NoCommit;
        final int abortOption = NdbOperation.AbortOption.AbortOnError;
        final int force = 0;
        if (tx.execute(execType, abortOption, force) != 0
            || tx.getNdbError().status() != NdbError.Status.Success)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void commitTransaction() {
        // commit the current transaction
        final int execType = NdbTransaction.ExecType.Commit;
        final int abortOption = NdbOperation.AbortOption.AbortOnError;
        final int force = 0;
        if (tx.execute(execType, abortOption, force) != 0
            || tx.getNdbError().status() != NdbError.Status.Success)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void rollbackTransaction() {
        // abort the current transaction
        final int execType = NdbTransaction.ExecType.Rollback;
        final int abortOption = NdbOperation.AbortOption.DefaultAbortOption;
        final int force = 0;
        if (tx.execute(execType, abortOption, force) != 0
            || tx.getNdbError().status() != NdbError.Status.Success)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void closeTransaction() {
        // close the current transaction
        // to be called irrespectively of success or failure
        // equivalent to tx.close()
        ndb.closeTransaction(tx);
        tx = null;
    }

    // ----------------------------------------------------------------------

    static protected class CommonAB_RA {
        public NdbRecAttr id;
        public NdbRecAttr cint;
        public NdbRecAttr clong;
        public NdbRecAttr cfloat;
        public NdbRecAttr cdouble;
    };

    protected void fetchCommonAttributes(CommonAB_RA cab, NdbOperation op) {
        final ByteBuffer val = null;
        if ((cab.id = op.getValue(model.attr_id, val)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if ((cab.cint = op.getValue(model.attr_cint, val)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if ((cab.clong = op.getValue(model.attr_clong, val)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if ((cab.cfloat = op.getValue(model.attr_cfloat, val)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if ((cab.cdouble = op.getValue(model.attr_cdouble, val)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected int verifyCommonAttributes(CommonAB_RA cab) {
        final int id = cab.id.int32_value();
        final int cint = cab.cint.int32_value();
        verify(id == cint);
        final long clong = cab.clong.int64_value();
        verify(clong == cint);
        final float cfloat = cab.cfloat.float_value();
        verify(cfloat == cint);
        final double cdouble = cab.cdouble.double_value();
        verify(cdouble == cint);
        return cint;
    }

    protected void ins(TableConst table, int from, int to,
                       boolean setAttrs, boolean batch) {
        beginTransaction();
        for (int i = from; i <= to; i++) {
            // get an insert operation for the table
            NdbOperation op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.insertTuple() != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // set values; key attribute needs to be set first
            if (op.equal(model.attr_id, i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (setAttrs) {
                if (op.setValue(model.attr_cint, -i) != 0)
                    throw new RuntimeException(toStr(tx.getNdbError()));
                if (op.setValue(model.attr_clong, (long)-i) != 0)
                    throw new RuntimeException(toStr(tx.getNdbError()));
                if (op.setValue(model.attr_cfloat, (float)-i) != 0)
                    throw new RuntimeException(toStr(tx.getNdbError()));
                if (op.setValue(model.attr_cdouble, (double)-i) != 0)
                    throw new RuntimeException(toStr(tx.getNdbError()));
            }

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
    }

    protected void delByPK(TableConst table, int from, int to,
                           boolean batch) {
        beginTransaction();
        for (int i = from; i <= to; i++) {
            // get a delete operation for the table
            NdbOperation op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.deleteTuple() != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // set key attribute
            if (op.equal(model.attr_id, i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
    }

    protected int delByScan(TableConst table, boolean batch) {
        beginTransaction();

        // get a full table scan operation (no scan filter defined)
        final NdbScanOperation op = tx.getNdbScanOperation(table);
        if (op == null)
            throw new RuntimeException(toStr(tx.getNdbError()));

        // define a read scan with exclusive locks
        final int lock_mode = NdbOperation.LockMode.LM_Exclusive;
        final int scan_flags = 0;
        final int parallel = 0;
        final int batch_ = 0;
        if (op.readTuples(lock_mode, scan_flags, parallel, batch_) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));

        // start the scan; don't commit yet
        executeOperations();

        // delete all rows in a given scan
        int count = 0;
        int stat;
        final boolean allowFetch = true; // request new batches when exhausted
        final boolean forceSend = false; // send may be delayed
        while ((stat = op.nextResult(allowFetch, forceSend)) == 0) {
            // delete all tuples within a batch
            do {
                if (op.deleteCurrentTuple() != 0)
                    throw new RuntimeException(toStr(tx.getNdbError()));
                count++;

                // execute the operation now if in non-batching mode
                if (!batch)
                    executeOperations();
            } while ((stat = op.nextResult(!allowFetch, forceSend)) == 0);

            if (stat == 1) {
                // no more batches
                break;
            }
            if (stat == 2) {
                // end of current batch, fetch next
                final int execType = NdbTransaction.ExecType.NoCommit;
                final int abortOption = NdbOperation.AbortOption.AbortOnError;
                final int force = 0;
                if (tx.execute(execType, abortOption, force) != 0
                    || tx.getNdbError().status() != NdbError.Status.Success)
                    throw new RuntimeException(toStr(tx.getNdbError()));
                continue;
            }
            throw new RuntimeException("stat == " + stat);
        }
        if (stat != 1)
            throw new RuntimeException("stat == " + stat);

        // close the scan
        final boolean forceSend_ = false;
        final boolean releaseOp = false;
        op.close(forceSend_, releaseOp);

        commitTransaction();
        closeTransaction();
        return count;
    }

    protected void setByPK(TableConst table, int from, int to,
                           boolean batch) {
        beginTransaction();
        for (int i = from; i <= to; i++) {
            // get an update operation for the table
            NdbOperation op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.updateTuple() != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // set key attribute
            if (op.equal(model.attr_id, i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // set values
            if (op.setValue(model.attr_cint, i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.setValue(model.attr_clong, (long)i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.setValue(model.attr_cfloat, (float)i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.setValue(model.attr_cdouble, (double)i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
    }

    protected void fetchCommonAttributes(ByteBuffer cab, NdbOperation op) {
        if (op.getValue(model.attr_id, cab) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        int p = cab.position();
        //out.println("cab.position() == " + p);
        cab.position(p += 4);
        if (op.getValue(model.attr_cint, cab) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        cab.position(p += 4);
        if (op.getValue(model.attr_clong, cab) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        cab.position(p += 8);
        if (op.getValue(model.attr_cfloat, cab) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        cab.position(p += 4);
        if (op.getValue(model.attr_cdouble, cab) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        cab.position(p += 8);
    }

    protected int verifyCommonAttributes(ByteBuffer cab) {
        final int id = cab.getInt();
        final int cint = cab.getInt();
        final long clong = cab.getLong();
        final float cfloat = cab.getFloat();
        final double cdouble = cab.getDouble();

        if (false) {
            out.println("id == " + id);
            out.println("cint == " + cint);
            out.println("clong == " + clong);
            out.println("cfloat == " + cfloat);
            out.println("cdouble == " + cdouble);
        }
        if (false) {
            verify(cint == id);
            verify(clong == cint);
            verify(cfloat == cint);
            verify(cdouble == cint);
        }
        return cint;
    }

    protected void getByPK_bb(TableConst table, int from, int to,
                              boolean batch) {
        // operation results
        final int count = (to - from) + 1;
        final ByteBuffer cab = ByteBuffer.allocateDirect(count * 28);
        cab.order(ByteOrder.nativeOrder());

        beginTransaction();
        for (int i = 0, j = from; i < count; i++, j++) {
            // get a read operation for the table
            NdbOperation op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.readTuple(NdbOperation.LockMode.LM_CommittedRead) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // set key attribute
            if (op.equal(model.attr_id, j) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // get attributes (not readable until after commit)
            fetchCommonAttributes(cab, op);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();

        // check fetched values
        cab.rewind();

        for (int i = 0, j = from; i < count; i++, j++) {
            // check other attributes
            final int id1 = verifyCommonAttributes(cab);
            verify(id1 == j);
        }
        closeTransaction();
    }

    protected void getByPK_ar(TableConst table, int from, int to,
                              boolean batch) {
        // operation results
        final int count = (to - from) + 1;
        final CommonAB_RA[] cab_ra = new CommonAB_RA[count];

        beginTransaction();
        for (int i = 0, j = from; i < count; i++, j++) {
            // get a read operation for the table
            NdbOperation op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.readTuple(NdbOperation.LockMode.LM_CommittedRead) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // set key attribute
            if (op.equal(model.attr_id, j) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // get attributes (not readable until after commit)
            final CommonAB_RA c = new CommonAB_RA();
            //if ((c.id = op.getValue(model.attr_id, null)) == null)
            //    throw new RuntimeException(toStr(tx.getNdbError()));
            fetchCommonAttributes(c, op);
            cab_ra[i] = c;

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();

        // check fetched values
        for (int i = 0, j = from; i < count; i++, j++) {
            //check key attribute
            verify(cab_ra[i].id.int32_value() == j);

            // check other attributes
            final int id1 = verifyCommonAttributes(cab_ra[i]);
            verify(id1 == j);
        }
        closeTransaction();
    }

    protected void setVarbinary(TableConst table, int from, int to,
                                boolean batch, byte[] bytes) {
// XXX Buffer overflow, need to debug
/*
        final ByteBuffer buf;
        if (bytes == null) {
            buf = null;
        } else {
            final int slen = bytes.length;
            // XXX assumes column declared as VARBINARY/CHAR(<255)
            final int sbuf = 1 + slen;
            // XXX buffer overflow if slen >255!!!
            assert (slen < 255);
            buf = ByteBuffer.allocateDirect(slen);
            //buf.order(ByteOrder.nativeOrder());
            buf.put((byte)slen);
            buf.put(bytes, 0, slen);
            buf.flip();
        }

        beginTransaction();
        for (int i = from; i <= to; i++) {
            // get an insert operation for the table
            NdbOperation op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.updateTuple() != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // set key attribute
            if (op.equal(model.attr_id, i) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // set values
            if (op.setValue(model.attr_B0_cvarbinary_def, buf) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
*/
    }

    protected void setVarchar(TableConst table, int from, int to,
                              boolean batch, String string) {
// XXX not implemented yet
/*
        beginTransaction();
        for (int i = from; i <= to; i++) {
            // get an update operation for the table
            final NdbOperation op = tx.getUpdateOperation(table);
            assert op != null;

            // set key attribute
            op.equalInt(model.attr_id, i);

            // set varchar
            op.setString(model.attr_B0_cvarchar_def, string);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
*/
    }

    protected void getVarbinary(TableConst table, int from, int to,
                                boolean batch, byte[] bytes) {
// XXX not implemented yet
/*
        final int count = (to - from) + 1;
        final int slen = bytes.length;
        final int sline = (1 + slen);
        final int sbuf = count * sline;
        final ByteBuffer buf = ByteBuffer.allocateDirect(sbuf);
        //buf.order(ByteOrder.nativeOrder());

        beginTransaction();
        for (int i = 0, j = from; i < count; i++, j++) {
            // get a read operation for the table
            NdbOperation op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.readTuple(NdbOperation.LockMode.LM_CommittedRead) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // set key attribute
            if (op.equal(model.attr_id, j) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // get attributes (not readable until after commit)
            final int p = buf.position();
            if (op.getValue(attr_cvar, buf) == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            //out.println("buf.position() == " + p);
            buf.position(p += sline);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();

        // check fetched values
        buf.rewind();

        // copy (move) the strings to make them aligned and 0-terminated
        for (int i = 0, j = from; i < count; i++, j++) {
            // check other attributes
            final int id1 = verifyCommonAttributes(buf);
            verify(id1 == j);

            const size_t n = s[0];
            VERIFY(n < sline);

            // move and 0-terminated string
            memmove(s, s + 1, n);
            s[n] = 0;

            // check fetched values
            //CDBG << "!!! s=" << (void*)s << ", '" << s << "'" << endl;
            VERIFY(strcmp(s, str) == 0);
        }
        closeTransaction();
*/
    }

    protected void getVarchar(TableConst table, int from, int to,
                              boolean batch, String string) {
// XXX not implemented yet
/*
        // operation results
        final int count = (to - from) + 1;
        final NdbResultSet[] rss = new NdbResultSet[count];

        beginTransaction();
        for (int i = 0, j = from; i < count; i++, j++) {
            // get a read operation for the table
            NdbOperation op = tx.getSelectOperation(table);
            assert op != null;

            // set key attribute
            op.equalInt(model.attr_id, j);

            // define fetched attributes
            op.getValue(model.attr_B0_cvarchar_def);

            // get attributes (not readable until after commit)
            rss[i] = op.resultData();

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        //executeOperations();
        commitTransaction();

        // check fetched values
        for (int i = 0, j = from; i < count; i++, j++) {
            final NdbResultSet rs = rss[i];
            final boolean hasNext = rs.next();
            assert hasNext;

            // check varchar
            if (true) {
                final String cvarchar_def
                    = rs.getString(model.attr_B0_cvarchar_def);
                verify(string.equals(cvarchar_def));
            } else {
                // verification imposes a string->bytes conversion penalty
                final byte[] cvarchar_def
                    = rs.getStringBytes(model.attr_B0_cvarchar_def);
                verify(Arrays.equals(string.getBytes(), cvarchar_def));
            }

            assert !rs.next();
        }
        closeTransaction();
*/
    }

    protected void setB0ToA(int count_A, int count_B,
                            boolean batch) {
// XXX not implemented yet
/*
        beginTransaction();
        for (int i = 1; i <= count_B; i++) {
            // get an update operation for the table
            final NdbOperation op = tx.getUpdateOperation(model.table_B0);
            assert op != null;

            // set key attribute
            op.equalInt(model.attr_id, i);

            // set a_id attribute
            int a_id = ((i - 1) % count_A) + 1;
            op.setInt(model.attr_B0_a_id, a_id);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
*/
    }

    protected void nullB0ToA(int count_A, int count_B,
                             boolean batch) {
// XXX not implemented yet
/*
        beginTransaction();
        for (int i = 1; i <= count_B; i++) {
            // get an update operation for the table
            final NdbOperation op = tx.getUpdateOperation(model.table_B0);
            assert op != null;

            // set key attribute
            op.equalInt(model.attr_id, i);

            // set a_id attribute
            int a_id = ((i - 1) % count_A) + 1;
            op.setNull(model.attr_B0_a_id);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
*/
    }

    protected void navB0ToA(int count_A, int count_B,
                            boolean batch) {
// XXX not implemented yet
/*
        beginTransaction();

        // fetch the foreign keys from B0 and read attributes from A
        final NdbResultSet[] abs = new NdbResultSet[count_B];
        for (int i = 1, j = 0; i <= count_B; i++, j++) {
            // fetch the foreign key value from B0
            NdbResultSet rs;
            {
                // get a read operation for the table
                NdbOperation op = tx.getSelectOperation(model.table_B0);
                assert op != null;

                // set key attribute
                op.equalInt(model.attr_id, i);

                // define fetched attributes
                op.getValue(model.attr_B0_a_id);

                // get attributes (not readable until after commit)
                rs = op.resultData();
            }
            executeOperations(); // start the scan; don't commit yet

            // fetch the attributes from A
            {
                // get a read operation for the table
                NdbOperation op = tx.getSelectOperation(model.table_A);
                assert op != null;

                // set key attribute
                final int a_id = rs.getInt(model.attr_B0_a_id);
                assert a_id == ((i - 1) % count_A) + 1;
                op.equalInt(model.attr_id, a_id);

                // define fetched attributes
                op.getValue(model.attr_id);
                fetchCommonAttributes(op);

                // get attributes (not readable until after commit)
                abs[j] = op.resultData();
            }

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();

        // check fetched values
        for (int i = 1, j = 0; i <= count_B; i++, j++) {
            final NdbResultSet ab = abs[j];
            final boolean hasNext = ab.next();
            assert hasNext;

            // check key attribute
            final int id = ab.getInt(model.attr_id);
            //out.println("id = " + id + ", i = " + i);
            verify(id == ((i - 1) % count_A) + 1);

            // check other attributes
            final int k = getCommonAttributes(ab);
            verify(k == id);

            assert !ab.next();
        }
        closeTransaction();
*/
    }

    protected void navB0ToAalt(int count_A, int count_B,
                               boolean batch) {
// XXX not implemented yet
/*
        beginTransaction();

        // fetch the foreign key value from B0
        final NdbResultSet[] a_ids = new NdbResultSet[count_B];
        for (int i = 1, j = 0; i <= count_B; i++, j++) {
                // get a read operation for the table
                NdbOperation op = tx.getSelectOperation(model.table_B0);
                assert op != null;

                // set key attribute
                op.equalInt(model.attr_id, i);

                // define fetched attributes
                op.getValue(model.attr_B0_a_id);

                // get attributes (not readable until after commit)
                a_ids[j] = op.resultData();
        }
        executeOperations(); // start the scan; don't commit yet

        // fetch the attributes from A
        final NdbResultSet[] abs = new NdbResultSet[count_B];
        for (int i = 1, j = 0; i <= count_B; i++, j++) {
            // get a read operation for the table
            NdbOperation op = tx.getSelectOperation(model.table_A);
            assert op != null;

            // set key attribute
            final int a_id = a_ids[j].getInt(model.attr_B0_a_id);
            assert a_id == ((i - 1) % count_A) + 1;
            op.equalInt(model.attr_id, a_id);

            // define fetched attributes
            op.getValue(model.attr_id);
            fetchCommonAttributes(op);

            // get attributes (not readable until after commit)
            abs[j] = op.resultData();

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();

        // check fetched values
        for (int i = 1, j = 0; i <= count_B; i++, j++) {
            final NdbResultSet ab = abs[j];
            final boolean hasNext = ab.next();
            assert hasNext;

            // check key attribute
            final int id = ab.getInt(model.attr_id);
            //out.println("id = " + id + ", i = " + i);
            verify(id == ((i - 1) % count_A) + 1);

            // check other attributes
            final int k = getCommonAttributes(ab);
            verify(k == id);

            assert !ab.next();
        }
        closeTransaction();
*/
    }

    protected void navAToB0(int count_A, int count_B,
                            boolean forceSend) {
// XXX not implemented yet
/*
        beginTransaction();

        // fetch attributes from B0 by foreign key scan
        final NdbResultSet[] abs = new NdbResultSet[count_B];
        int j = 0;
        for (int i = 1; i <= count_B; i++) {
            // get an index scan operation for the table
            // XXX ? no locks (LM_CommittedRead) or shared locks (LM_Read)
            final NdbIndexScanOperation op
                = tx.getSelectIndexScanOperation(model.idx_B0_a_id,
                                                 model.table_B0,
                                                 LockMode.LM_CommittedRead);
            assert op != null;

            // define the scan's bounds (faster than using a scan filter)
            // XXX this hardwired column name isn't right
            //op.setBoundInt("a_id", BoundType.BoundEQ, i);
            // compare with Operations.cpp:
            //    if (op->setBound(idx_B0_a_id->getColumn(0)->getAttrId()...
            //
            // which translates into
            //out.println("idx_B0_a_id.getNoOfColumns() = "
            //            + model.idx_B0_a_id.getNoOfColumns());
            //out.println("idx_B0_a_id.getColumn(0).getColumnNo() = "
            //            + model.idx_B0_a_id.getColumn(0).getColumnNo());
            //op.setBoundInt(model.idx_B0_a_id.getColumn(0).getColumnNo(),
            //               BoundType.BoundEQ, i);
            // except that we get the usual error with NDBJ:
            //[java] idx_B0_a_id.getColumn(0).getColumnNo() = 0
            //    [java] caught com.mysql.ndbjtie.ndbapi.NdbApiException:
            //           Invalid attribute name or number
            //
            // so we go by column name
            //out.println("idx_B0_a_id.getColumn(0).getName() = "
            //            + model.idx_B0_a_id.getColumn(0).getName());
            //op.setBoundInt(model.idx_B0_a_id.getColumn(0).getName(),
            //               BoundType.BoundEQ, i);
            // which is actually "a_id", so, for now, we call
            op.setBoundInt("a_id", BoundType.BoundEQ, i);

            // define fetched attributes
            op.getValue(model.attr_id);
            fetchCommonAttributes(op);

            // start the scan; don't commit yet
            executeOperations();

            int stat;
            final boolean allowFetch = true; // request new batches when exhausted
            while ((stat = op.nextResult(allowFetch, forceSend)) == 0) {
                // get attributes (not readable until after commit)
                abs[j++] = op.resultData();
            }
            if (stat != 1)
                throw new RuntimeException("stat == " + stat);
        }
        commitTransaction();
        assert (j++ == count_B);

        // check fetched values
        j = 0;
        for (int i = 1; i <= count_B; i++) {
            final NdbResultSet ab = abs[j++];
            //out.println("j = " + j + ", ab = " + ab);
            //final boolean hasNext = ab.next();
            // throws
            //[java] j = 1, ab = com.mysql.ndbjtie.ndbapi.NdbResultSetImpl@6f144c
            //    [java] caught com.mysql.ndbjtie.ndbapi.NdbApiException: Unknown error code
            //    [java] com.mysql.ndbjtie.ndbapi.NdbApiException: Unknown error code
            //    [java] at com.mysql.ndbjtie.ndbapi.NdbJTieJNI.NdbScanOperationImpl_nextResult__SWIG_(Native Method)
            //    [java] at com.mysql.ndbjtie.ndbapi.NdbScanOperationImpl.nextResult(NdbScanOperationImpl.java:93)
            //    [java] at com.mysql.ndbjtie.ndbapi.NdbResultSetImpl.next(NdbResultSetImpl.java:362)
            //    [java] at com.mysql.cluster.crund.NdbJTieLoad.navAToB0(NdbJTieLoad.java:1205)
            //
            // YYY Frazer: check tx object for error (could be node failure)
            // Martin: doesn't help much; after ab.next():
            //out.println("tx.getNdbError() = " + tx.getNdbError().getCode());
            // returns -1 and
            //out.println("tx.getNdbError() = " + tx.getNdbError().getMessage());
            // says "Unknown error code"
            //
            // apparently,
            //final boolean hasNext = ab.next();
            // is the same as
            //final boolean hasNext = ab.next(true);
            // this returns false, but throws no exception:
            //final boolean hasNext = ab.next(false);
            out.println("tx.getNdbError() = " + tx.getNdbError().getCode());
            final boolean hasNext = ab.next();
            assert hasNext;

            // check key attribute
            final int id = ab.getInt(model.attr_id);
            verify(id == i);

            // check other attributes
            final int id1 = getCommonAttributes(ab);
            verify(id1 == i);

            assert !ab.next();
        }
        closeTransaction();
*/
    }

    protected void navAToB0alt(int count_A, int count_B,
                               boolean forceSend) {
// XXX not implemented yet, fix exception in navAToB0() first
/*
        assert false;
*/
    }


    // ----------------------------------------------------------------------
    // NDB JTie datastore operations
    // ----------------------------------------------------------------------

    protected void initConnection() {
        // optionally, connect and wait for reaching the data nodes (ndbds)
        out.print("waiting for data nodes...");
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
        out.print("connecting to database...");
        out.flush();
        ndb = Ndb.create(mgmd, catalog, schema);
        final int max_no_tx = 10; // maximum number of parallel tx (<=1024)
        // note each scan or index scan operation uses one extra transaction
        if (ndb.init(max_no_tx) != 0) {
            String msg = "Error caught: " + ndb.getNdbError().message();
            throw new RuntimeException(msg);
        }
        out.println("       [ok]");

        // initialize the schema shortcuts
        model = new Model(ndb);
    }

    protected void closeConnection() {
        out.print("closing database conn ...");
        out.flush();
        model = null;
        Ndb.delete(ndb);
        ndb = null;
        out.println("       [ok]");
    }

    protected void clearData() {
        out.print("deleting all rows ...");
        out.flush();
        final int delB0 = delByScan(model.table_B0, true);
        out.print("           [B0: " + delB0);
        out.flush();
        final int delA = delByScan(model.table_A, true);
        out.print(", A: " + delA);
        out.flush();
        out.println("]");
    }

    // ----------------------------------------------------------------------

    static public void main(String[] args) {
        System.out.println("NdbJTieLoad.main()");
        parseArguments(args);
        new NdbJTieLoad().run();
        System.out.println();
        System.out.println("NdbJTieLoad.main(): done.");
    }
}
