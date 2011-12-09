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

import java.util.Arrays;

//import com.mysql.cluster.ndbj.*;
import com.mysql.cluster.ndbj.NdbClusterConnection;
import com.mysql.cluster.ndbj.Ndb;
import com.mysql.cluster.ndbj.NdbDictionary;
import com.mysql.cluster.ndbj.NdbTable;
import com.mysql.cluster.ndbj.NdbColumn;
import com.mysql.cluster.ndbj.NdbIndex;
import com.mysql.cluster.ndbj.NdbError;
import com.mysql.cluster.ndbj.NdbApiException;
import com.mysql.cluster.ndbj.NdbTransaction;
//import com.mysql.cluster.ndbj.ExecType; // ndbj-0.7.0
import com.mysql.cluster.ndbj.NdbTransaction.ExecType; // ndbj-0.7.1
import com.mysql.cluster.ndbj.NdbOperation;
import com.mysql.cluster.ndbj.NdbOperation.LockMode;
//import com.mysql.cluster.ndbj.AbortOption; // ndbj-0.7.0
import com.mysql.cluster.ndbj.NdbOperation.AbortOption; // ndbj-0.7.1
import com.mysql.cluster.ndbj.NdbScanOperation;
import com.mysql.cluster.ndbj.NdbIndexScanOperation;
import com.mysql.cluster.ndbj.NdbIndexScanOperation.BoundType;
import com.mysql.cluster.ndbj.NdbResultSet;


/**
 * THE CRUND benchmark implementation against NDB/J aka NDB-Bindings.
 */
public class NdbjLoad extends NdbBase {

    // ----------------------------------------------------------------------
    // NDBJ resources
    // ----------------------------------------------------------------------

    // singleton object representing the NDB cluster (one per process)
    protected NdbClusterConnection mgmd;

    // object representing a connection to an NDB database
    protected Ndb ndb;

    // the benchmark's metadata shortcuts
    protected Model model;

    // object representing an NDB database transaction
    protected NdbTransaction tx;

