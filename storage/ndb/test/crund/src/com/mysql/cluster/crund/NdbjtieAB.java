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

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

import java.util.Arrays;
import java.util.List;
import java.util.ArrayList;
import java.util.Properties;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.CharsetEncoder;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CoderResult;
import java.nio.charset.CharacterCodingException;

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
import com.mysql.ndbjtie.ndbapi.NdbOperationConst;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation.ScanFlag;
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation.BoundType;
import com.mysql.ndbjtie.ndbapi.NdbRecAttr;
//import com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode;
import com.mysql.ndbjtie.ndbapi.NdbTransaction.ExecType;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst.AbortOption;

import com.mysql.cluster.crund.CrundDriver.XMode;

/**
 * The NDB JTie benchmark implementation.
 */
public class NdbjtieAB extends CrundLoad {
    // NDB settings
    protected String mgmdConnect;
    protected String catalog;
    protected String schema;
    protected int nMaxConcTx;
    protected int nConcScans;

    // NDB JTie resources
    protected Ndb_cluster_connection mgmd;
    protected Ndb ndb;
    protected NdbTransaction tx;
    protected int ndbOpLockMode;
    protected Model model;

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

    public NdbjtieAB(CrundDriver driver) {
        super(driver);
    }

    static public void main(String[] args) {
        System.out.println("NdbjtieAB.main()");
        CrundDriver.parseArguments(args);
        final CrundDriver driver = new CrundDriver();
        final CrundLoad load = new NdbjtieAB(driver);
        driver.run();
        System.out.println();
        System.out.println("NdbjtieAB.main(): done.");
    }

    // ----------------------------------------------------------------------
    // NDB JTie intializers/finalizers
    // ----------------------------------------------------------------------

   protected void initProperties() {
        out.println();
        out.print("reading NDB properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");
        final Properties props = driver.props;

        mgmdConnect = props.getProperty("ndb.mgmdConnect", "localhost");
        assert mgmdConnect != null;
        catalog = props.getProperty("ndb.catalog", "crunddb");
        assert catalog != null;
        schema = props.getProperty("ndb.schema", "def");
        assert schema != null;

        nMaxConcTx = driver.parseInt("ndb.nMaxConcTx", 1024);
        if (nMaxConcTx < 1) {
            msg.append("[IGNORED] ndb.nMaxConcTx:    " + nMaxConcTx + eol);
            nMaxConcTx = 1024;
        }

        nConcScans = driver.parseInt("ndb.nConcScans", 255);
        if (nConcScans < 1) {
            msg.append("[IGNORED] ndb.nConcScans:    " + nConcScans + eol);
            nConcScans = 255;
        }

        if (msg.length() == 0) {
            out.println("      [ok]");
        } else {
            driver.hasIgnoredSettings = true;
            out.println();
            out.print(msg.toString());
        }

        name = "ndbjtie"; // shortcut will do, "(" + mgmdConnect + ")";
    }

    protected void printProperties() {
        out.println();
        out.println("NDB settings ...");
        out.println("ndb.mgmdConnect:                \"" + mgmdConnect + "\"");
        out.println("ndb.catalog:                    \"" + catalog + "\"");
        out.println("ndb.schema:                     \"" + schema + "\"");
        out.println("ndb.nMaxConcTx:                 " + nMaxConcTx);
        out.println("ndb.nConcScans:                 " + nConcScans);
    }

