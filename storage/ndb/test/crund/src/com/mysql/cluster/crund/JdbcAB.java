/*
  Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

import java.util.Properties;

import java.sql.SQLException;
import java.sql.DriverManager;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.Statement;
import java.sql.Array;
import java.sql.Types;

import com.mysql.cluster.crund.CrundDriver.XMode;

/**
 * The JDBC benchmark implementation.
 */
public class JdbcAB extends CrundLoad {

    // JDBC settings
    protected String jdbcDriver;
    protected String url;
    protected String user;
    protected String password;

    // JDBC resources
    protected Connection conn;
    protected PreparedStatement delAllA;
    protected PreparedStatement delAllB;
    protected String lmSuffix;

    public JdbcAB(CrundDriver driver) {
        super(driver);
    }

    static public void main(String[] args) {
        System.out.println("JdbcAB.main()");
        CrundDriver.parseArguments(args);
        final CrundDriver driver = new CrundDriver();
        final CrundLoad load = new JdbcAB(driver);
        driver.run();
        System.out.println();
        System.out.println("JdbcAB.main(): done.");
    }

    // ----------------------------------------------------------------------
    // JDBC intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        System.out.println();
        out.print("reading JDBC properties ...");

        final StringBuilder msg = new StringBuilder();
        final Properties props = driver.props;

        // load the JDBC driver class
        jdbcDriver = props.getProperty("jdbc.driver");
        if (jdbcDriver == null) {
            throw new RuntimeException("Missing property: jdbc.driver");
        }
        try {
            Class.forName(jdbcDriver);
        } catch (ClassNotFoundException e) {
            out.println("Cannot load JDBC driver '" + jdbcDriver
                        + "' from classpath '"
                        + System.getProperty("java.class.path") + "'");
            throw new RuntimeException(e);
        }

        // have url initialized first
        url = props.getProperty("jdbc.url");
        if (url == null) {
            throw new RuntimeException("Missing property: jdbc.url");
        }
        user = props.getProperty("jdbc.user");
        password = props.getProperty("jdbc.password");

        if (msg.length() == 0) {
            out.println("     [ok]");
        } else {
            driver.hasIgnoredSettings = true;
            out.println();
            out.print(msg.toString());
        }

