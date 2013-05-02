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

import java.util.Iterator;
import java.util.Arrays;

import java.sql.SQLException;
import java.sql.DriverManager;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.Statement;
import java.sql.Array;
import java.sql.Types;

/**
 * The JDBC benchmark implementation.
 */
public class JdbcLoad extends CrundDriver {

    // JDBC settings
    protected String driver;
    protected String url;
    protected String user;
    protected String password;

    // JDBC resources
    protected Connection conn;
    protected PreparedStatement delAllA;
    protected PreparedStatement delAllB;

    // ----------------------------------------------------------------------
    // JDBC intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        super.initProperties();

        out.print("setting jdbc properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        // load the JDBC driver class
        driver = props.getProperty("jdbc.driver");
        if (driver == null) {
            throw new RuntimeException("Missing property: jdbc.driver");
        }
        try {
            Class.forName(driver);
        } catch (ClassNotFoundException e) {
            out.println("Cannot load JDBC driver '" + driver
                        + "' from classpath '"
                        + System.getProperty("java.class.path") + "'");
            throw new RuntimeException(e);
        }

        url = props.getProperty("jdbc.url");
        if (url == null) {
            throw new RuntimeException("Missing property: jdbc.url");
        }

        user = props.getProperty("jdbc.user");
        password = props.getProperty("jdbc.password");

        if (msg.length() == 0) {
            out.println("     [ok]");
        } else {
            out.println();
            out.print(msg.toString());
        }

        // have url initialized first
        descr = "->" + url;
    }

    protected void printProperties() {
        super.printProperties();

        out.println();
        out.println("jdbc settings ...");
        out.println("jdbc.driver:                    " + driver);
        out.println("jdbc.url:                       " + url);
        out.println("jdbc.user:                      \"" + user + "\"");
        out.println("jdbc.password:                  \"" + password + "\"");
    }

    protected void initLoad() throws Exception {
        // XXX support generic load class
        //super.init();
    }

    protected void closeLoad() throws Exception {
        // XXX support generic load class
        //super.close();
    }

    // ----------------------------------------------------------------------
    // JDBC operations
    // ----------------------------------------------------------------------

    // model assumptions: relationships: identity 1:1
    protected abstract class JdbcOp extends Op {
        protected String sql;
        protected PreparedStatement ps;
        protected XMode xMode;

        public JdbcOp(String name, XMode m, String sql) {
            super(name + (m == null ? "" : toStr(m)));
            this.sql = sql;
            this.xMode = m;
        }

        public void init() throws SQLException {
            if (ps == null)
                ps = conn.prepareStatement(sql);
        }

        public void close() throws SQLException {
            if (ps != null)
                ps.close();
            ps = null;
        }
    }

    protected abstract class WriteOp extends JdbcOp {
        public WriteOp(String name, XMode m, String sql) {
            super(name, m, sql);
        }

        public void run(int nOps) throws SQLException {
            conn.setAutoCommit(xMode == XMode.INDY);
            switch (xMode) {
            case INDY :
            case EACH :
                for (int i = 0; i < nOps; i++) {
                    setParams(i);
                    final int cnt = ps.executeUpdate();
                    verify(1, cnt);
                }
                if (xMode != XMode.INDY)
                    conn.commit();
                break;
            case BULK :
                for(int i = 0; i < nOps; i++) {
                    setParams(i);
                    ps.addBatch();
                }
                final int[] cnt = ps.executeBatch();
                for (int i = 0; i < nOps; i++)
                    verify(1, cnt[i]);
                conn.commit();
                break;
            }
        }

        protected void setParams(int i) throws SQLException {}
    };

    protected abstract class ReadOp extends JdbcOp {
        protected ResultSet rs;

        public ReadOp(String name, XMode m, String sql) {
            super(name, m, sql);
        }

        public void run(int nOps) throws SQLException {
            conn.setAutoCommit(xMode == XMode.INDY);
            switch (xMode) {
            case INDY :
            case EACH :
                for (int i = 0; i < nOps; i++) {
                    setParams(i);
                    rs = ps.executeQuery();
                    rs.next();
                    getValues(i);
                    verify(!rs.next());
                    rs.close();
                    assert !ps.getMoreResults();
                }
                if (xMode != XMode.INDY)
                    conn.commit();
                break;
            case BULK :
                // use dynamic SQL for generic bulk queries
                // allow for multi/single result sets with single/multi rows
                final String q = getQueries(nOps);
                final Statement s = conn.createStatement();
                boolean hasRS = s.execute(q);
                int i = 0;
                while (hasRS) {
                    rs = s.getResultSet();
                    while (rs.next())
                        getValues(i++);
                    hasRS = s.getMoreResults();
                }
                verify(nOps, i);

                if (xMode != XMode.INDY)
                    conn.commit();
                break;
            }
        }