    public void init() throws Exception {
        assert mgmd == null;
        super.init();

        // load native library (better diagnostics doing it explicitely)
        out.println();
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
    // NDB JTie Crund metadata
    // ----------------------------------------------------------------------

    // returns a string representation of an NdbError
    static protected String toStr(NdbErrorConst e) {
        return "NdbError[" + e.code() + "]: " + e.message();
    }

    // returns the width of a column in bytes
    static protected int columnWidth(ColumnConst c) {
        int s = c.getSize(); // size of type or of base type
        int al = c.getLength(); // length or max length, 1 for scalars
        int at = c.getArrayType(); // size of length prefix, practically
        return (s * al) + at;
    }

    // the benchmark's schema information
    static protected class Model {
        // dictionary objects
        public final TableConst table_A;
        public final TableConst table_B;
        public final ColumnConst column_A_id;
        public final ColumnConst column_A_cint;
        public final ColumnConst column_A_clong;
        public final ColumnConst column_A_cfloat;
        public final ColumnConst column_A_cdouble;
        public final ColumnConst column_B_id;
        public final ColumnConst column_B_cint;
        public final ColumnConst column_B_clong;
        public final ColumnConst column_B_cfloat;
        public final ColumnConst column_B_cdouble;
        public final ColumnConst column_B_aid;
        public final ColumnConst column_B_cvarbinary_def;
        public final ColumnConst column_B_cvarchar_def;
        public final IndexConst idx_B_aid;

        // attribute ids
        public final int attr_id;
        public final int attr_cint;
        public final int attr_clong;
        public final int attr_cfloat;
        public final int attr_cdouble;
        public final int attr_B_aid;
        public final int attr_B_cvarbinary_def;
        public final int attr_B_cvarchar_def;
        public final int attr_idx_B_aid;

        // widths
        public final int width_id;
        public final int width_cint;
        public final int width_clong;
        public final int width_cfloat;
        public final int width_cdouble;
        public final int width_B_aid;
        public final int width_B_cvarbinary_def; // width including prefix
        public final int width_B_cvarchar_def; // width including prefix
        public final int width_A_row; // sum of width of columns in A
        public final int width_B_row; // sum of width of columns in B
        public final int width_AB_row; // sum of width of common columns in A,B
        public final int wprefix_B_cvarbinary_def; // width of length prefix
        public final int wprefix_B_cvarchar_def; // width of length prefix

        // initialize this instance from the dictionary
        public Model(Ndb ndb) {
            final Dictionary dict = ndb.getDictionary();

            // get columns of table A
            // problems finding table name if upper case
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

            // get columns of table B
            if ((table_B = dict.getTable("b")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B_id = table_B.getColumn("id")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B_cint = table_B.getColumn("cint")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B_clong = table_B.getColumn("clong")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B_cfloat = table_B.getColumn("cfloat")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B_cdouble = table_B.getColumn("cdouble")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B_aid = table_B.getColumn("a_id")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B_cvarbinary_def = table_B.getColumn("cvarbinary_def")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));
            if ((column_B_cvarchar_def = table_B.getColumn("cvarchar_def")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));

            // get indexes of table B
            if ((idx_B_aid = dict.getIndex("I_B_FK", "b")) == null)
                throw new RuntimeException(toStr(dict.getNdbError()));

            // get common attribute ids for tables A, B
            attr_id = column_A_id.getColumnNo();
            if (attr_id != column_B_id.getColumnNo())
                throw new RuntimeException("attribute id mismatch");
            attr_cint = column_A_cint.getColumnNo();
            if (attr_cint != column_B_cint.getColumnNo())
                throw new RuntimeException("attribute id mismatch");
            attr_clong = column_A_clong.getColumnNo();
            if (attr_clong != column_B_clong.getColumnNo())
                throw new RuntimeException("attribute id mismatch");
            attr_cfloat = column_A_cfloat.getColumnNo();
            if (attr_cfloat != column_B_cfloat.getColumnNo())
                throw new RuntimeException("attribute id mismatch");
            attr_cdouble = column_A_cdouble.getColumnNo();
            if (attr_cdouble != column_B_cdouble.getColumnNo())
                throw new RuntimeException("attribute id mismatch");

            // get extra attribute ids for table B
            attr_B_aid = column_B_aid.getColumnNo();
            attr_B_cvarbinary_def = column_B_cvarbinary_def.getColumnNo();
            attr_B_cvarchar_def = column_B_cvarchar_def.getColumnNo();

            // get attribute ids for columns in index B_aid
            attr_idx_B_aid = idx_B_aid.getColumn(0).getColumnNo();

            // get the width of common columns in tables A, B
            width_id = columnWidth(column_A_id);
            if (width_id != columnWidth(column_B_id))
                throw new RuntimeException("attribute id mismatch");
            width_cint = columnWidth(column_A_cint);
            if (width_cint != columnWidth(column_B_cint))
                throw new RuntimeException("attribute id mismatch");
            width_clong = columnWidth(column_A_clong);
            if (width_clong != columnWidth(column_B_clong))
                throw new RuntimeException("attribute id mismatch");
            width_cfloat = columnWidth(column_A_cfloat);
            if (width_cfloat != columnWidth(column_B_cfloat))
                throw new RuntimeException("attribute id mismatch");
            width_cdouble = columnWidth(column_A_cdouble);
            if (width_cdouble != columnWidth(column_B_cdouble))
                throw new RuntimeException("attribute id mismatch");

            // get the width of extra columns in table B
            width_B_aid = columnWidth(column_B_aid);
            width_B_cvarbinary_def = columnWidth(column_B_cvarbinary_def);
            width_B_cvarchar_def = columnWidth(column_B_cvarchar_def);

            // row width of tables A, B
            width_A_row = (
                + width_id
                + width_cint
                + width_clong
                + width_cfloat
                + width_cdouble);
            width_B_row = (
                + width_id
                + width_cint
                + width_clong
                + width_cfloat
                + width_cdouble
                + width_B_aid
                + width_B_cvarbinary_def
                + width_B_cvarchar_def);
            width_AB_row = (
                + width_id
                + width_cint
                + width_clong
                + width_cfloat
                + width_cdouble);

            // get the width of the length prefix of columns in table B
            wprefix_B_cvarbinary_def = column_B_cvarbinary_def.getArrayType();
            wprefix_B_cvarchar_def = column_B_cvarchar_def.getArrayType();
        }
    };

    // ----------------------------------------------------------------------
    // NDB JTie operations
    // ----------------------------------------------------------------------

    // current model assumption: relationships only 1:1 identity
    // (target id of a navigation operation is verified against source id)
    protected abstract class NdbjtieOp extends Op {
        protected final XMode xMode;

        public NdbjtieOp(String name, XMode m) {
            super(name + "," + m);
            this.xMode = m;
        }
    };

    protected abstract class WriteOp extends NdbjtieOp {
        protected final TableConst table;
        protected NdbOperation op;

        public WriteOp(String name, XMode m, TableConst table) {
            super(name, m);
            this.table = table;
        }

        public void run(int[] id) {
            final int n = id.length;
            switch (xMode) {
            case indy :
                for (int i = 0; i < n; i++) {
                    beginTransaction();
                    write(id[i]);
                    commitTransaction();
                    closeTransaction();
                }
                break;
            case each :
                beginTransaction();
                for (int i = 0; i < n; i++) {
                    write(id[i]);
                    executeOperations();
                }
                commitTransaction();
                closeTransaction();
                break;
            case bulk :
                beginTransaction();
                write(id);
                commitTransaction();
                closeTransaction();
                break;
            }
        }

        protected void write(int[] id) {
            for (int i = 0; i < id.length; i++)
                write(id[i]);
        }

        protected void write(int id) {
            setOp();
            setValues(id);
        }

        protected abstract void setOp();

        protected abstract void setValues(int id);
    }

    protected abstract class UpdateOp extends WriteOp {
        public UpdateOp(String name, XMode m, TableConst table) {
            super(name, m, table);
        }

        protected final void setOp() {
            op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.updateTuple() != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
        }
    }

    protected abstract class InsertOp extends WriteOp {
        public InsertOp(String name, XMode m, TableConst table) {
            super(name, m, table);
        }

        protected final void setOp() {
            op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.insertTuple() != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
        }
    }

    protected abstract class DeleteOp extends WriteOp {
        public DeleteOp(String name, XMode m, TableConst table) {
            super(name, m, table);
        }

        protected final void setOp() {
            op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.deleteTuple() != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
        }
    }

    protected abstract class ReadOp extends NdbjtieOp {
        protected final TableConst table;
        protected NdbOperation op;

        public ReadOp(String name, XMode m, TableConst table) {
            super(name, m);
            this.table = table;
        }

        public void run(int[] id) {
            final int n = id.length;
            switch (xMode) {
            case indy :
                for (int i = 0; i < n; i++) {
                    beginTransaction();
                    alloc(1);
                    rewind();
                    read(id[i]);
                    commitTransaction();
                    rewind();
                    check(id[i]);
                    free();
                    closeTransaction();
                }
                break;
            case each :
                beginTransaction();
                alloc(1);
                for (int i = 0; i < n; i++) {
                    rewind();
                    read(id[i]);
                    executeOperations();
                    rewind();
                    check(id[i]);
                }
                commitTransaction();
                free(); // ok outside loop, no cloned RecAttrs
                closeTransaction();
                break;
            case bulk :
                beginTransaction();
                alloc(n);
                rewind();
                read(id);
                executeOperations();
                rewind();
                check(id);
                commitTransaction();
                free();
                closeTransaction();
                break;
            }
        }

        protected void read(int[] id) {
            for (int i = 0; i < id.length; i++)
                read(id[i]);
        }

        protected void read(int id) {
            setOp();
            getValues(id);
        }

        protected final void setOp() {
            op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            if (op.readTuple(ndbOpLockMode) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
        }

        protected abstract void alloc(int n);
        protected abstract void rewind();
        protected abstract void free();

        protected abstract void getValues(int id);

        protected void check(int[] id) {
            for (int i = 0; i < id.length; i++)
                check(id[i]);
        }

        protected abstract void check(int id);
    }

    protected abstract class BBReadOp extends ReadOp {
        protected final int rowWidth;
        protected ByteBuffer bb;

        public BBReadOp(String name, XMode m, TableConst table, int rowWidth) {
            super(name, m, table);
            this.rowWidth = rowWidth;
        }

        protected void alloc(int n) {
            bb = ByteBuffer.allocateDirect(rowWidth * n);
            bb.order(bo); // initial order is BIG_ENDIAN
        }

        protected void rewind() {
            bb.rewind(); // prepare buffer for reading
        }

        protected void free() {
            bb = null;
        }
    }

    protected abstract class RAReadOp<H> extends ReadOp {
        protected final Class<H> cls;
        protected List<H> ah;
        protected int pos;

        public RAReadOp(String name, XMode m, TableConst table, Class<H> cls) {
            super(name, m, table);
            this.cls = cls;
        }

        protected void alloc(int n) {
            try {
                ah = new ArrayList<H>(n);
                for (int i = 0; i < n; i++)
                    ah.add(cls.newInstance());
            } catch (InstantiationException ex) {
                throw new RuntimeException(ex);
            } catch (IllegalAccessException ex) {
                throw new RuntimeException(ex);
            }
            pos = 0;
        }

        protected void rewind() {
            pos = 0;
        }

        protected void free() {
            ah = null;
        }
    }

    protected abstract class IndexScanOp extends NdbjtieOp {
        protected final IndexConst index;
        protected final boolean forceSend;
        protected NdbIndexScanOperation op[];

        public IndexScanOp(String name, XMode m, IndexConst index) {
            super(name, m);
            this.index = index;
            this.forceSend = true; // no send delay for 1-thread app
        }

        public void run(int[] id) {
            final int n = id.length;
            switch (xMode) {
            case indy :
                for (int i = 0; i < n; i++) {
                    beginTransaction();
                    op = new NdbIndexScanOperation[1];
                    final int o = 0; // scan op index
                    alloc(1); // not needed
                    read(o, id[i]);
                    executeOperations();
                    rewind();
                    fetch(o, id[i]);
                    commitTransaction();
                    rewind();
                    check(id[i]);
                    free();
                    closeTransaction();
                }
                break;
            case each :
                beginTransaction();
                op = new NdbIndexScanOperation[1];
                final int o = 0; // scan op index
                for (int i = 0; i < n; i++) {
                    alloc(1);
                    rewind();
                    read(o, id[i]);
                    executeOperations();
                    rewind();
                    fetch(o, id[i]);
                    rewind();
                    check(id[i]);
                    free(); // delete RecAttrs cloned within loop
                }
                commitTransaction();
                closeTransaction();
                break;
            case bulk :
                beginTransaction();
                final int bs = nConcScans; // batch size
                op = new NdbIndexScanOperation[bs];
                for (int b = 0; b < n; b += bs) {
                    final int e = ((b + bs) < n ? (b + bs) : n);
                    final int[] idb = Arrays.copyOfRange(id, b, e);
                    assert idb.length <= bs;
                    alloc(bs);
                    read(idb);
                    executeOperations();
                    rewind();
                    fetch(idb);
                    rewind();
                    check(idb);
                    free();
                }
                commitTransaction();
                closeTransaction();
                break;
            }
        }

        protected void read(int[] id) {
            final int n = id.length;
            assert n <= nConcScans;
            for (int i = 0; i < n; i++) {
                rewind();
                final int o = i; // scan op index
                read(o, id[i]);
            }
        }

        protected void read(int o, int id) {
            setOp(o);
            getValues(o, id);
        }

        protected final void setOp(int o) {
            final NdbIndexScanOperation iso
                = tx.getNdbIndexScanOperation(index);
            if (iso == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            op[o] = iso;

            // define a read scan
            final int lockMode = ndbOpLockMode; // LockMode.LM_CommittedRead;
            final int scanFlags = 0 | ScanFlag.SF_OrderBy; // sort on index
            final int parallel = 0; // #fragments to scan in parallel (0=max)
            final int batch = 0; // #rows to fetch in each batch
            if (iso.readTuples(lockMode, scanFlags, parallel, batch) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
        }

        protected void fetch(int[] id) {
            final int n = id.length;
            assert n <= nConcScans;
            for (int i = 0; i < n; i++) {
                final int o = i; // scan op index
                fetch(o, id[i]);
            }
        }

        protected void fetch(int o, int id) {
            // read the result set executing the defined read operations
            final NdbIndexScanOperation iso = op[o];
            int stat;
            final boolean allowFetch = true; // request batches when needed
            while ((stat = iso.nextResult(allowFetch, forceSend)) == 0)
                copy(o);
            if (stat != 1)
                throw new RuntimeException(toStr(tx.getNdbError()));

            // close the scan, no harm in delaying/accumulating close()
            final boolean releaseOp = true;
            iso.close(!forceSend, !releaseOp);
        }

        protected abstract void alloc(int n);
        protected abstract void rewind();
        protected abstract void copy(int o);
        protected abstract void free();

        protected abstract void getValues(int o, int id);

        protected void check(int[] id) {
            final int n = id.length;
            for (int i = 0; i < n; i++)
                check(id[i]);
        }

        protected abstract void check(int id);
    }

    protected abstract class BBIndexScanOp extends IndexScanOp {
        protected final int rowWidth;
        protected ByteBuffer b; // current result row of current scan
        protected ByteBuffer bb; // collected scan results

        public BBIndexScanOp(String name, XMode m,
                             IndexConst index, int rowWidth) {
            super(name, m, index);
            this.rowWidth = rowWidth;
        }

        protected void alloc(int n) {
            assert 0 <= n && n <= nConcScans;
            b = ByteBuffer.allocateDirect(rowWidth);
            b.order(bo); // initial order is BIG_ENDIAN
            bb = ByteBuffer.allocateDirect(rowWidth * n);
            bb.order(bo); // initial order is BIG_ENDIAN
        }

        protected void rewind() {
            b.rewind(); // prepare buffer for reading
            bb.rewind(); // prepare buffer for reading
        }

        protected void copy(int o) {
            b.rewind(); // prepare buffer for reading
            bb.put(b);
        }

        protected void free() {
            b = null;
            bb = null;
        }
    }

    protected abstract class RAIndexScanOp<H extends RecHolder>
        extends IndexScanOp {
        protected final Class<H> cls;
        protected H[] oh; // RecHolder per scan operation, managed by operation
        protected List<H> ah; // RecHolder per scan result row, cloned & freed
        protected int pos;

        public RAIndexScanOp(String name, XMode m,
                             IndexConst index, Class<H> cls) {
            super(name, m, index);
            this.cls = cls;
        }

        @SuppressWarnings("unchecked")
        protected void alloc(int n) {
            assert 0 <= n && n <= nConcScans;
            // 'unchecked' warning, no API to get from Class<H> cls
            // to a Class<H[]> instance for an explicit call to cast()
            oh = (H[])Array.newInstance(cls, n); // max: nConcScans
            try {
                for (int i = 0; i < n; i++)
                    oh[i] = cls.newInstance();
            } catch (Exception ex) {
                throw new RuntimeException(ex);
            }

            ah = new ArrayList<H>(n);
            for (int i = 0; i < n; i++)
                ah.add(null);
            pos = 0;
        }

        protected void rewind() {
            pos = 0;
        }

        protected void copy(int o) {
            // save clone of NdbRecAttrs in RecHolder reused by scan operation
            final H c = cls.cast(oh[o].clone());
            //ah.add(c);
            ah.set(pos++, c);
        }

        protected void free() {
            for (H a : ah)
                if (a != null)
                    a.delete(); // cloned NdbRecAttrs lifecycle-managed by app
            ah.clear();
            oh = null; // getValue()-returned NdbRecAttrs managed by NDB
        }
    }

// ----------------------------------------------------------------------

    protected void initOperations() {
        out.print("initializing operations ...");
        out.flush();

        //out.println("default charset: "
        //    + java.nio.charset.Charset.defaultCharset().displayName());

        for (XMode m : driver.xModes) {
            // inner classes can only refer to a constant
            final XMode xMode = m;

            ops.add(
                new InsertOp("A_insAttr", xMode, model.table_A) {
                    public void setValues(int id) {
                        setKeyA(op, id); // needs to be set first
                        setAttrA(op, -id);
                    }
                });

            ops.add(
                new InsertOp("B_insAttr", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setAttrB(op, -id);
                    }
                });

            ops.add(
                new UpdateOp("A_setAttr", xMode, model.table_A) {
                    public void setValues(int id) {
                        setKeyA(op, id); // needs to be set first
                        setAttrA(op, id);
                    }
                });

            ops.add(
                new UpdateOp("B_setAttr", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setAttrB(op, id);
                    }
                });

            ops.add(
                new BBReadOp("A_getAttr_bb", xMode,
                             model.table_A, model.width_AB_row) {
                    protected void getValues(int id) {
                        setKeyA(op, id); // needs to be set first
                        getKeyA(op, bb);
                        getAttrA(op, bb);
                    }

                    protected void check(int id) {
                        checkKeyA(id, bb);
                        checkAttrA(id, bb);
                    }
                });

            ops.add(
                new RAReadOp<RecAttrHolder>("A_getAttr_ra", xMode,
                             model.table_A, RecAttrHolder.class) {
                    protected void getValues(int id) {
                        setKeyA(op, id); // needs to be set first
                        final RecAttrHolder h = ah.get(pos++);
                        getKeyA(op, h);
                        getAttrA(op, h);
                    }

                    protected void check(int id) {
                        final RecAttrHolder h = ah.get(pos++);
                        checkKeyA(id, h);
                        checkAttrA(id, h);
                    }
                });

            ops.add(
                new BBReadOp("B_getAttr_bb", xMode,
                             model.table_B, model.width_AB_row) {
                    protected void getValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        getKeyB(op, bb);
                        getAttrB(op, bb);
                    }

                    protected void check(int id) {
                        checkKeyB(id, bb);
                        checkAttrB(id, bb);
                    }
                });

            ops.add(
                new RAReadOp<RecAttrHolder>("B_getAttr_ra", xMode,
                             model.table_B, RecAttrHolder.class) {
                    protected void getValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        final RecAttrHolder h = ah.get(pos++);
                        getKeyB(op, h);
                        getAttrB(op, h);
                    }

                    protected void check(int id) {
                        final RecAttrHolder h = ah.get(pos++);
                        checkKeyB(id, h);
                        checkAttrB(id, h);
                    }
                });

            for (int i = 0; i < bytes.length; i++) {
                // inner classes can only refer to a constant
                final byte[] b = bytes[i];
                final int l = b.length;
                if (l > driver.maxVarbinaryBytes)
                    break;

                if (l > (model.width_B_cvarbinary_def
                         - model.wprefix_B_cvarbinary_def)) {
                    String msg = "property maxVarbinaryBytes > |B.cvarbinary|";
                    throw new RuntimeException(msg);
                }

            ops.add(
                new UpdateOp("B_setVarbin_" + l, xMode,
                             model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setVarbinaryB(op, b);
                    }
                });

            ops.add(
                new BBReadOp("B_getVarbin_" + l, xMode,
                             model.table_B, model.width_B_cvarbinary_def) {

                    protected void getValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        getVarbinaryB(op, bb);
                    }

                    protected void check(int id) {
                        checkVarbinaryB(b, bb);
                    }
                });

            ops.add(
                new UpdateOp("B_clearVarbin_" + l, xMode,
                             model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setVarbinaryB(op, null);
                    }
                });
            }

            for (int i = 0; i < strings.length; i++) {
                // inner classes can only refer to a constant
                final String s = strings[i];
                final int l = s.length();
                if (l > driver.maxVarcharChars)
                    break;

                if (l > (model.width_B_cvarchar_def
                         - model.wprefix_B_cvarchar_def)) {
                    String msg = "property maxVarcharChars > |B.cvarchar|";
                    throw new RuntimeException(msg);
                }

            ops.add(
                new UpdateOp("B_setVarchar_" + l, xMode,
                             model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setVarcharB(op, s);
                    }
                });

            ops.add(
                new BBReadOp("B_getVarchar_" + l, xMode,
                             model.table_B, model.width_B_cvarchar_def) {

                    protected void getValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        getVarcharB(op, bb);
                    }

                    protected void check(int id) {
                        checkVarcharB(s, bb);
                    }
                });

            ops.add(
                new UpdateOp("B_clearVarchar_" + l, xMode,
                             model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setVarcharB(op, null);
                    }
                });
            }

            ops.add(
                new UpdateOp("B_setA", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        final int aid = id;
                        setAIdB(op, aid);
                    }
                });

            ops.add(new BBReadOp("B_getA_bb", xMode,
                                 model.table_A, model.width_AB_row) {
                    // sub-query
                    protected final BBReadOp getAId
                    = new BBReadOp("B_getAId_bb", xMode,
                                   model.table_B, model.width_B_aid) {
                            protected void getValues(int id) {
                                setKeyB(op, id); // needs to be set first
                                getAIdB(op, bb);
                            }

                            protected void check(int id) {}
                        };

                    protected void alloc(int n) {
                        getAId.alloc(n);
                        super.alloc(n);
                    }

                    protected void free() {
                        getAId.free();
                        super.free();
                    }

                    protected void read(int[] id) {
                        // run sub-query
                        getAId.rewind();
                        getAId.read(id);
                        executeOperations();

                        // run this query
                        // cannot call into super.read(int[]) -> this.read(int)
                        getAId.rewind();
                        final IntBuffer aid = getAId.bb.asIntBuffer();
                        while (aid.hasRemaining())
                            super.read(aid.get());
                    }

                    protected void read(int id) {
                        // run sub-query
                        getAId.rewind();
                        getAId.read(id);
                        executeOperations();

                        // run this query
                        getAId.rewind();
                        final int aid = getAId.bb.getInt();
                        super.read(aid);
                    }

                    protected void getValues(int id) {
                        setKeyA(op, id); // needs to be set first
                        getKeyA(op, bb);
                        getAttrA(op, bb);
                    }

                    protected void check(int id) {
                        checkKeyA(id, bb);
                        checkAttrA(id, bb);
                    }
                });

            ops.add(new RAReadOp<RecAttrHolder>("B_getA_ra", xMode,
                                 model.table_A, RecAttrHolder.class) {
                    // sub-query
                    protected final RAReadOp<RecIdHolder> getAId
                    = new RAReadOp<RecIdHolder>("B_getAId_ra", xMode,
                                   model.table_B, RecIdHolder.class) {
                            protected void getValues(int id) {
                                setKeyB(op, id); // needs to be set first
                                final RecIdHolder h = ah.get(pos++);
                                getAIdB(op, h);
                            }

                            protected void check(int id) {}
                        };

                    protected void alloc(int n) {
                        getAId.alloc(n);
                        super.alloc(n);
                    }

                    protected void free() {
                        getAId.free();
                        super.free();
                    }

                    protected void read(int[] id) {
                        // run sub-query
                        getAId.rewind();
                        getAId.read(id);
                        executeOperations();

                        // run this query
                        // cannot call into super.read(int[]) -> this.read(int)
                        getAId.rewind();
                        for (RecIdHolder h : getAId.ah)
                            super.read(h.id.int32_value());
                    }

                    protected void read(int id) {
                        // run sub-query
                        getAId.rewind();
                        getAId.read(id);
                        executeOperations();

                        // run this query
                        getAId.rewind();
                        final RecIdHolder h = getAId.ah.get(getAId.pos++);
                        super.read(h.id.int32_value());
                    }

                    protected void getValues(int id) {
                        setKeyA(op, id); // needs to be set first
                        final RecAttrHolder h = ah.get(pos++);
                        getKeyA(op, h);
                        getAttrA(op, h);
                    }

                    protected void check(int id) {
                        final RecAttrHolder h = ah.get(pos++);
                        checkKeyA(id, h);
                        checkAttrA(id, h);
                    }
                });

            ops.add(new BBIndexScanOp("A_getBs_bb", xMode,
                                      model.idx_B_aid, model.width_AB_row) {
                    protected final void setBounds(int o, int id) {
                        setBoundEqAIdB(op[o], id);
                    }

                    protected void getValues(int o, int id) {
                        setBounds(o, id);
                        getKeyB(op[o], b);
                        getAttrB(op[o], b);
                    }

                    protected void check(int id) {
                        checkKeyB(id, bb);
                        checkAttrB(id, bb);
                    }
                });

            ops.add(new RAIndexScanOp<RecAttrHolder>("A_getBs_ra", xMode,
                                                     model.idx_B_aid,
                                                     RecAttrHolder.class) {
                    protected final void setBounds(int o, int id) {
                        setBoundEqAIdB(op[o], id);
                    }

                    protected void getValues(int o, int id) {
                        setBounds(o, id);
                        getKeyB(op[o], oh[o]);
                        getAttrB(op[o], oh[o]);
                    }

                    protected void check(int id) {
                        final RecAttrHolder h = ah.get(pos++);
                        checkKeyB(id, h);
                        checkAttrB(id, h);
                    }
                });

            ops.add(
                new UpdateOp("B_clearA", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        final int aid = -1;
                        setAIdB(op, aid);
                    }
                });

            ops.add(
                new DeleteOp("B_del", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id);
                    }
                });

            ops.add(
                new DeleteOp("A_del", xMode, model.table_A) {
                    public void setValues(int id) {
                        setKeyA(op, id);
                    }
                });

            ops.add(
                new InsertOp("A_ins", xMode, model.table_A) {
                    public void setValues(int id) {
                        setKeyA(op, id);
                    }
                });

            ops.add(
                new InsertOp("B_ins", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id);
                    }
                });

            ops.add(
                new NdbjtieOp("B_delAll", XMode.bulk) {
                    public void run(int[] id) {
                        final int d = delByScan(model.table_B);
                        verify(id.length, d);
                    }
                });

            ops.add(
                new NdbjtieOp("A_delAll", XMode.bulk) {
                    public void run(int[] id) {
                        final int d = delByScan(model.table_A);
                        verify(id.length, d);
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

    protected void clearPersistenceContext() throws Exception {
        // nothing to do as we're not caching beyond Tx scope
    }

    // ----------------------------------------------------------------------

    static protected class RecHolder implements Cloneable {
        public RecHolder clone() {
            try {
                return (RecHolder)super.clone();
            } catch (CloneNotSupportedException e) {
                throw new RuntimeException(e);
            }
        }

        public void delete() {}

        // renamed, generic method measurably slower than hard-coded version
        // generically clones this object using NdbRecAttr.cloneNative()
        // on all public fields of this instance
        public RecHolder cloneGenerically() {
            final RecHolder c;
            try {
                c = (RecHolder)super.clone();
                final Class<NdbRecAttr> nra = NdbRecAttr.class;
                final Method m = nra.getMethod("cloneNative");
                final Field[] fields = this.getClass().getFields();
                for (Field f : fields) {
                    final NdbRecAttr tf = (NdbRecAttr)f.get(this);
                    final NdbRecAttr cf = (NdbRecAttr)m.invoke(tf);
                    f.set(c, cf);
                }
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            return c;
        }

        // renamed, generic method measurably slower than hard-coded version
        // generically clears this object calling NdbRecAttr.delete()
        // on all public fields of this instance
        public void deleteGenerically() {
            try {
                final Class<NdbRecAttr> nra = NdbRecAttr.class;
                final Method m = nra.getMethod("delete", nra);
                final Field[] fields = this.getClass().getFields();
                for (Field f : fields) {
                    final NdbRecAttr tf = (NdbRecAttr)f.get(this);
                    m.invoke(null, tf);
                    f.set(this, null); // nullify field
                }
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    static protected class RecIdHolder extends RecHolder {
        public NdbRecAttr id;

        public RecIdHolder clone() {
            final RecIdHolder h = (RecIdHolder)super.clone();
            h.id = id.cloneNative();
            return h;
        }

        public void delete() { // must be called on cloned instances exactly
            NdbRecAttr.delete(id);
            id = null;
        }
    }

    static protected class RecAttrHolder extends RecIdHolder {
        public NdbRecAttr cint;
        public NdbRecAttr clong;
        public NdbRecAttr cfloat;
        public NdbRecAttr cdouble;

        public RecAttrHolder clone() {
            final RecAttrHolder h = (RecAttrHolder)super.clone();
            h.cint = cint.cloneNative();
            h.clong = clong.cloneNative();
            h.cfloat = cfloat.cloneNative();
            h.cdouble = cdouble.cloneNative();
            return h;
        }

        public void delete() { // must be called on cloned instances exactly
            NdbRecAttr.delete(cint);
            NdbRecAttr.delete(clong);
            NdbRecAttr.delete(cfloat);
            NdbRecAttr.delete(cdouble);
            cint = null;
            clong = null;
            cfloat = null;
            cdouble = null;
            super.delete();
        }
    };

    // ----------------------------------------------------------------------

    protected void setKeyA(NdbOperation op, int id) {
        if (op.equal(model.attr_id, id) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void setKeyB(NdbOperation op, int id) {
        setKeyA(op, id); // currently same as A
    }

    protected void getKeyA(NdbOperation op, ByteBuffer bb) {
        if (op.getValue(model.attr_id, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(bb.position() + model.width_id);
    }

    protected void getKeyB(NdbOperation op, ByteBuffer bb) {
        getKeyA(op, bb); // currently same as A
    }

    protected void getKeyA(NdbOperation op, RecIdHolder ah) {
        if ((ah.id = op.getValue(model.attr_id, null)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void getKeyB(NdbOperation op, RecIdHolder ah) {
        getKeyA(op, ah); // currently same as A
    }

    protected void checkKeyA(int i, ByteBuffer bb) {
        verify(i, bb.getInt());
    }

    protected void checkKeyB(int i, ByteBuffer bb) {
        checkKeyA(i, bb); // currently same as A
    }

    protected void checkAttrB(int i, ByteBuffer bb) {
        checkAttrA(i, bb); // currently same as A
    }

    protected void checkKeyA(int i, RecIdHolder ah) {
        verify(i, ah.id.int32_value());
    }

    protected void checkKeyB(int i, RecIdHolder ah) {
        checkKeyA(i, ah); // currently same as A
    }

    // ----------------------------------------------------------------------

    protected void setAttrA(NdbOperation op, int i) {
        if (op.setValue(model.attr_cint, i) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if (op.setValue(model.attr_clong, (long)i) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if (op.setValue(model.attr_cfloat, (float)i) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if (op.setValue(model.attr_cdouble, (double)i) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void setAttrB(NdbOperation op, int id) {
        setAttrA(op, id); // currently same as A
    }

    protected void getAttrA(NdbOperation op, ByteBuffer bb) {
        int p = bb.position();
        if (op.getValue(model.attr_cint, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += model.width_cint);
        if (op.getValue(model.attr_clong, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += model.width_clong);
        if (op.getValue(model.attr_cfloat, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += model.width_cfloat);
        if (op.getValue(model.attr_cdouble, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(p += model.width_cdouble);
    }

    protected void getAttrB(NdbOperation op, ByteBuffer bb) {
        getAttrA(op, bb); // currently same as A
    }

    protected void getAttrA(NdbOperation op, RecAttrHolder ah) {
        if ((ah.cint = op.getValue(model.attr_cint, null)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if ((ah.clong = op.getValue(model.attr_clong, null)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if ((ah.cfloat = op.getValue(model.attr_cfloat, null)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        if ((ah.cdouble = op.getValue(model.attr_cdouble, null)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void getAttrB(NdbOperation op, RecAttrHolder ah) {
        getAttrA(op, ah); // currently same as A
    }

    protected void checkAttrA(int i, ByteBuffer bb) {
        verify(i, bb.getInt());
        verify(i, bb.getLong());
        verify(i, bb.getFloat());
        verify(i, bb.getDouble());
    }

    protected void checkAttrA(int i, RecAttrHolder ah) {
        verify(i, ah.cint.int32_value());
        verify(i, ah.clong.int64_value());
        verify(i, ah.cfloat.float_value());
        verify(i, ah.cdouble.double_value());
    }

    protected void checkAttrB(int i, RecAttrHolder ah) {
        checkAttrA(i, ah); // currently same as A
    }

    // ----------------------------------------------------------------------

    protected void setVarbinaryB(NdbOperation op, byte[] b) {
        ByteBuffer to = null;
        if (b != null) {
            final int w = model.width_B_cvarbinary_def;
            final int lpw = model.wprefix_B_cvarbinary_def;
            final ByteBuffer bb = ByteBuffer.allocateDirect(w);
            to = asByteBuffer(bb, b, lpw);
        }
        if (op.setValue(model.attr_B_cvarbinary_def, to) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void getVarbinaryB(NdbOperation op, ByteBuffer bb) {
        final int w = model.width_B_cvarbinary_def;
        if (op.getValue(model.attr_B_cvarbinary_def, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(bb.position() + w);
    }

    protected void checkVarbinaryB(byte[] b, ByteBuffer bb) {
        final int w = model.width_B_cvarbinary_def;
        final int lpw = model.wprefix_B_cvarbinary_def;
        final byte[] to = asByteArray(bb, lpw);
        verify(b, to);
        bb.position(bb.position() + w);
    }

    protected void setVarcharB(NdbOperation op, String s) {
        ByteBuffer to = null;
        if (s != null) {
            final int w = model.width_B_cvarchar_def;
            final int lpw = model.wprefix_B_cvarchar_def;
            final ByteBuffer bb = ByteBuffer.allocateDirect(w);
            final CharBuffer cb = CharBuffer.wrap(s);
            to = asByteBuffer(bb, cb, lpw);
        }
        if (op.setValue(model.attr_B_cvarchar_def, to) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void getVarcharB(NdbOperation op, ByteBuffer bb) {
        final int w = model.width_B_cvarchar_def;
        if (op.getValue(model.attr_B_cvarchar_def, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(bb.position() + w);
    }

    protected void checkVarcharB(String s, ByteBuffer bb) {
        final int w = model.width_B_cvarchar_def;
        final int lpw = model.wprefix_B_cvarchar_def;
        final CharBuffer to = asCharBuffer(bb, lpw);
        verify(CharBuffer.wrap(s), to);
        bb.position(bb.position() + w);
    }

    // ----------------------------------------------------------------------

    protected void setAIdB(NdbOperation op, int aid) {
        if (op.setValue(model.attr_B_aid, aid) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void getAIdB(NdbOperation op, ByteBuffer bb) {
        if (op.getValue(model.attr_B_aid, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(bb.position() + model.width_B_aid);
    }

    protected void getAIdB(NdbOperation op, RecIdHolder ah) {
        if ((ah.id = op.getValue(model.attr_B_aid, null)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void setBoundEqAIdB(NdbIndexScanOperation op, int id) {
        final ByteBuffer bnd = ByteBuffer.allocateDirect(4);
        bnd.order(bo); // initial order is BIG_ENDIAN
        bnd.putInt(id);
        bnd.flip();
        if (op.setBound(model.attr_idx_B_aid, BoundType.BoundEQ, bnd) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    // ----------------------------------------------------------------------

    protected int delByScan(TableConst table) {
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
        final boolean allowFetch = true; // fetch new batches, opt usage below
        final boolean forceSend = true; // no send delay for 1-thread app
        while ((stat = op.nextResult(allowFetch, forceSend)) == 0) {
            // delete all tuples within a batch
            do {
                if (op.deleteCurrentTuple() != 0)
                    throw new RuntimeException(toStr(tx.getNdbError()));
                count++;
            } while ((stat = op.nextResult(!allowFetch, forceSend)) == 0);

            if (stat == 1) {
                // no more batches
                break;
            }
            if (stat == 2) {
                // end of current batch, fetch next
                final int execType = ExecType.NoCommit;
                final int abortOption = AbortOption.AbortOnError;
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

        // close the scan, no harm in delaying/accumulating close()
        final boolean releaseOp = true;
        op.close(!forceSend, !releaseOp);

        commitTransaction();
        closeTransaction();
        return count;
    }

    // ----------------------------------------------------------------------

    static protected byte[] asByteArray(ByteBuffer from,
                                        int prefixBytes) {
        final ByteBuffer bb = from.asReadOnlyBuffer();

        // read length prefix
        assert 1 <= prefixBytes && prefixBytes <= 2;
        int l = 0;
        for (int i = 0; i < prefixBytes; i++)
            l |= ((bb.get() & 0xff)<<(i * 8));
        assert 0 <= l && l < 1<<(prefixBytes * 8);

        final byte[] to = new byte[l];
        bb.get(to); // BufferUnderflowException if insufficient bytes in 'bb'
        return to;
    }

    static protected ByteBuffer asByteBuffer(ByteBuffer to,
                                             byte[] from,
                                             int prefixBytes) {
        final int p = to.position();

        // write length prefix
        assert 1 <= prefixBytes && prefixBytes <= 2;
        final int l = from.length;
        assert 0 <= l && l < 1<<(prefixBytes * 8);
        for (int i = 0; i < prefixBytes; i++)
            to.put((byte)(l>>>(i * 8) & 0xff));

        to.put(from); // BufferOverflowException if insufficient space in 'bb'
        final ByteBuffer bb = to.duplicate();
        bb.position(p);
        return bb;
    }

    static protected CharBuffer asCharBuffer(ByteBuffer from,
                                             int prefixBytes) {
        final ByteBuffer bb = from.asReadOnlyBuffer();

        // read length prefix from initial position
        assert 1 <= prefixBytes && prefixBytes <= 2;
        int l = 0;
        for (int i = 0; i < prefixBytes; i++)
            l |= ((bb.get() & 0xff)<<(i * 8));
        assert 0 <= l && l < 1<<(prefixBytes * 8);

        // decode
        final int q = bb.position() + l;
        assert q <= bb.limit();
        bb.limit(q);
        final CharBuffer to;
        try {
            to = csDecoder.decode(bb);
            assert !bb.hasRemaining();
        } catch (CharacterCodingException ex) {
            throw new RuntimeException(ex);
        }
        assert to.position() == 0 && to.limit() == to.capacity();

        return to;
    }

    static protected ByteBuffer asByteBuffer(ByteBuffer to,
                                             CharBuffer from,
                                             int prefixBytes) {
        final CharBuffer cb = from.asReadOnlyBuffer();

        // advance length prefix
        assert 1 <= prefixBytes && prefixBytes <= 2;
        final int p = to.position();
        to.position(p + prefixBytes);

        // encode
        try {
            final boolean endOfInput = true;
            final CoderResult cr = csEncoder.encode(cb, to, endOfInput);
            if (!cr.isUnderflow())
                cr.throwException();
            assert !cb.hasRemaining();
        } catch (CharacterCodingException ex) {
            throw new RuntimeException(ex);
        }

        // write length prefix at initial position
        final int q = to.position();
        final int l = (q - p) - prefixBytes;
        assert 0 <= l && l < 1<<(prefixBytes * 8);
        for (int i = 0; i < prefixBytes; i++)
            to.put(p + i, (byte)(l>>>(i * 8) & 0xff));

        final ByteBuffer bb = to.duplicate();
        bb.position(p);
        return bb;
    }

    // ----------------------------------------------------------------------

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
        final int execType = ExecType.NoCommit;
        final int abortOption = AbortOption.AbortOnError;
        final int force = 0;
        if (tx.execute(execType, abortOption, force) != 0
            || tx.getNdbError().status() != NdbError.Status.Success)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void commitTransaction() {
        // commit the current transaction
        final int execType = ExecType.Commit;
        final int abortOption = AbortOption.AbortOnError;
        final int force = 0;
        if (tx.execute(execType, abortOption, force) != 0
            || tx.getNdbError().status() != NdbError.Status.Success)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void rollbackTransaction() {
        // abort the current transaction
        final int execType = ExecType.Rollback;
        final int abortOption = AbortOption.DefaultAbortOption;
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
    // NDB JTie datastore operations
    // ----------------------------------------------------------------------

    public void initConnection() {
        assert mgmd != null;
        assert ndb == null;
        assert model == null;
        out.println();
        out.println("initializing NDB resources ...");

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
        // note each scan or index scan operation uses one extra transaction
        if (ndb.init(nMaxConcTx) != 0) {
            String msg = "Error caught: " + ndb.getNdbError().message();
            throw new RuntimeException(msg);
        }
        out.println("      [ok: " + catalog + "." + schema + "]");

        out.print("using lock mode for reads ...");
        out.flush();
        final String lm;
        switch (driver.lockMode) {
        case none:
            ndbOpLockMode = NdbOperationConst.LockMode.LM_CommittedRead;
            lm = "LM_CommittedRead";
            break;
        case shared:
            ndbOpLockMode = NdbOperationConst.LockMode.LM_Read;
            lm = "LM_Read";
            break;
        case exclusive:
            ndbOpLockMode = NdbOperationConst.LockMode.LM_Exclusive;
            lm = "LM_Exclusive";
            break;
        default:
            ndbOpLockMode = NdbOperationConst.LockMode.LM_CommittedRead;
            lm = "LM_CommittedRead";
            assert false;
        }
        out.println("   [ok: " + lm + "]");

        out.print("caching metadata ...");
        out.flush();
        model = new Model(ndb);
        out.println("            [ok]");

        initOperations();
    }

    public void closeConnection() {
        assert model != null;
        assert ndb != null;
        out.println();
        out.println("releasing NDB resources ...");

        closeOperations();

        out.print("clearing metadata cache ...");
        out.flush();
        model = null;
        out.println("     [ok]");

        out.print("closing database connection ...");
        out.flush();
        Ndb.delete(ndb);
        ndb = null;
        out.println(" [ok]");
    }

    public void clearData() {
        out.print("deleting all rows ...");
        out.flush();
        final int delB = delByScan(model.table_B);
        out.print("           [B: " + delB);
        out.flush();
        final int delA = delByScan(model.table_A);
        out.print(", A: " + delA);
        out.flush();
        out.println("]");
    }
}
