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

import java.util.List;
import java.util.ArrayList;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
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
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbRecAttr;
import com.mysql.ndbjtie.ndbapi.NdbOperationConst.LockMode;
/*
//import com.mysql.ndbjtie.ndbapi.ExecType;
import com.mysql.ndbjtie.ndbapi.NdbTransaction.ExecType;
import com.mysql.ndbjtie.ndbapi.NdbOperation;
//import com.mysql.ndbjtie.ndbapi.AbortOption;
import com.mysql.ndbjtie.ndbapi.NdbOperation.AbortOption;
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation;
import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation.BoundType;
*/

/**
 * The NDB JTie benchmark implementation.
 */
public class NdbjtieLoad extends CrundDriver {

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

    // ----------------------------------------------------------------------
    // NDB JTie intializers/finalizers
    // ----------------------------------------------------------------------

   protected void initProperties() {
        super.initProperties();

        out.print("setting ndb properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        // the hostname and port number of NDB mgmd
        mgmdConnect = props.getProperty("ndb.mgmdConnect", "localhost");
        assert mgmdConnect != null;

        // the database
        catalog = props.getProperty("ndb.catalog", "crunddb");
        assert catalog != null;

        // the schema
        schema = props.getProperty("ndb.schema", "def");
        assert schema != null;

        if (msg.length() == 0) {
            out.println("      [ok]");
        } else {
            out.println();
            out.print(msg.toString());
        }

        descr = "->ndbjtie(" + mgmdConnect + ")";
    }

    protected void printProperties() {
        super.printProperties();

        out.println();
        out.println("ndb settings ...");
        out.println("ndb.mgmdConnect:                \"" + mgmdConnect + "\"");
        out.println("ndb.catalog:                    \"" + catalog + "\"");
        out.println("ndb.schema:                     \"" + schema + "\"");
    }

    protected void initLoad() throws Exception {
        // XXX support generic load class
        //super.init();
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

    protected void closeLoad() throws Exception {
        assert mgmd != null;

        out.println();
        out.print("closing cluster connection ...");
        out.flush();
        Ndb_cluster_connection.delete(mgmd);
        mgmd = null;
        out.println("  [ok]");

        // XXX support generic load class
        //super.close();
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
        public final int attr_id;
        public final int attr_cint;
        public final int attr_clong;
        public final int attr_cfloat;
        public final int attr_cdouble;
        public final int attr_B_aid;
        public final int attr_B_cvarbinary_def;
        public final int attr_B_cvarchar_def;
        public final int width_id;
        public final int width_cint;
        public final int width_clong;
        public final int width_cfloat;
        public final int width_cdouble;
        public final int width_B_aid;
        public final int width_B_cvarbinary_def;
        public final int width_B_cvarchar_def;
        public final int width_A_row; // sum of width of columns in A
        public final int width_B_row; // sum of width of columns in B
        public final int width_AB_row; // sum of width of common columns in A,B
        public final int wprefix_B_cvarbinary_def; // width of length prefix
        public final int wprefix_B_cvarchar_def; // width of length prefix

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
 
            // get attribute ids for table B
            attr_B_aid = column_B_aid.getColumnNo();
            attr_B_cvarbinary_def = column_B_cvarbinary_def.getColumnNo();
            attr_B_cvarchar_def = column_B_cvarchar_def.getColumnNo();

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

            // get the width of columns in table B
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

    // assumes PKs: 0..nOps, relationships: identity 1:1
    protected abstract class NdbjtieOp extends Op {
        protected final TableConst table;
        protected XMode xMode;

        public NdbjtieOp(String name, XMode m, TableConst table) {
            super(name + (m == null ? "" : toStr(m)));
            this.table = table;
            this.xMode = m;
        }

        public void init() {}

        public void close() {}
    };

    protected abstract class WriteOp extends NdbjtieOp {
        protected NdbOperation op;

        public WriteOp(String name, XMode m, TableConst table) {
            super(name, m, table);
        }

        public void run(int nOps) {
            switch (xMode) {
            case INDY :
                for (int id = 0; id < nOps; id++) {
                    beginTransaction();
                    write(id);
                    commitTransaction();
                    closeTransaction();
                }
                break;
            case EACH :
            case BULK :
                // Approach: control when persistent context is flushed,
                // i.e., at commit for 1 database roundtrip only.
                beginTransaction();
                for (int id = 0; id < nOps; id++) {
                    write(id);
                    if (xMode == XMode.EACH)
                        executeOperations();
                }
                commitTransaction();
                closeTransaction();
                break;
            }
        }

        protected void write(int id) {
            op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
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
            if (op.updateTuple() != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
        }
    }

    protected abstract class InsertOp extends WriteOp {
        public InsertOp(String name, XMode m, TableConst table) {
            super(name, m, table);
        }

        protected final void setOp() {
            if (op.insertTuple() != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
        }
    }

    protected abstract class DeleteOp extends WriteOp {
        public DeleteOp(String name, XMode m, TableConst table) {
            super(name, m, table);
        }

        protected final void setOp() {
            if (op.deleteTuple() != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
        }
    }

    protected abstract class ReadOp extends NdbjtieOp {
        protected NdbOperation op;

        public ReadOp(String name, XMode m, TableConst table) {
            super(name, m, table);
        }

        public void run(int nOps) {
            switch (xMode) {
            case INDY :
                for (int id = 0; id < nOps; id++) {
                    beginTransaction();
                    alloc(1);
                    rewind();
                    read(id);
                    commitTransaction();
                    rewind();
                    check(id);
                    closeTransaction();
                }
                break;
            case EACH :
                beginTransaction();
                alloc(1);
                for (int id = 0; id < nOps; id++) {
                    rewind();
                    read(id);
                    executeOperations();
                    rewind();
                    check(id);
                }
                commitTransaction();
                closeTransaction();
                break;
            case BULK :
                beginTransaction();
                alloc(nOps);
                rewind();
                for (int id = 0; id < nOps; id++)
                    read(id);
                executeOperations();
                rewind();
                for (int id = 0; id < nOps; id++)
                    check(id);
                commitTransaction();
                closeTransaction();
                break;
            }
        }

        protected final void read(int id) {
            op = tx.getNdbOperation(table);
            if (op == null)
                throw new RuntimeException(toStr(tx.getNdbError()));
            setOp();
            getValues(id);
        }

        protected final void setOp() {
            if (op.readTuple(ndbOpLockMode) != 0)
                throw new RuntimeException(toStr(tx.getNdbError()));
        }

        protected abstract void alloc(int n);

        protected abstract void rewind();

        protected abstract void getValues(int id);

        protected abstract void check(int id);
    }

    protected abstract class BBReadOp extends ReadOp {
        protected final int rowWidth;
        protected ByteBuffer bb;

        public BBReadOp(String name, XMode m, TableConst table, int rowWidth) {
            super(name, m, table);
            this.rowWidth = rowWidth;
        }

        protected final void alloc(int n) {
            bb = ByteBuffer.allocateDirect(rowWidth * n);
            bb.order(bo); // initial order is BIG_ENDIAN
        }

        protected final void rewind() {
            bb.rewind(); // prepare buffer for reading
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

        protected final void alloc(int n) {
            try {
                ah = new ArrayList<H>(n);
                for (int i = 0; i < n; i++)
                    ah.add(cls.newInstance());
            } catch (InstantiationException ex) {
                throw new RuntimeException(ex);
            } catch (IllegalAccessException ex) {
                throw new RuntimeException(ex);
            }
        }

        protected final void rewind() {
            pos = 0;
        }
    }

    // ----------------------------------------------------------------------

    protected void initOperations() {
        out.print("initializing operations ...");
        out.flush();

        //out.println("default charset: "
        //    + java.nio.charset.Charset.defaultCharset().displayName());

        for (XMode m : xMode) {
            // inner classes can only refer to a constant
            final XMode xMode = m;
            final boolean forceSend = true;

            ops.add(
                new InsertOp("A_insAttr_", xMode, model.table_A) {
                    public void setValues(int id) {
                        setKeyA(op, id); // needs to be set first
                        setAttrA(op, -id);
                    }
                });

            ops.add(
                new InsertOp("B_insAttr_", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setAttrB(op, -id);
                    }
                });

            ops.add(
                new UpdateOp("A_setAttr_", xMode, model.table_A) {
                    public void setValues(int id) {
                        setKeyA(op, id); // needs to be set first
                        setAttrA(op, id);
                    }
                });

            ops.add(
                new UpdateOp("B_setAttr_", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setAttrB(op, id);
                    }
                });

            ops.add(
                new BBReadOp("A_getAttr_bb_", xMode,
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
                new RAReadOp<RecAttrHolder>("A_getAttr_ra_", xMode,
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
                new BBReadOp("B_getAttr_bb_", xMode,
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
                new RAReadOp<RecAttrHolder>("B_getAttr_ra_", xMode,
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
                if (l > maxVarbinaryBytes)
                    break;

                if (l > (model.width_B_cvarbinary_def
                         - model.wprefix_B_cvarbinary_def)) {
                    String msg = "property maxVarbinaryBytes > |B.cvarbinary|";
                    throw new RuntimeException(msg);
                }

            ops.add(
                new UpdateOp("B_setVarbin_" + l + "_" , xMode,
                             model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setVarbinaryB(op, b);
                    }
                });

            ops.add(
                new BBReadOp("B_getVarbin_" + l + "_" , xMode,
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
                new UpdateOp("B_clearVarbin_" + l + "_" , xMode,
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
                if (l > maxVarcharChars)
                    break;

                if (l > (model.width_B_cvarchar_def
                         - model.wprefix_B_cvarchar_def)) {
                    String msg = "property maxVarcharChars > |B.cvarchar|";
                    throw new RuntimeException(msg);
                }

            ops.add(
                new UpdateOp("B_setVarchar_" + l + "_" , xMode,
                             model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setVarcharB(op, s);
                    }
                });

            ops.add(
                new BBReadOp("B_getVarchar_" + l + "_" , xMode,
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
                new UpdateOp("B_clearVarchar_" + l + "_" , xMode,
                             model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        setVarcharB(op, null);
                    }
                });
            }

            ops.add(
                new UpdateOp("B_setA_", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        final int aid = id;
                        setAIdB(op, aid);
                    }
                });

            ops.add(
                new UpdateOp("B_clearA_", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id); // needs to be set first
                        final int aid = -1;
                        setAIdB(op, aid);
                    }
                });

            ops.add(
                new Op("B_getA_" + toStr(xMode)) {
                    public void run(int nOps) {
                        navBToA(nOps, xMode);
                    }
                });

            ops.add(
                new Op("A_getBs") {
                    public void run(int nOps) {
                        navAToB(nOps, !forceSend);
                    }
                });

            ops.add(
                new Op("A_getBs_forceSend") {
                    public void run(int nOps) {
                        navAToB(nOps, forceSend);
                    }
                });

            ops.add(
                new DeleteOp("B_del_", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id);
                    }
                });

            ops.add(
                new DeleteOp("A_del_", xMode, model.table_A) {
                    public void setValues(int id) {
                        setKeyA(op, id);
                    }
                });

            ops.add(
                new InsertOp("A_ins_", xMode, model.table_A) {
                    public void setValues(int id) {
                        setKeyA(op, id);
                    }
                });

            ops.add(
                new InsertOp("B_ins_", xMode, model.table_B) {
                    public void setValues(int id) {
                        setKeyB(op, id);
                    }
                });

            ops.add(
                new Op("B_delAll") {
                    public void run(int nOps) {
                        final int n = delByScan(model.table_B);
                        verify(nOps, n);
                    }
                });

            ops.add(
                new Op("A_delAll") {
                    public void run(int nOps) {
                        final int n = delByScan(model.table_A);
                        verify(nOps, n);
                    }
                });
        }

        out.println("     [Op: " + ops.size() + "]");
    }

    protected void closeOperations() {
        out.println();
        out.print("closing operations ...");
        out.flush();
        ops.clear();
        out.println("          [ok]");
    }

    // ----------------------------------------------------------------------

    static protected class RecAttrHolder {
        public NdbRecAttr id;
        public NdbRecAttr cint;
        public NdbRecAttr clong;
        public NdbRecAttr cfloat;
        public NdbRecAttr cdouble;
    };

    protected void setKeyA(NdbOperation op, int id) {
        if (op.equal(model.attr_id, id) != 0)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void setKeyB(NdbOperation op, int id) {
        setKeyA(op, id); // currently same as A
    }

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

    protected void getKeyA(NdbOperation op, ByteBuffer bb) {
        if (op.getValue(model.attr_id, bb) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
        bb.position(bb.position() + model.width_id);
    }
    
    protected void getKeyB(NdbOperation op, ByteBuffer bb) {
        getKeyA(op, bb); // currently same as A
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

    protected void getKeyA(NdbOperation op, RecAttrHolder ah) {
        if ((ah.id = op.getValue(model.attr_id, null)) == null)
            throw new RuntimeException(toStr(tx.getNdbError()));
    }

    protected void getKeyB(NdbOperation op, RecAttrHolder ah) {
        getKeyA(op, ah); // currently same as A
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

    protected void checkKeyA(int i, ByteBuffer bb) {
        verify(i, bb.getInt());
    }

    protected void checkKeyB(int i, ByteBuffer bb) {
        checkKeyA(i, bb); // currently same as A
    }

    protected void checkAttrA(int i, ByteBuffer bb) {
        verify(i, bb.getInt());
        verify(i, bb.getLong());
        verify(i, bb.getFloat());
        verify(i, bb.getDouble());
    }

    protected void checkAttrB(int i, ByteBuffer bb) {
        checkAttrA(i, bb); // currently same as A
    }

    protected void checkKeyA(int i, RecAttrHolder ah) {
        verify(i, ah.id.int32_value());
    }

    protected void checkKeyB(int i, RecAttrHolder ah) {
        checkKeyA(i, ah); // currently same as A
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

    protected void navBToA(int n, XMode xMode) {
// XXX not implemented yet
    }

    protected void navAToB(int n, boolean forceSend) {
// XXX not implemented yet
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
        final boolean allowFetch = true; // request new batches when exhausted
        final boolean forceSend = false; // send may be delayed
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

    // ----------------------------------------------------------------------

    protected byte[] asByteArray(ByteBuffer from,
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

    protected ByteBuffer asByteBuffer(ByteBuffer to,
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

    protected CharBuffer asCharBuffer(ByteBuffer from,
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

    protected ByteBuffer asByteBuffer(ByteBuffer to,
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
    // NDB JTie datastore operations
    // ----------------------------------------------------------------------

    public void initConnection() {
        assert mgmd != null;
        assert ndb == null;
        assert model == null;

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

        out.print("caching metadata ...");
        out.flush();
        model = new Model(ndb);
        out.println("            [ok]");

        out.print("using lock mode for reads ...");
        out.flush();
        final String lm;
        switch (lockMode) {
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
        assert model != null;
        assert ndb != null;

        out.println();
        out.println("releasing ndbjtie resources ...");

        out.print("clearing metadata cache...");
        out.flush();
        model = null;
        out.println("      [ok]");

        out.print("closing database connection ...");
        out.flush();
        Ndb.delete(ndb);
        ndb = null;
        out.println(" [ok]");
    }

    // so far, there's no NDB support for caching data beyond Tx scope
    protected void clearPersistenceContext() throws Exception {}

    protected void clearData() {
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

    // ----------------------------------------------------------------------

    static public void main(String[] args) {
        System.out.println("NdbjtieLoad.main()");
        parseArguments(args);
        new NdbjtieLoad().run();
        System.out.println();
        System.out.println("NdbjtieLoad.main(): done.");
    }
}