    // ----------------------------------------------------------------------
    // NDBJ intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        super.initProperties();
        descr = "->ndbj->ndbapi(" + mgmdConnect + ")";
    }

    protected void initLoad() throws Exception {
        // XXX support generic load class
        //super.init();

        // load native library (better diagnostics doing it explicitely)
        out.println();
        loadSystemLibrary("ndbj");

        // instantiate NDB cluster singleton
        out.println();
        out.print("creating cluster conn...");
        out.flush();
        mgmd = NdbClusterConnection.create(mgmdConnect);
        assert mgmd != null;
        out.println("    [ok]");

        // connect to cluster management node (ndb_mgmd)
        out.print("connecting to mgmd ...");
        out.flush();
        final int retries = 0;        // retries (< 0 = indefinitely)
        final int delay = 0;          // seconds to wait after retry
        final boolean verbose = true; // print report of progess
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

    protected void closeLoad() throws Exception {
        out.println();
        out.print("closing mgmd connection ...");
        out.flush();
        if (mgmd != null)
            mgmd.close();
        mgmd = null;
        out.println("     [ok]");

        // XXX support generic load class
        //super.close();
    }

    // ----------------------------------------------------------------------
    // NDBJ operations
    // ----------------------------------------------------------------------

    // returns a string representation of an NdbError
    static protected String toStr(NdbError e) {
        return "NdbError[" + e.getCode() + "]: " + e.getMessage();
    }

    // holds shortcuts to the benchmark's schema information
    static protected class Model {
        public final NdbTable table_A;
        public final NdbTable table_B0;
        public final NdbColumn column_A_id;
        public final NdbColumn column_A_cint;
        public final NdbColumn column_A_clong;
        public final NdbColumn column_A_cfloat;
        public final NdbColumn column_A_cdouble;
        public final NdbColumn column_B0_id;
        public final NdbColumn column_B0_cint;
        public final NdbColumn column_B0_clong;
        public final NdbColumn column_B0_cfloat;
        public final NdbColumn column_B0_cdouble;
        public final NdbColumn column_B0_a_id;
        public final NdbColumn column_B0_cvarbinary_def;
        public final NdbColumn column_B0_cvarchar_def;
        public final NdbIndex idx_B0_a_id;
        public final int attr_id;
        public final int attr_cint;
        public final int attr_clong;
        public final int attr_cfloat;
        public final int attr_cdouble;
        public final int attr_B0_a_id;
        public final int attr_B0_cvarbinary_def;
        public final int attr_B0_cvarchar_def;

        // XXX need names due to broken
        // NdbOperation.getValue(int), NdbResultSet.getXXX(int)
        public final String name_id;
        public final String name_cint;
        public final String name_clong;
        public final String name_cfloat;
        public final String name_cdouble;
        public final String name_B0_a_id;
        public final String name_B0_cvarbinary_def;
        public final String name_B0_cvarchar_def;

        // initialize this instance from the dictionary
        public Model(Ndb ndb) throws NdbApiException {
            final NdbDictionary dict = ndb.getDictionary();

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

            // XXX need names due to broken
            // NdbOperation.getValue(int), NdbResultSet.getXXX(int)
            name_id = column_A_id.getName();
            if (!name_id.equals(column_B0_id.getName()))
                throw new RuntimeException("attribute name mismatch");
            name_cint = column_A_cint.getName();
            if (!name_cint.equals(column_B0_cint.getName()))
                throw new RuntimeException("attribute name mismatch");
            name_clong = column_A_clong.getName();
            if (!name_clong.equals(column_B0_clong.getName()))
                throw new RuntimeException("attribute name mismatch");
            name_cfloat = column_A_cfloat.getName();
            if (!name_cfloat.equals(column_B0_cfloat.getName()))
                throw new RuntimeException("attribute name mismatch");
            name_cdouble = column_A_cdouble.getName();
            if (!name_cdouble.equals(column_B0_cdouble.getName()))
                throw new RuntimeException("attribute name mismatch");
            name_B0_a_id = column_B0_a_id.getName();
            name_B0_cvarbinary_def = column_B0_cvarbinary_def.getName();
            name_B0_cvarchar_def = column_B0_cvarchar_def.getName();
        }
    };

    // some string and byte literals
    final String string1 = "i";
    final String string10 = "xxxxxxxxxx";
    final String string100 = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
    final String string1000 = "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm";
    final byte[] bytes1 = string1.getBytes();
    final byte[] bytes10 = string10.getBytes();
    final byte[] bytes100 = string100.getBytes();
    final byte[] bytes1000 = string1000.getBytes();
    final String[] strings = { string1, string10, string100 };
    final byte[][] bytes = { bytes1, bytes10, bytes100 };

    protected void initOperations() throws NdbApiException {
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
                    public void run(int nOps)
                        throws NdbApiException {
                        ins(model.table_A, 1, nOps, !setAttrs, batch);
                    }
                });

            ops.add(
                new Op("insB0" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        ins(model.table_B0, 1, nOps, !setAttrs, batch);
                    }
                });

            ops.add(
                new Op("setAByPK" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        setByPK(model.table_A, 1, nOps, batch);
                    }
                });

            ops.add(
                new Op("setB0ByPK" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        setByPK(model.table_B0, 1, nOps, batch);
                    }
                });

            ops.add(
                new Op("getAByPK" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        getByPK(model.table_A, 1, nOps, batch);
                    }
                });

            ops.add(
                new Op("getB0ByPK" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        getByPK(model.table_B0, 1, nOps, batch);
                    }
                });

            for (int i = 0, l = 1; l <= maxVarbinaryBytes; l *= 10, i++) {
                final byte[] b = bytes[i];
                assert l == b.length;

                ops.add(
                    new Op("setVarbinary" + l + (batch ? "_batch" : "")) {
                        public void run(int nOps)
                            throws NdbApiException {
                            setVarbinary(model.table_B0, 1, nOps, batch, b);
                        }
                    });

                ops.add(
                    new Op("getVarbinary" + l + (batch ? "_batch" : "")) {
                        public void run(int nOps)
                            throws NdbApiException {
                            getVarbinary(model.table_B0, 1, nOps, batch, b);
                        }
                    });
            }

            for (int i = 0, l = 1; l <= maxVarcharChars; l *= 10, i++) {
                final String s = strings[i];
                assert l == s.length();

                ops.add(
                    new Op("setVarchar" + l + (batch ? "_batch" : "")) {
                        public void run(int nOps)
                            throws NdbApiException {
                            setVarchar(model.table_B0, 1, nOps, batch, s);
                        }
                    });

                ops.add(
                    new Op("getVarchar" + l + (batch ? "_batch" : "")) {
                        public void run(int nOps)
                            throws NdbApiException {
                            getVarchar(model.table_B0, 1, nOps, batch, s);
                        }
                    });
            }

            ops.add(
                new Op("setB0->A" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        setB0ToA(nOps, batch);
                    }
                });

            ops.add(
                new Op("navB0->A" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        navB0ToA(nOps, batch);
                    }
                });

            ops.add(
                new Op("navB0->A_alt" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        navB0ToAalt(nOps, batch);
                    }
                });

            // XXX exclude, NDB/J exceptions
            ops.add(
                new Op("navA->B0" + (forceSend ? "_forceSend" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        navAToB0(nOps, forceSend);
                    }
                });

            // XXX exclude, not implemented yet
            ops.add(
                new Op("navA->B0_alt" + (forceSend ? "_forceSend" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        navAToB0alt(nOps, forceSend);
                    }
                });

            ops.add(
                new Op("nullB0->A" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        nullB0ToA(nOps, batch);
                    }
                });

            ops.add(
                new Op("delB0ByPK" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        delByPK(model.table_B0, 1, nOps, batch);
                    }
                });

            ops.add(
                new Op("delAByPK" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        delByPK(model.table_A, 1, nOps, batch);
                    }
                });

            ops.add(
                new Op("insA_attr" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        ins(model.table_A, 1, nOps, setAttrs, batch);
                    }
                });

            ops.add(
                new Op("insB0_attr" + (batch ? "_batch" : "")) {
                    public void run(int nOps)
                        throws NdbApiException {
                        ins(model.table_B0, 1, nOps, setAttrs, batch);
                    }
                });

            ops.add(
                new Op("delAllB0") {
                    public void run(int nOps)
                        throws NdbApiException {
                        final int count = delByScan(model.table_B0);
                        assert count == nOps;
                    }
                });

            ops.add(
                new Op("delAllA") {
                    public void run(int nOps)
                        throws NdbApiException {
                        final int count = delByScan(model.table_A);
                        assert count == nOps;
                    }
                });
        }

        out.println(" [Op: " + ops.size() + "]");
    }

    protected void closeOperations() {
        out.print("closing operations ...");
        out.flush();
        ops.clear();
        out.println("      [ok]");
    }

    protected void beginTransaction() throws NdbApiException {
        // start a transaction
        // must be closed with NdbTransaction.close
        tx = ndb.startTransaction();
        assert tx != null;
    }

    protected void executeOperations() throws NdbApiException {
        // execute but don't commit the current transaction
        // XXX not documented: return value != 0 v throwing exception
        // YYY Monty: should always throw exception -> void method
        int stat = tx.execute(ExecType.NoCommit, AbortOption.AbortOnError);
        if (stat != 0)
            throw new RuntimeException("stat == " + stat);
    }

    protected void commitTransaction() throws NdbApiException {
        // commit the current transaction
        // XXX not documented: return value != 0 v throwing exception
        // YYY Monty: should always throw exception -> void method
        assert tx != null;
        int stat = tx.execute(ExecType.Commit, AbortOption.AbortOnError);
        if (stat != 0)
            throw new RuntimeException("stat == " + stat);
    }

    protected void rollbackTransaction() throws NdbApiException {
        // abort the current transaction
        // XXX not documented: return value != 0 v throwing exception
        // YYY Monty: should always throw exception -> void method
        int stat = tx.execute(ExecType.Rollback);
        if (stat != 0)
            throw new RuntimeException("stat == " + stat);
    }

    protected void closeTransaction() {
        // close the current transaction
        // to be called irrespectively of success or failure
        tx.close();
        tx = null;
    }

    // ----------------------------------------------------------------------

    protected void fetchCommonAttributes(NdbOperation op)
        throws NdbApiException {
        op.getValue(model.name_cint);
        op.getValue(model.name_clong);
        op.getValue(model.name_cfloat);
        op.getValue(model.name_cdouble);
    }

    protected int getCommonAttributes(NdbResultSet rs)
        throws NdbApiException {
        final int cint = rs.getInt(model.name_cint);
        final long clong = rs.getLong(model.name_clong);
        verify(clong == cint);
        final float cfloat = rs.getFloat(model.name_cfloat);
        verify(cfloat == cint);
        final double cdouble = rs.getDouble(model.name_cdouble);
        verify(cdouble == cint);
        return cint;
    }

    protected void ins(NdbTable table, int from, int to,
                       boolean setAttrs, boolean batch)
        throws NdbApiException {
        beginTransaction();
        for (int i = from; i <= to; i++) {
            // get an insert operation for the table
            final NdbOperation op = tx.getInsertOperation(table);
            assert op != null;

            // set key attribute
            op.equalInt(model.name_id, i);

            // set other attributes
            if (setAttrs) {
                op.setInt(model.name_cint, -i);
                op.setLong(model.name_clong, -i);
                op.setFloat(model.name_cfloat, -i);
                op.setDouble(model.name_cdouble, -i);
            }

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
    }

    protected void delByPK(NdbTable table, int from, int to,
                           boolean batch)
        throws NdbApiException {
        beginTransaction();
        for (int i = from; i <= to; i++) {
            // get a delete operation for the table
            final NdbOperation op = tx.getDeleteOperation(table);
            assert op != null;

            // set key attribute
            op.equalInt(model.name_id, i);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
    }

    protected int delByScan(NdbTable table) throws NdbApiException {
        beginTransaction();

        // get a full table scan operation with exclusive locks
        final NdbScanOperation op
            = tx.getSelectScanOperation(table, LockMode.LM_Exclusive);
        assert op != null;

        // start the scan; don't commit yet
        executeOperations();

        // delete all rows in a given scan
        int count = 0;
        int stat;
        final boolean allowFetch = true; // request new batches when exhausted
        while ((stat = op.nextResult(allowFetch)) == 0) {
            // delete all tuples within a batch
            do {
                op.deleteCurrentTuple();
                count++;

                // XXX execute the operation now if in non-batching mode
                //if (!batch)
                //    executeOperations();
          } while ((stat = op.nextResult(!allowFetch)) == 0);

            if (stat == 1) {
                // no more batches
                break;
            }
            if (stat == 2) {
                // end of current batch, fetch next
                // XXX not documented: return value != 0 v throwing exception
                // YYY Monty: should always throw exception -> void method
                int s = tx.execute(ExecType.NoCommit, AbortOption.AbortOnError);
                if (s != 0)
                    throw new RuntimeException("s == " + s);
                continue;
            }
            throw new RuntimeException("stat == " + stat);
        }
        if (stat != 1)
            throw new RuntimeException("stat == " + stat);

        // close the scan
        op.close();

        commitTransaction();
        closeTransaction();
        return count;
    }

    protected void setByPK(NdbTable table, int from, int to,
                           boolean batch)
        throws NdbApiException {
        beginTransaction();
        for (int i = from; i <= to; i++) {
            // get an insert operation for the table
            final NdbOperation op = tx.getUpdateOperation(table);
            assert op != null;

            // set key attribute
            op.equalInt(model.name_id, i);

            // set other attributes
            op.setInt(model.name_cint, i);
            op.setLong(model.name_clong, i);
            op.setFloat(model.name_cfloat, i);
            op.setDouble(model.name_cdouble, i);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
    }

    // XXX need to use names instead of ids due to broken
    // NdbOperation.getValue(int), NdbResultSet.getXXX(int)
    protected void testBrokenGetValueByIndex()
        throws NdbApiException {
        assert tx == null;
        tx = ndb.startTransaction();
        assert tx != null;
        NdbOperation op = tx.getSelectOperation(model.table_A);
        assert op != null;
        final int int_val = 1;
        op.equalInt("id", int_val);
        op.getValue("id"); // XXX error with index instead of "id"
        final NdbResultSet rs = op.resultData();
        tx.execute(ExecType.Commit, AbortOption.AbortOnError, true);
        while (rs.next()) {
            int id = rs.getInt("id"); // XXX error with index instead of "id"
            assert id == int_val;
        }
        tx.close();
        tx = null;
    }

    protected void getByPK(NdbTable table, int from, int to,
                           boolean batch)
        throws NdbApiException {
        // operation results
        final int count = (to - from) + 1;
        final NdbResultSet[] rss = new NdbResultSet[count];

        beginTransaction();
        for (int i = 0, j = from; i < count; i++, j++) {
            // get a read operation for the table
            NdbOperation op = tx.getSelectOperation(table);
            assert op != null;

            // set key attribute
            op.equalInt(model.name_id, j);

            // define fetched attributes
            op.getValue(model.name_id);
            fetchCommonAttributes(op);

            // get attributes (not readable until after commit)
            rss[i] = op.resultData();

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();

        // check fetched values
        for (int i = 0, j = from; i < count; i++, j++) {
            final NdbResultSet rs = rss[i];
            final boolean hasNext = rs.next();
            assert hasNext;

            // check key attribute
            final int id = rs.getInt(model.name_id);
            verify(id == j);

            // check other attributes
            final int id1 = getCommonAttributes(rs);
            verify(id1 == j);

            assert !rs.next();
        }
        closeTransaction();
    }

    protected void setVarbinary(NdbTable table, int from, int to,
                                boolean batch, byte[] bytes)
        throws NdbApiException {
        beginTransaction();
        for (int i = from; i <= to; i++) {
            // get an update operation for the table
            final NdbOperation op = tx.getUpdateOperation(table);
            assert op != null;

            // set key attribute
            op.equalInt(model.name_id, i);

            // set varbinary
            op.setBytes(model.name_B0_cvarbinary_def, bytes);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
    }

    protected void setVarchar(NdbTable table, int from, int to,
                              boolean batch, String string)
        throws NdbApiException {
        beginTransaction();
        for (int i = from; i <= to; i++) {
            // get an update operation for the table
            final NdbOperation op = tx.getUpdateOperation(table);
            assert op != null;

            // set key attribute
            op.equalInt(model.name_id, i);

            // set varchar
            op.setString(model.name_B0_cvarchar_def, string);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
    }

    protected void getVarbinary(NdbTable table, int from, int to,
                                boolean batch, byte[] bytes)
        throws NdbApiException {
        // operation results
        final int count = (to - from) + 1;
        final NdbResultSet[] rss = new NdbResultSet[count];

        beginTransaction();
        for (int i = 0, j = from; i < count; i++, j++) {
            // get a read operation for the table
            NdbOperation op = tx.getSelectOperation(table);
            assert op != null;

            // set key attribute
            op.equalInt(model.name_id, j);

            // define fetched attributes
            op.getValue(model.name_B0_cvarbinary_def);

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

            // check varbinary
            final byte[] cvarbinary_def
                = rs.getBytes(model.name_B0_cvarbinary_def);
            verify(Arrays.equals(bytes, cvarbinary_def));

            assert !rs.next();
        }
        closeTransaction();
    }

    protected void getVarchar(NdbTable table, int from, int to,
                              boolean batch, String string)
        throws NdbApiException {
        // operation results
        final int count = (to - from) + 1;
        final NdbResultSet[] rss = new NdbResultSet[count];

        beginTransaction();
        for (int i = 0, j = from; i < count; i++, j++) {
            // get a read operation for the table
            NdbOperation op = tx.getSelectOperation(table);
            assert op != null;

            // set key attribute
            op.equalInt(model.name_id, j);

            // define fetched attributes
            op.getValue(model.name_B0_cvarchar_def);

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
                    = rs.getString(model.name_B0_cvarchar_def);
                verify(string.equals(cvarchar_def));
            } else {
                // verification imposes a string->bytes conversion penalty
                final byte[] cvarchar_def
                    = rs.getStringBytes(model.name_B0_cvarchar_def);
                verify(Arrays.equals(string.getBytes(), cvarchar_def));
            }

            assert !rs.next();
        }
        closeTransaction();
    }

    protected void setB0ToA(int nOps,
                            boolean batch)
        throws NdbApiException {
        beginTransaction();
        for (int i = 1; i <= nOps; i++) {
            // get an update operation for the table
            final NdbOperation op = tx.getUpdateOperation(model.table_B0);
            assert op != null;

            // set key attribute
            op.equalInt(model.name_id, i);

            // set a_id attribute
            int a_id = ((i - 1) % nOps) + 1;
            op.setInt(model.name_B0_a_id, a_id);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
    }

    protected void nullB0ToA(int nOps,
                             boolean batch)
        throws NdbApiException {
        beginTransaction();
        for (int i = 1; i <= nOps; i++) {
            // get an update operation for the table
            final NdbOperation op = tx.getUpdateOperation(model.table_B0);
            assert op != null;

            // set key attribute
            op.equalInt(model.name_id, i);

            // set a_id attribute
            int a_id = ((i - 1) % nOps) + 1;
            op.setNull(model.name_B0_a_id);

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();
        closeTransaction();
    }

    protected void navB0ToA(int nOps,
                            boolean batch)
        throws NdbApiException {
        beginTransaction();

        // fetch the foreign keys from B0 and read attributes from A
        final NdbResultSet[] abs = new NdbResultSet[nOps];
        for (int i = 1, j = 0; i <= nOps; i++, j++) {
            // fetch the foreign key value from B0
            NdbResultSet rs;
            {
                // get a read operation for the table
                NdbOperation op = tx.getSelectOperation(model.table_B0);
                assert op != null;

                // set key attribute
                op.equalInt(model.name_id, i);

                // define fetched attributes
                op.getValue(model.name_B0_a_id);

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
                final int a_id = rs.getInt(model.name_B0_a_id);
                assert a_id == ((i - 1) % nOps) + 1;
                op.equalInt(model.name_id, a_id);

                // define fetched attributes
                op.getValue(model.name_id);
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
        for (int i = 1, j = 0; i <= nOps; i++, j++) {
            final NdbResultSet ab = abs[j];
            final boolean hasNext = ab.next();
            assert hasNext;

            // check key attribute
            final int id = ab.getInt(model.name_id);
            //out.println("id = " + id + ", i = " + i);
            verify(id == ((i - 1) % nOps) + 1);

            // check other attributes
            final int k = getCommonAttributes(ab);
            verify(k == id);

            assert !ab.next();
        }
        closeTransaction();
    }

    protected void navB0ToAalt(int nOps,
                               boolean batch)
        throws NdbApiException {
        beginTransaction();

        // fetch the foreign key value from B0
        final NdbResultSet[] a_ids = new NdbResultSet[nOps];
        for (int i = 1, j = 0; i <= nOps; i++, j++) {
                // get a read operation for the table
                NdbOperation op = tx.getSelectOperation(model.table_B0);
                assert op != null;

                // set key attribute
                op.equalInt(model.name_id, i);

                // define fetched attributes
                op.getValue(model.name_B0_a_id);

                // get attributes (not readable until after commit)
                a_ids[j] = op.resultData();
        }
        executeOperations(); // start the scan; don't commit yet

        // fetch the attributes from A
        final NdbResultSet[] abs = new NdbResultSet[nOps];
        for (int i = 1, j = 0; i <= nOps; i++, j++) {
            // get a read operation for the table
            NdbOperation op = tx.getSelectOperation(model.table_A);
            assert op != null;

            // set key attribute
            final int a_id = a_ids[j].getInt(model.name_B0_a_id);
            assert a_id == ((i - 1) % nOps) + 1;
            op.equalInt(model.name_id, a_id);

            // define fetched attributes
            op.getValue(model.name_id);
            fetchCommonAttributes(op);

            // get attributes (not readable until after commit)
            abs[j] = op.resultData();

            // execute the operation now if in non-batching mode
            if (!batch)
                executeOperations();
        }
        commitTransaction();

        // check fetched values
        for (int i = 1, j = 0; i <= nOps; i++, j++) {
            final NdbResultSet ab = abs[j];
            final boolean hasNext = ab.next();
            assert hasNext;

            // check key attribute
            final int id = ab.getInt(model.name_id);
            //out.println("id = " + id + ", i = " + i);
            verify(id == ((i - 1) % nOps) + 1);

            // check other attributes
            final int k = getCommonAttributes(ab);
            verify(k == id);

            assert !ab.next();
        }
        closeTransaction();
    }

    protected void navAToB0(int nOps,
                            boolean forceSend)
        throws NdbApiException {
// throws exceptions, see below:
/*
        beginTransaction();

        // fetch attributes from B0 by foreign key scan
        final NdbResultSet[] abs = new NdbResultSet[nOps];
        int j = 0;
        for (int i = 1; i <= nOps; i++) {
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
            //    [java] caught com.mysql.cluster.ndbj.NdbApiException:
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
            op.getValue(model.name_id);
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
        assert (j++ == nOps);

        // check fetched values
        j = 0;
        for (int i = 1; i <= nOps; i++) {
            final NdbResultSet ab = abs[j++];
            //out.println("j = " + j + ", ab = " + ab);
            //final boolean hasNext = ab.next();
            // throws
            //[java] j = 1, ab = com.mysql.cluster.ndbj.NdbResultSetImpl@6f144c
            //    [java] caught com.mysql.cluster.ndbj.NdbApiException: Unknown error code
            //    [java] com.mysql.cluster.ndbj.NdbApiException: Unknown error code
            //    [java] at com.mysql.cluster.ndbj.NdbjJNI.NdbScanOperationImpl_nextResult__SWIG_(Native Method)
            //    [java] at com.mysql.cluster.ndbj.NdbScanOperationImpl.nextResult(NdbScanOperationImpl.java:93)
            //    [java] at com.mysql.cluster.ndbj.NdbResultSetImpl.next(NdbResultSetImpl.java:362)
            //    [java] at com.mysql.cluster.crund.NdbjLoad.navAToB0(NdbjLoad.java:1205)
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
            final int id = ab.getInt(model.name_id);
            verify(id == i);

            // check other attributes
            final int id1 = getCommonAttributes(ab);
            verify(id1 == i);

            assert !ab.next();
        }
        closeTransaction();
*/
    }

    protected void navAToB0alt(int nOps,
                               boolean forceSend)
        throws NdbApiException {
// XXX not implemented yet, fix exception in navAToB0() first
/*
        assert false;
*/
    }

    // ----------------------------------------------------------------------
    // NDBJ datastore operations
    // ----------------------------------------------------------------------

    protected void initConnection() throws NdbApiException {
        out.println();

        // optionally, connect and wait for reaching the data nodes (ndbds)
        out.print("waiting for ndbd ...");
        out.flush();
        final int initial_wait = 10; // secs to wait until first node detected
        final int final_wait = 0;    // secs to wait after first node detected

        // XXX return: 0 all nodes live, > 0 at least one node live, < 0 error
        try {
            mgmd.waitUntilReady(initial_wait, final_wait);
        } catch (NdbApiException e) {
            out.println();
            out.println("!!! data nodes were not ready within "
                        + (initial_wait + final_wait) + "s.");
            throw e;
        }
        out.println("            [ok]");

        // connect to database
        out.print("connecting to ndbd ...");
        out.flush();
        try {
            // XXX where to set schema?
            // YYY Frazer: schema not too useful in NDB at the moment
            // XXX unclear if maxThreads ^= maxNumberOfTransactions
            //     since ndb.init(maxNumberOfTransactions) is deprecated
            //final int maxThreads = 4;
            //ndb = mgmd.createNdb(catalog, maxThreads);
            // YYY Frazer: yes, maxThreads == maxNumber(concurrent)OfTransactions
            ndb = mgmd.createNdb(catalog);
        } catch (NdbApiException e) {
            out.println();
            out.println("!!! failed to connect: " + e);
            throw e;
        }
        out.println("          [ok]");

        // initialize the schema shortcuts
        model = new Model(ndb);
    }

    protected void closeConnection() {
        out.println();
        out.print("closing ndbd connection ...");
        out.flush();
        model = null;
        ndb.close();
        ndb = null;
        out.println("     [ok]");
    }

    protected void clearData() throws NdbApiException {
        out.print("deleting all rows ...");
        out.flush();
        final int delB0 = delByScan(model.table_B0);
        out.print("           [B0: " + delB0);
        out.flush();
        final int delA = delByScan(model.table_A);
        out.print(", A: " + delA);
        out.flush();
        out.println("]");
    }

    // ----------------------------------------------------------------------

    static public void main(String[] args) {
        System.out.println("NdbjLoad.main()");
        parseArguments(args);
        new NdbjLoad().run();
        System.out.println();
        System.out.println("NdbjLoad.main(): done.");
    }
}