        name = url.substring(0, 10); // shortcut will do
    }

    protected void printProperties() {
        out.println();
        out.println("jdbc settings ...");
        out.println("jdbc.driver:                    " + jdbcDriver);
        out.println("jdbc.url:                       " + url);
        out.println("jdbc.user:                      \"" + user + "\"");
        out.println("jdbc.password:                  \"" + password + "\"");
    }

    // ----------------------------------------------------------------------
    // JDBC operations
    // ----------------------------------------------------------------------

    // current model assumption: relationships only 1:1 identity
    // (target id of a navigation operation is verified against source id)
    protected abstract class JdbcOp extends Op {
        protected String sql;
        protected PreparedStatement ps;
        protected XMode xMode;

        public JdbcOp(String name, XMode m, String sql) {
            super(name + "," + m);
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

        public void run(int[] id) throws SQLException {
            final int n = id.length;
            conn.setAutoCommit(xMode == XMode.indy);
            switch (xMode) {
            case indy :
            case each :
                for (int i = 0; i < n; i++) {
                    setParams(id[i]);
                    final int cnt = ps.executeUpdate();
                    verify(1, cnt);
                }
                if (xMode != XMode.indy)
                    conn.commit();
                break;
            case bulk :
                for(int i = 0; i < n; i++) {
                    setParams(id[i]);
                    ps.addBatch();
                }
                final int[] cnt = ps.executeBatch();
                for (int i = 0; i < n; i++)
                    verify(1, cnt[i]);
                conn.commit();
                break;
            }
        }

        protected void setParams(int id) throws SQLException {}
    };

    protected abstract class ReadOp extends JdbcOp {
        protected ResultSet rs;

        public ReadOp(String name, XMode m, String sql) {
            super(name, m, sql);
            sql += lmSuffix; // append lock mode suffix
        }

        public void run(int[] id) throws SQLException {
            final int n = id.length;
            conn.setAutoCommit(xMode == XMode.indy);
            switch (xMode) {
            case indy :
            case each :
                for (int i = 0; i < n; i++) {
                    setParams(id[i]);
                    rs = ps.executeQuery();
                    rs.next();
                    getValues(id[i]);
                    verify(!rs.next());
                    rs.close();
                    assert !ps.getMoreResults();
                }
                if (xMode != XMode.indy)
                    conn.commit();
                break;
            case bulk :
                // use dynamic SQL for generic bulk queries
                // allow for multi/single result sets with single/multi rows
                final String q = getQueries(id);
                final Statement s = conn.createStatement();
                boolean hasRS = s.execute(q);
                int i = 0;
                while (hasRS) {
                    rs = s.getResultSet();
                    while (rs.next())
                        getValues(id[i++]);
                    hasRS = s.getMoreResults();
                }
                verify(n, i);
                conn.commit();
                break;
            }
        }

        /**
         * Generic bulk query support: dynamic SQL query with substituted
         * Ids for params.
         */
        protected String getQueries(int[] id) throws SQLException {
            // default: multiply query substituting Id for all params
            return multipleQueries(id);
        }

        /**
         * Bulk query support, mechanism #1: multiplied SQL queries with
         * substituted Ids for params.
         *
         * The mysql jdbc driver requires property allowMultiQueries=true
         * passed to DriverManager.getConnection() or in URL
         * jdbc:mysql://localhost/crunddb?allowMultiQueries=true
         */
        protected String multipleQueries(int[] id) throws SQLException {
            final int n = id.length;
            final StringBuilder sb = new StringBuilder();
            for (int i = 0; i < n; i++)
                sb.append(sql.replace("?", String.valueOf(id[i]))).append(";");
            return sb.toString();
        }

        /**
         * Bulk query support, mechanism #2: where-in clause with
         * comma-separated List of Ids.
         */
        protected String whereInIdList(int[] id) throws SQLException {
            final int n = id.length;
            final StringBuilder sb = new StringBuilder();
            for (int i = 0; i < n; i++)
                sb.append(String.valueOf(id[i]) + ",");
            sb.append("null"); // need at least one value
            return sql.replace("?", sb);
        }

        /**
         * Bulk query support, mechanism #3: where-in clause with Ids
         * passed as SQL Array.
         *
         * JDBC 4 feature.  Not supported by mysql driver 5.1 (or Java DB).
         */
        protected Array idArray(int[] id) throws SQLException {
            final int n = id.length;
            final Object[] a = new Object[n];
            for (int i = 0; i < n; i++)
                a[i] = new Integer(id[i]);
            return conn.createArrayOf("integer", a);
        }

        protected void setParams(int id) throws SQLException {}
        protected void getValues(int id) throws SQLException {}
    }

    protected void initOperations() throws SQLException {
        out.print("initializing operations ...");
        out.flush();

        for (XMode m : driver.xModes) {
            // inner classes can only refer to a constant
            final XMode xMode = m;

            ops.add(
                new WriteOp("A_insAttr", xMode,
                            "INSERT INTO A (id, cint, clong, cfloat, cdouble) VALUES (?, ?, ?, ?, ?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                        setAttrA(ps, 2, -id);
                    }
                });

            ops.add(
                new WriteOp("B_insAttr", xMode,
                            "INSERT INTO B (id, cint, clong, cfloat, cdouble) VALUES (?, ?, ?, ?, ?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                        setAttrB(ps, 2, -id);
                    }
                });

            ops.add(
                new WriteOp("A_setAttr", xMode,
                            "UPDATE A a SET a.cint = ?, a.clong = ?, a.cfloat = ?, a.cdouble = ? WHERE (a.id = ?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(setAttrA(ps, 1, id), id);
                    }
                });

            ops.add(
                new WriteOp("B_setAttr", xMode,
                            "UPDATE B b SET b.cint = ?, b.clong = ?, b.cfloat = ?, b.cdouble = ? WHERE (b.id = ?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(setAttrB(ps, 1, id), id);
                    }
                });

            ops.add(
                new ReadOp("A_getAttr", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM A WHERE (id = ?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }

                    protected void getValues(int id) throws SQLException {
                        verify(id, rs.getInt(1));
                        verifyAttrA(id, 2, rs);
                    }
                });

            ops.add(
                new ReadOp("B_getAttr", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM B WHERE (id = ?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }

                    protected void getValues(int id) throws SQLException {
                        verify(id, rs.getInt(1));
                        verifyAttrB(id, 2, rs);
                    }
                });

            ops.add(
                new ReadOp("A_getAttr_wherein", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM A WHERE id in (?) ORDER BY id") {
                    protected String getQueries(int[] id) throws SQLException {
                        return whereInIdList(id);
                    }

                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }

                    protected void getValues(int id) throws SQLException {
                        assert rs != null;
                        verify(id, rs.getInt(1));
                        verifyAttrA(id, 2, rs);
                    }
                });

            ops.add(
                new ReadOp("B_getAttr_wherein", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM A WHERE id in (?) ORDER BY id") {
                    protected String getQueries(int[] id) throws SQLException {
                        return whereInIdList(id);
                    }

                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }

                    protected void getValues(int id) throws SQLException {
                        assert rs != null;
                        verify(id, rs.getInt(1));
                        verifyAttrB(id, 2, rs);
                    }
                });

            for (int i = 0; i < bytes.length; i++) {
                // inner classes can only refer to a constant
                final byte[] b = bytes[i];
                final int l = b.length;
                if (l > driver.maxVarbinaryBytes)
                    break;

                ops.add(
                    new WriteOp("B_setVarbin_" + l, xMode,
                                "UPDATE B b SET b.cvarbinary_def = ? WHERE (b.id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setBytes(1, b);
                            ps.setInt(2, id);
                        }
                    });

                ops.add(
                    new ReadOp("B_getVarbin_" + l, xMode,
                               "SELECT cvarbinary_def FROM B WHERE (id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setInt(1, id);
                        }

                        protected void getValues(int id) throws SQLException {
                            verify(b, rs.getBytes(1));
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarbin_" + l, xMode,
                                "UPDATE B b SET b.cvarbinary_def = NULL WHERE (b.id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setInt(1, id);
                        }
                    });
            }

            for (int i = 0; i < strings.length; i++) {
                // inner classes can only refer to a constant
                final String s = strings[i];
                final int l = s.length();
                if (l > driver.maxVarcharChars)
                    break;

                ops.add(
                    new WriteOp("B_setVarchar_" + l, xMode,
                                "UPDATE B b SET b.cvarchar_def = ? WHERE (b.id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setString(1, s);
                            ps.setInt(2, id);
                        }
                    });

                ops.add(
                    new ReadOp("B_getVarchar_" + l, xMode,
                               "SELECT cvarchar_def FROM B WHERE (id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setInt(1, id);
                        }

                        protected void getValues(int id) throws SQLException {
                            verify(s, rs.getString(1));
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarchar_" + l, xMode,
                                "UPDATE B b SET b.cvarchar_def = NULL WHERE (b.id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setInt(1, id);
                        }
                    });
            }

            for (int i = 3; i < bytes.length; i++) {
                // inner classes can only refer to a constant
                final byte[] b = bytes[i];
                final int l = b.length;
                if (l > driver.maxBlobBytes)
                    break;

                ops.add(
                    new WriteOp("B_setBlob_" + l, xMode,
                                "UPDATE B b SET b.cblob_def = ? WHERE (b.id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setBytes(1, b);
                            ps.setInt(2, id);
                        }
                    });

                ops.add(
                    new ReadOp("B_getBlob_" + l, xMode,
                               "SELECT cblob_def FROM B WHERE (id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setInt(1, id);
                        }

                        protected void getValues(int id) throws SQLException {
                            verify(b, rs.getBytes(1));
                        }
                    });

                ops.add(
                    new WriteOp("B_clearBlob_" + l, xMode,
                                "UPDATE B b SET b.cblob_def = NULL WHERE (b.id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setInt(1, id);
                        }
                    });
            }

            for (int i = 3; i < strings.length; i++) {
                // inner classes can only refer to a constant
                final String s = strings[i];
                final int l = s.length();
                if (l > driver.maxTextChars)
                    break;

                ops.add(
                    new WriteOp("B_setText_" + l, xMode,
                                "UPDATE B b SET b.ctext_def = ? WHERE (b.id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setString(1, s);
                            ps.setInt(2, id);
                        }
                    });

                ops.add(
                    new ReadOp("B_getText_" + l, xMode,
                               "SELECT ctext_def FROM B WHERE (id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setInt(1, id);
                        }

                        protected void getValues(int id) throws SQLException {
                            verify(s, rs.getString(1));
                        }
                    });

                ops.add(
                    new WriteOp("B_clearText_" + l, xMode,
                                "UPDATE B b SET b.ctext_def = NULL WHERE (b.id = ?)") {
                        protected void setParams(int id) throws SQLException {
                            ps.setInt(1, id);
                        }
                    });
            }

            ops.add(
                new WriteOp("B_setA", xMode,
                            "UPDATE B b SET b.a_id = ? WHERE (b.id = ?)") {
                    protected void setParams(int id) throws SQLException {
                        final int aId = id;
                        ps.setInt(1, aId);
                        ps.setInt(2, id);
                    }
                });

            ops.add(
                new ReadOp("B_getA_joinproj", xMode,
                           "SELECT a.id, a.cint, a.clong, a.cfloat, a.cdouble FROM A a, b b WHERE (a.id = b.a_id AND b.id = ?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }

                    protected void getValues(int id) throws SQLException {
                        verify(id, rs.getInt(1));
                        verifyAttrA(id, 2, rs);
                    }
                });

            ops.add(
                new ReadOp("B_getA_subsel", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM A WHERE id = (SELECT b.a_id FROM B b WHERE b.id = ?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }

                    protected void getValues(int id) throws SQLException {
                        verify(id, rs.getInt(1));
                        verifyAttrA(id, 2, rs);
                    }
                });

            ops.add(
                new ReadOp("A_getBs", xMode,
                           "SELECT id, cint, clong, cfloat, cdouble FROM B WHERE (a_id = ?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }

                    protected void getValues(int id) throws SQLException {
                        verify(id, rs.getInt(1));
                        verifyAttrB(id, 2, rs);
                    }
                });

            ops.add(
                new WriteOp("B_clearA", xMode,
                            "UPDATE B b SET b.a_id = NULL WHERE (b.id = ?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }
                });

            ops.add(
                // MySQL rejects this syntax: "DELETE FROM B b WHERE b.id = ?"
                new WriteOp("B_del", xMode,
                            "DELETE FROM B WHERE id = ?") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }
                });

            ops.add(
                // MySQL rejects this syntax: "DELETE FROM A a WHERE a.id = ?"
                new WriteOp("A_del", xMode,
                            "DELETE FROM A WHERE id = ?") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }
                });

            ops.add(
                new WriteOp("A_ins", xMode,
                            "INSERT INTO A (id) VALUES (?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }
                });

            ops.add(
                new WriteOp("B_ins", xMode,
                            "INSERT INTO B (id) VALUES (?)") {
                    protected void setParams(int id) throws SQLException {
                        ps.setInt(1, id);
                    }
                });

            ops.add(
                new WriteOp("B_delAll", XMode.bulk,
                            "DELETE FROM B") {
                    public void run(int[] id) throws SQLException {
                        final int n = id.length;
                        conn.setAutoCommit(true);
                        final int cnt = ps.executeUpdate();
                        verify(n, cnt);
                    }
                });

            ops.add(
                new WriteOp("A_delAll", XMode.bulk,
                            "DELETE FROM A") {
                    public void run(int[] id) throws SQLException {
                        final int n = id.length;
                        conn.setAutoCommit(true);
                        final int cnt = ps.executeUpdate();
                        verify(n, cnt);
                    }
                });
        }

        // prepare all statements
        for (Op o : ops)
            ((JdbcOp)o).init();
        out.println("     [JdbcOp: " + ops.size() + "]");
    }

    protected void closeOperations() throws SQLException {
        out.print("closing operations ...");
        out.flush();
        for (Op o : ops)
            ((JdbcOp)o).close();
        ops.clear();
        out.println("          [ok]");
    }

    protected void clearPersistenceContext() {
        // nothing to do as long as we're not caching beyond Tx scope
    }

    // ----------------------------------------------------------------------

    protected int setAttrA(PreparedStatement ps, int p, int id)
        throws SQLException {
        ps.setInt(p++, id);
        ps.setLong(p++, (long)id);
        ps.setFloat(p++, (float)id);
        ps.setDouble(p++, (double)id);
        return p;
    }

    protected int setAttrB(PreparedStatement ps, int p, int id)
        throws SQLException {
        return setAttrA(ps, p, id); // currently same as A
    }

    protected int verifyAttrA(int id, int p, ResultSet rs)
        throws SQLException {
        verify(id, rs.getInt(p++));
        verify(id, rs.getLong(p++));
        verify(id, rs.getFloat(p++));
        verify(id, rs.getDouble(p++));
        return p;
    }

    protected int verifyAttrB(int id, int p, ResultSet rs)
        throws SQLException {
        return verifyAttrA(id, p, rs); // currently same as A
    }

    // ----------------------------------------------------------------------
    // JDBC datastore operations
    // ----------------------------------------------------------------------

    public void initConnection() throws SQLException {
        assert conn == null;
        out.println();
        out.println("initializing JDBC resources ...");

        out.print("creating JDBC connection ...");
        out.flush();
        conn = DriverManager.getConnection(url, user, password);
        delAllA = conn.prepareStatement("DELETE FROM A");
        delAllB = conn.prepareStatement("DELETE FROM B");
        out.println("    [ok: 1]");

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

        out.print("using lock mode for reads ...");
        out.flush();
        switch (driver.lockMode) {
        case none:
            lmSuffix = "";
            break;
        case shared:
            lmSuffix = " LOCK IN share mode";
            break;
        case exclusive:
            lmSuffix = " FOR UPDATE";
            break;
        default:
            lmSuffix = "";
            assert false;
        }
        out.println("   [ok: " + "SELECT" + lmSuffix + "]");

        initOperations();
    }

    public void closeConnection() throws SQLException {
        assert conn != null;
        out.println();
        out.println("releasing JDBC resources ...");

        closeOperations();

        out.print("closing JDBC connection ...");
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

    public void clearData() throws SQLException {
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
}