        /**
         * Generic bulk query support: dynamic SQL query with substituted
         * Ids for params.
         */
        protected String getQueries(int nOps) throws SQLException {
            // default: multiply query substituting Id for all params
            return multipleQueries(nOps);
        }
        
        /**
         * Bulk query support, mechanism #1: multiplied SQL queries with
         * substituted Ids for params.
         *
         * The mysql jdbc driver requires property allowMultiQueries=true
         * passed to DriverManager.getConnection() or in URL
         * jdbc:mysql://localhost/crunddb?allowMultiQueries=true
         */
        protected String multipleQueries(int nOps) throws SQLException {
            final StringBuilder sb = new StringBuilder();
            for (int i = 0; i < nOps; i++)
                sb.append(sql.replace("?", String.valueOf(i))).append(";");
            return sb.toString();
        }

        /**
         * Bulk query support, mechanism #2: where-in clause with
         * comma-separated List of Ids.
         */
        protected String whereInIdList(int nOps) throws SQLException {
            final StringBuilder sb = new StringBuilder();
            for (int i = 0; i < nOps; i++)
                sb.append(String.valueOf(i) + ",");
            sb.append("null"); // need at least one value
            return sql.replace("?", sb);
        }

        /**
         * Bulk query support, mechanism #3: where-in clause with Ids
         * passed as SQL Array.
         *
         * JDBC 4 feature.  Not supported by mysql driver 5.1 (or Java DB).
         */
        protected Array idArray(int nOps) throws SQLException {
            Object[] a = new Object[nOps];
            for (int i = 0; i < nOps; i++)
                a[i] = new Integer(i);
            return conn.createArrayOf("integer", a);
        }

        protected void setParams(int i) throws SQLException {}
        protected void getValues(int i) throws SQLException {}
    }

    protected void initOperations() throws SQLException {
        out.print("initializing statements ...");
        out.flush();

        for (XMode m : xMode) {
            // inner classes can only refer to a constant
            final XMode xMode = m;

            ops.add(
                new WriteOp("A_insAttr_", xMode,
                            "INSERT INTO A (id, cint, clong, cfloat, cdouble) VALUES (?, ?, ?, ?, ?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                        setAttrA(ps, 2, -i);
                    }
                });

            ops.add(
                new WriteOp("B_insAttr_", xMode,
                            "INSERT INTO B (id, cint, clong, cfloat, cdouble) VALUES (?, ?, ?, ?, ?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                        setAttrB(ps, 2, -i);
                    }
                });

            ops.add(
                new WriteOp("A_setAttr_", xMode,
                            "UPDATE A a SET a.cint = ?, a.clong = ?, a.cfloat = ?, a.cdouble = ? WHERE (a.id = ?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(setAttrA(ps, 1, i), i);
                    }
                });

            ops.add(
                new WriteOp("B_setAttr_", xMode,
                            "UPDATE B b SET b.cint = ?, b.clong = ?, b.cfloat = ?, b.cdouble = ? WHERE (b.id = ?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(setAttrB(ps, 1, i), i);
                    }
                });

            ops.add(
                new ReadOp("A_getAttr_", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM A WHERE (id = ?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }

                    protected void getValues(int i) throws SQLException {
                        verify(i, rs.getInt(1));
                        verifyAttrA(i, 2, rs);
                    }
                });

            ops.add(
                new ReadOp("B_getAttr_", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM B WHERE (id = ?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }

                    protected void getValues(int i) throws SQLException {
                        verify(i, rs.getInt(1));
                        verifyAttrB(i, 2, rs);
                    }
                });

            ops.add(
                new ReadOp("A_getAttr_wherein_", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM A WHERE id in (?) ORDER BY id") {
                    protected String getQueries(int nOps) throws SQLException {
                        return whereInIdList(nOps);
                    }

                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }

                    protected void getValues(int i) throws SQLException {
                        assert rs != null;
                        verify(i, rs.getInt(1));
                        verifyAttrA(i, 2, rs);
                    }
                });

            ops.add(
                new ReadOp("B_getAttr_wherein_", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM A WHERE id in (?) ORDER BY id") {
                    protected String getQueries(int nOps) throws SQLException {
                        return whereInIdList(nOps);
                    }

                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }

                    protected void getValues(int i) throws SQLException {
                        assert rs != null;
                        verify(i, rs.getInt(1));
                        verifyAttrB(i, 2, rs);
                    }
                });

            for (int i = 0; i < bytes.length; i++) {
                // inner classes can only refer to a constant
                final byte[] b = bytes[i];
                final int l = b.length;
                if (l > maxVarbinaryBytes)
                    break;

                ops.add(
                    new WriteOp("B_setVarbin_" + l + "_", xMode,
                                "UPDATE B b SET b.cvarbinary_def = ? WHERE (b.id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setBytes(1, b);
                            ps.setInt(2, i);
                        }
                    });

                ops.add(
                    new ReadOp("B_getVarbin_" + l + "_", xMode,
                               "SELECT cvarbinary_def FROM B WHERE (id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setInt(1, i);
                        }

                        protected void getValues(int i) throws SQLException {
                            verify(b, rs.getBytes(1));
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarbin_" + l + "_", xMode,
                                "UPDATE B b SET b.cvarbinary_def = NULL WHERE (b.id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setInt(1, i);
                        }
                    });
            }

            for (int i = 0; i < strings.length; i++) {
                // inner classes can only refer to a constant
                final String s = strings[i];
                final int l = s.length();
                if (l > maxVarcharChars)
                    break;

                ops.add(
                    new WriteOp("B_setVarchar_" + l + "_", xMode,
                                "UPDATE B b SET b.cvarchar_def = ? WHERE (b.id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setString(1, s);
                            ps.setInt(2, i);
                        }
                    });

                ops.add(
                    new ReadOp("B_getVarchar_" + l + "_", xMode,
                               "SELECT cvarchar_def FROM B WHERE (id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setInt(1, i);
                        }

                        protected void getValues(int i) throws SQLException {
                            verify(s, rs.getString(1));
                        }
                    });

                ops.add(
                    new WriteOp("clearVarchar_" + l + "_", xMode,
                                "UPDATE B b SET b.cvarchar_def = NULL WHERE (b.id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setInt(1, i);
                        }
                    });
            }

            for (int i = 3; i < bytes.length; i++) {
                // inner classes can only refer to a constant
                final byte[] b = bytes[i];
                final int l = b.length;
                if (l > maxBlobBytes)
                    break;

                ops.add(
                    new WriteOp("B_setBlob_" + l + "_", xMode,
                                "UPDATE B b SET b.cblob_def = ? WHERE (b.id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setBytes(1, b);
                            ps.setInt(2, i);
                        }
                    });

                ops.add(
                    new ReadOp("B_getBlob_" + l + "_", xMode,
                               "SELECT cblob_def FROM B WHERE (id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setInt(1, i);
                        }

                        protected void getValues(int i) throws SQLException {
                            verify(b, rs.getBytes(1));
                        }
                    });

                ops.add(
                    new WriteOp("B_clearBlob_" + l + "_", xMode,
                                "UPDATE B b SET b.cblob_def = NULL WHERE (b.id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setInt(1, i);
                        }
                    });
            }

            for (int i = 3; i < strings.length; i++) {
                // inner classes can only refer to a constant
                final String s = strings[i];
                final int l = s.length();
                if (l > maxTextChars)
                    break;

                ops.add(
                    new WriteOp("B_setText_" + l + "_", xMode,
                                "UPDATE B b SET b.ctext_def = ? WHERE (b.id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setString(1, s);
                            ps.setInt(2, i);
                        }
                    });

                ops.add(
                    new ReadOp("B_getText_" + l + "_", xMode,
                               "SELECT ctext_def FROM B WHERE (id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setInt(1, i);
                        }

                        protected void getValues(int i) throws SQLException {
                            verify(s, rs.getString(1));
                        }
                    });

                ops.add(
                    new WriteOp("B_clearText_" + l + "_", xMode,
                                "UPDATE B b SET b.ctext_def = NULL WHERE (b.id = ?)") {
                        protected void setParams(int i) throws SQLException {
                            ps.setInt(1, i);
                        }
                    });
            }

            ops.add(
                new WriteOp("B_setA_", xMode,
                            "UPDATE B b SET b.a_id = ? WHERE (b.id = ?)") {
                    protected void setParams(int i) throws SQLException {
                        final int aId = i;
                        ps.setInt(1, aId);
                        ps.setInt(2, i);
                    }
                });

            ops.add(
                new ReadOp("B_getA_joinproj_", xMode,
                           "SELECT a.id, a.cint, a.clong, a.cfloat, a.cdouble FROM A a, b b WHERE (a.id = b.a_id AND b.id = ?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }

                    protected void getValues(int i) throws SQLException {
                        verify(i, rs.getInt(1));
                        verifyAttrA(i, 2, rs);
                    }
                });

            ops.add(
                new ReadOp("B_getA_subsel_", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM A WHERE id = (SELECT b.a_id FROM B b WHERE b.id = ?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }

                    protected void getValues(int i) throws SQLException {
                        verify(i, rs.getInt(1));
                        verifyAttrA(i, 2, rs);
                    }
                });

            ops.add(
                new ReadOp("A_getBs_", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM B WHERE (a_id = ?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }

                    protected void getValues(int i) throws SQLException {
                        verify(i, rs.getInt(1));
                        verifyAttrB(i, 2, rs);
                    }
                });

            ops.add(
                new WriteOp("B_clearA_", xMode,
                            "UPDATE B b SET b.a_id = NULL WHERE (b.id = ?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }
                });

            ops.add(
                // MySQL rejects this syntax: "DELETE FROM B b WHERE b.id = ?"
                new WriteOp("B_del_", xMode,
                            "DELETE FROM B WHERE id = ?") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }
                });

            ops.add(
                // MySQL rejects this syntax: "DELETE FROM A a WHERE a.id = ?"
                new WriteOp("A_del_", xMode,
                            "DELETE FROM A WHERE id = ?") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }
                });

            ops.add(
                new WriteOp("A_ins_", xMode,
                            "INSERT INTO A (id) VALUES (?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }
                });

            ops.add(
                new WriteOp("B_ins_", xMode,
                            "INSERT INTO B (id) VALUES (?)") {
                    protected void setParams(int i) throws SQLException {
                        ps.setInt(1, i);
                    }
                });

            ops.add(
                new WriteOp("B_delAll", null,
                            "DELETE FROM B") {
                    public void run(int nOps) throws SQLException {
                        conn.setAutoCommit(true);
                        final int cnt = ps.executeUpdate();
                        verify(nOps, cnt);
                    }
                });

            ops.add(
                new WriteOp("A_delAll", null,
                            "DELETE FROM A") {
                    public void run(int nOps) throws SQLException {
                        conn.setAutoCommit(true);
                        final int cnt = ps.executeUpdate();
                        verify(nOps, cnt);
                    }
                });
        }

        // prepare all statements
        for (Op o : ops)
            ((JdbcOp)o).init();
        out.println("     [ReadOp: " + ops.size() + "]");
    }

    protected void closeOperations() throws SQLException {
        out.print("closing statements ...");
        out.flush();
        for (Op o : ops)
            ((JdbcOp)o).close();
        ops.clear();
        out.println("          [ok]");
    }

    // ----------------------------------------------------------------------

    protected int setAttrA(PreparedStatement ps, int p, int i)
        throws SQLException {
        ps.setInt(p++, i);
        ps.setLong(p++, (long)i);
        ps.setFloat(p++, (float)i);
        ps.setDouble(p++, (double)i);
        return p;
    }

    protected int setAttrB(PreparedStatement ps, int p, int i)
        throws SQLException {
        return setAttrA(ps, p, i); // currently same as A
    }

    protected int verifyAttrA(int i, int p, ResultSet rs)
        throws SQLException {
        verify(i, rs.getInt(p++));
        verify(i, rs.getLong(p++));
        verify(i, rs.getFloat(p++));
        verify(i, rs.getDouble(p++));
        return p;
    }

    protected int verifyAttrB(int i, int p, ResultSet rs)
        throws SQLException {
        return verifyAttrA(i, p, rs); // currently same as A
    }

    // ----------------------------------------------------------------------
    // JDBC datastore operations
    // ----------------------------------------------------------------------

    protected void initConnection() throws SQLException {
        assert (conn == null);

        out.println();
        out.println("initializing jdbc resources ...");

        out.println();
        out.print("creating jdbc connection ...");
        out.flush();
        conn = DriverManager.getConnection(url, user, password);
        delAllA = conn.prepareStatement("DELETE FROM A");
        delAllB = conn.prepareStatement("DELETE FROM B");
        out.println("    [ok: " + url + "]");

        out.print("setting isolation level ...");
        out.flush();
        // ndb storage engine only supports READ_COMMITTED
        final int il = Connection.TRANSACTION_READ_COMMITTED;
        conn.setTransactionIsolation(il);
        out.print("     [ok: ");
        switch (conn.getTransactionIsolation()) {
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
    }

    protected void closeConnection() throws SQLException {
        assert (conn != null);

        out.println();
        out.println("releasing jdbc resources ...");

        out.println();
        out.print("closing jdbc connection ...");
        out.flush();
        if (delAllB != null)
            delAllB.close();
        delAllB = null;
        if (delAllA != null)
            delAllA.close();
        delAllA = null;
        if (conn != null)
            conn.close();
        conn = null;
        out.println("     [ok]");
    }

    protected void clearPersistenceContext() {
        // nothing to do as long as we're not caching beyond Tx scope
    }

    protected void clearData() throws SQLException {
        conn.setAutoCommit(false);
        out.print("deleting all rows ...");
        out.flush();
        int delB = delAllB.executeUpdate();
        out.print("           [B: " + delB);
        out.flush();
        int delA = delAllA.executeUpdate();
        out.print(", A: " + delA);
        out.flush();
        conn.commit();
        out.println("]");
    }

    // ----------------------------------------------------------------------

    static public void main(String[] args) {
        System.out.println("JdbcLoad.main()");
        parseArguments(args);
        new JdbcLoad().run();
        System.out.println();
        System.out.println("JdbcLoad.main(): done.");
    }
}
