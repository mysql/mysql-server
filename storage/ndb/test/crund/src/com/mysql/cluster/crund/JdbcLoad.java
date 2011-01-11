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

import java.util.Iterator;
import java.util.Arrays;

import java.sql.SQLException;
import java.sql.DriverManager;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;


/**
 * A benchmark implementation against a JDBC/SQL database.
 */
public class JdbcLoad extends CrundDriver {

    // JDBC settings
    protected String driver;
    protected String url;
    protected String user;
    protected String password;
    protected boolean autoCommit;

    // JDBC resources
    protected Connection conn;
    protected PreparedStatement delAllA;
    protected PreparedStatement delAllB0;

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

        // single ops not supported yet
        autoCommit = parseBoolean("jdbc.autoCommit", false);
        if (autoCommit) {
            msg.append("[ignored] autoCommit:           " + autoCommit + eol);
            autoCommit = false;
        }

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
        out.println("jdbc.autoCommit:                " + autoCommit);
    }

    // ----------------------------------------------------------------------
    // JDBC operations
    // ----------------------------------------------------------------------

    protected abstract class JdbcOp extends Op {
        final protected String sql;
        protected PreparedStatement stmt;

        public JdbcOp(String name, String sql) {
            super(name);
            this.sql = sql;
        }

        public void init() throws SQLException {
            if (stmt == null)
                stmt = conn.prepareStatement(sql);
        }

        public void close() throws SQLException {
            if (stmt != null)
                stmt.close();
            stmt = null;
        }
    };

    static protected void setCommonAttributes(PreparedStatement stmt, int i)
        throws SQLException {
        stmt.setInt(2, i);
        stmt.setLong(3, (long)i);
        stmt.setFloat(4, (float)i);
        stmt.setDouble(5, (double)i);
    }

    static protected int getCommonAttributes(ResultSet rs)
        throws SQLException {
        final int cint = rs.getInt(2);
        final long clong = rs.getLong(3);
        verify(clong == cint);
        final float cfloat = rs.getFloat(4);
        verify(cfloat == cint);
        final double cdouble = rs.getDouble(5);
        verify(cdouble == cint);
        return cint;
    }

    protected void initOperations() throws SQLException {
        out.print("initializing statements ...");
        out.flush();

        for (boolean f = false, done = false; !done; done = f, f = true) {
            // inner classes can only refer to a constant
            final boolean batch = f;
            final boolean ignore = f;
            final boolean setAttrs = true;

            ops.add(
                new JdbcOp("insA" + (batch ? "_batch" : ""),
                           "INSERT INTO a (id) VALUES (?)") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countA; i++) {
                            stmt.setInt(1, i);
                            if (batch) {
                                stmt.addBatch();
                            } else {
                                int cnt = stmt.executeUpdate();
                                verify(cnt == 1);
                            }
                        }
                        if (batch) {
                            int[] cnts = stmt.executeBatch();
                            for (int i = 0; i < cnts.length; i++) {
                                verify(cnts[i] == 1);
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("insB0" + (batch ? "_batch" : ""),
                           "INSERT INTO b0 (id) VALUES (?)") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countB; i++) {
                            stmt.setInt(1, i);
                            if (batch) {
                                stmt.addBatch();
                            } else {
                                int cnt = stmt.executeUpdate();
                                verify(cnt == 1);
                            }
                        }
                        if (batch) {
                            int[] cnts = stmt.executeBatch();
                            for (int i = 0; i < cnts.length; i++) {
                                verify(cnts[i] == 1);
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("setAByPK" + (batch ? "_batch" : ""),
                           "UPDATE a a SET a.cint = ?, a.clong = ?, a.cfloat = ?, a.cdouble = ? WHERE (a.id = ?)") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countA; i++) {
                            // refactor by numbered args
                            stmt.setInt(1, i);
                            stmt.setInt(2, i);
                            stmt.setInt(3, i);
                            stmt.setInt(4, i);
                            stmt.setInt(5, i);
                            if (batch) {
                                stmt.addBatch();
                            } else {
                                int cnt = stmt.executeUpdate();
                                verify(cnt == 1);
                            }
                        }
                        if (batch) {
                            int[] cnts = stmt.executeBatch();
                            for (int i = 0; i < cnts.length; i++) {
                                verify(cnts[i] == 1);
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("setB0ByPK" + (batch ? "_batch" : ""),
                           "UPDATE b0 b0 SET b0.cint = ?, b0.clong = ?, b0.cfloat = ?, b0.cdouble = ? WHERE (b0.id = ?)") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countB; i++) {
                            // refactor by numbered args
                            stmt.setInt(1, i);
                            stmt.setInt(2, i);
                            stmt.setInt(3, i);
                            stmt.setInt(4, i);
                            stmt.setInt(5, i);
                            if (batch) {
                                stmt.addBatch();
                            } else {
                                int cnt = stmt.executeUpdate();
                                verify(cnt == 1);
                            }
                        }
                        if (batch) {
                            int[] cnts = stmt.executeBatch();
                            for (int i = 0; i < cnts.length; i++) {
                                verify(cnts[i] == 1);
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp((batch ? "getAllA_asc" : "getAByPK"),
                           "SELECT id, cint, clong, cfloat, cdouble FROM a "
                           + (batch ? "ORDER BY id ASC" : "WHERE (id = ?)")) {
                    public void run(int countA, int countB) throws SQLException {
                        if (batch) {
                            ResultSet rs = stmt.executeQuery();
                            for (int i = 1; i <= countA; i++) {
                                rs.next();
                                final int id = rs.getInt(1);
                                verify(id == i);
                                final int j = getCommonAttributes(rs);
                                verify(j == id);
                            }
                            verify(!rs.next());
                            rs.close();
                        } else {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setInt(1, i);
                                ResultSet rs = stmt.executeQuery();
                                rs.next();
                                final int id = rs.getInt(1);
                                verify(id == i);
                                final int j = getCommonAttributes(rs);
                                verify(j == id);
                                verify(!rs.next());
                                rs.close();
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp((batch ? "getAllB0_asc" : "getB0ByPK"),
                           "SELECT id, cint, clong, cfloat, cdouble FROM b0 "
                           + (batch ? "ORDER BY id ASC" : "WHERE (id = ?)")) {
                    public void run(int countA, int countB) throws SQLException {
                        if (batch) {
                            ResultSet rs = stmt.executeQuery();
                            for (int i = 1; i <= countB; i++) {
                                rs.next();
                                final int id = rs.getInt(1);
                                verify(id == i);
                                final int j = getCommonAttributes(rs);
                                verify(j == id);
                            }
                            verify(!rs.next());
                            rs.close();
                        } else {
                            for (int i = 1; i <= countB; i++) {
                                stmt.setInt(1, i);
                                ResultSet rs = stmt.executeQuery();
                                rs.next();
                                final int id = rs.getInt(1);
                                verify(id == i);
                                final int j = getCommonAttributes(rs);
                                verify(j == id);
                                verify(!rs.next());
                                rs.close();
                            }
                        }
                        conn.commit();
                    }
                });

            for (int i = 0, l = 1; l <= maxVarbinaryBytes; l *= 10, i++) {
                final byte[] b = bytes[i];
                assert l == b.length;

                ops.add(
                    new JdbcOp("setVarbinary" + l + (batch ? "_batch" : ""),
                               "UPDATE b0 b0 SET b0.cvarbinary_def = ? WHERE (b0.id = ?)") {
                        public void run(int countA, int countB)
                            throws SQLException {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setBytes(1, b);
                                stmt.setInt(2, i);
                                if (batch) {
                                    stmt.addBatch();
                                } else {
                                    int cnt = stmt.executeUpdate();
                                    verify(cnt == 1);
                                }
                            }
                            if (batch) {
                                int[] cnts = stmt.executeBatch();
                                for (int i = 0; i < cnts.length; i++) {
                                    verify(cnts[i] == 1);
                                }
                            }
                            conn.commit();
                        }
                    });

                ops.add(
                    new JdbcOp("getVarbinary" + l,
                               "SELECT cvarbinary_def FROM b0 WHERE (id = ?)") {
                        public void run(int countA, int countB)
                            throws SQLException {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setInt(1, i);
                                ResultSet rs = stmt.executeQuery();
                                rs.next();
                                final byte[] r = rs.getBytes(1);
                                verify(Arrays.equals(b, r));
                                verify(!rs.next());
                                rs.close();
                            }
                            conn.commit();
                        }
                    });

                ops.add(
                    new JdbcOp("clearVarbinary" + l + (batch ? "_batch" : ""),
                               "UPDATE b0 b0 SET b0.cvarbinary_def = NULL WHERE (b0.id = ?)") {
                        public void run(int countA, int countB)
                            throws SQLException {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setInt(1, i);
                                if (batch) {
                                    stmt.addBatch();
                                } else {
                                    int cnt = stmt.executeUpdate();
                                    verify(cnt == 1);
                                }
                            }
                            if (batch) {
                                int[] cnts = stmt.executeBatch();
                                for (int i = 0; i < cnts.length; i++) {
                                    verify(cnts[i] == 1);
                                }
                            }
                            conn.commit();
                        }
                    });
            }

            for (int i = 0, l = 1; l <= maxVarcharChars; l *= 10, i++) {
                final String s = strings[i];
                assert l == s.length();

                ops.add(
                    new JdbcOp("setVarchar" + l + (batch ? "_batch" : ""),
                               "UPDATE b0 b0 SET b0.cvarchar_def = ? WHERE (b0.id = ?)") {
                        public void run(int countA, int countB)
                            throws SQLException {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setString(1, s);
                                stmt.setInt(2, i);
                                if (batch) {
                                    stmt.addBatch();
                                } else {
                                    int cnt = stmt.executeUpdate();
                                    verify(cnt == 1);
                                }
                            }
                            if (batch) {
                                int[] cnts = stmt.executeBatch();
                                for (int i = 0; i < cnts.length; i++) {
                                    verify(cnts[i] == 1);
                                }
                            }
                            conn.commit();
                        }
                    });

                ops.add(
                    new JdbcOp("getVarchar" + l,
                               "SELECT cvarchar_def FROM b0 WHERE (id = ?)") {
                        public void run(int countA, int countB)
                            throws SQLException {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setInt(1, i);
                                ResultSet rs = stmt.executeQuery();
                                rs.next();
                                final String r = rs.getString(1);
                                verify(s.equals(r));
                                verify(!rs.next());
                                rs.close();
                            }
                            conn.commit();
                        }
                    });

                ops.add(
                    new JdbcOp("clearVarchar" + l + (batch ? "_batch" : ""),
                               "UPDATE b0 b0 SET b0.cvarchar_def = NULL WHERE (b0.id = ?)") {
                        public void run(int countA, int countB)
                            throws SQLException {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setInt(1, i);
                                if (batch) {
                                    stmt.addBatch();
                                } else {
                                    int cnt = stmt.executeUpdate();
                                    verify(cnt == 1);
                                }
                            }
                            if (batch) {
                                int[] cnts = stmt.executeBatch();
                                for (int i = 0; i < cnts.length; i++) {
                                    verify(cnts[i] == 1);
                                }
                            }
                            conn.commit();
                        }
                    });
            }

            if (false) { // works but not implemented in other backends yet
            for (int i = 3, l = 1000; l <= maxBlobBytes; l *= 10, i++) {
                final byte[] b = bytes[i];
                assert l == b.length;

                ops.add(
                    new JdbcOp("setBlob" + l + (batch ? "_batch" : ""),
                               "UPDATE b0 b0 SET b0.cblob_def = ? WHERE (b0.id = ?)") {
                        public void run(int countA, int countB)
                            throws SQLException {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setBytes(1, b);
                                stmt.setInt(2, i);
                                if (batch) {
                                    stmt.addBatch();
                                } else {
                                    int cnt = stmt.executeUpdate();
                                    verify(cnt == 1);
                                }
                            }
                            if (batch) {
                                int[] cnts = stmt.executeBatch();
                                for (int i = 0; i < cnts.length; i++) {
                                    verify(cnts[i] == 1);
                                }
                            }
                            conn.commit();
                        }
                    });

                ops.add(
                    new JdbcOp("getBlob" + l,
                               "SELECT cblob_def FROM b0 WHERE (id = ?)") {
                        public void run(int countA, int countB)
                            throws SQLException {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setInt(1, i);
                                ResultSet rs = stmt.executeQuery();
                                rs.next();
                                final byte[] r = rs.getBytes(1);
                                verify(Arrays.equals(b, r));
                                verify(!rs.next());
                                rs.close();
                            }
                            conn.commit();
                        }
                    });
            }
            }
            
            if (false) { // works but not implemented in other backends yet
            for (int i = 3, l = 1000; l <= maxTextChars; l *= 10, i++) {
                final String s = strings[i];
                assert l == s.length();

                ops.add(
                    new JdbcOp("setText" + l + (batch ? "_batch" : ""),
                               "UPDATE b0 b0 SET b0.ctext_def = ? WHERE (b0.id = ?)") {
                        public void run(int countA, int countB)
                            throws SQLException {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setString(1, s);
                                stmt.setInt(2, i);
                                if (batch) {
                                    stmt.addBatch();
                                } else {
                                    int cnt = stmt.executeUpdate();
                                    verify(cnt == 1);
                                }
                            }
                            if (batch) {
                                int[] cnts = stmt.executeBatch();
                                for (int i = 0; i < cnts.length; i++) {
                                    verify(cnts[i] == 1);
                                }
                            }
                            conn.commit();
                        }
                    });

                ops.add(
                    new JdbcOp("getText" + l,
                               "SELECT ctext_def FROM b0 WHERE (id = ?)") {
                        public void run(int countA, int countB)
                            throws SQLException {
                            for (int i = 1; i <= countA; i++) {
                                stmt.setInt(1, i);
                                ResultSet rs = stmt.executeQuery();
                                rs.next();
                                final String r = rs.getString(1);
                                verify(s.equals(r));
                                verify(!rs.next());
                                rs.close();
                            }
                            conn.commit();
                        }
                    });
            }
            }
            
            ops.add(
                new JdbcOp("setB0->A" + (batch ? "_batch" : ""),
                           "UPDATE b0 b0 SET b0.a_id = ? WHERE (b0.id = ?)") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countB; i++) {
                            int aId = ((i - 1) % countA) + 1;
                            stmt.setInt(1, aId);
                            stmt.setInt(2, i);
                            if (batch) {
                                stmt.addBatch();
                            } else {
                                int cnt = stmt.executeUpdate();
                                verify(cnt == 1);
                            }
                        }
                        if (batch) {
                            int[] cnts = stmt.executeBatch();
                            for (int i = 0; i < cnts.length; i++) {
                                verify(cnts[i] == 1);
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("navB0->A_subsel",
                           "SELECT id, cint, clong, cfloat, cdouble FROM a WHERE id = (SELECT b0.a_id FROM b0 b0 WHERE b0.id = ?)") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countB; i++) {
                            stmt.setInt(1, i);
                            ResultSet rs = stmt.executeQuery();
                            rs.next();
                            final int id = rs.getInt(1);
                            verify(id == ((i - 1) % countA) + 1);
                            final int j = getCommonAttributes(rs);
                            verify(j == id);
                            verify(!rs.next());
                            rs.close();
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("navB0->A_joinproj",
                           "SELECT a.id, a.cint, a.clong, a.cfloat, a.cdouble FROM a a, b0 b0 WHERE (a.id = b0.a_id AND b0.id = ?)") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countB; i++) {
                            stmt.setInt(1, i);
                            ResultSet rs = stmt.executeQuery();
                            rs.next();
                            final int id = rs.getInt(1);
                            verify(id == ((i - 1) % countA) + 1);
                            final int j = getCommonAttributes(rs);
                            verify(j == id);
                            verify(!rs.next());
                            rs.close();
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("navB0->A_2stmts",
                           "SELECT id, cint, clong, cfloat, cdouble FROM a WHERE id = ?") {

                    protected PreparedStatement stmt0;

                    public void init() throws SQLException {
                        super.init();
                        stmt0 = conn.prepareStatement("SELECT a_id FROM b0 WHERE id = ?");
                    }

                    public void close() throws SQLException {
                        stmt0.close();
                        stmt0 = null;
                        super.close();
                    }

                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countB; i++) {
                            // fetch a.id
                            stmt0.setInt(1, i);
                            ResultSet rs0 = stmt0.executeQuery();
                            rs0.next();
                            int aId = rs0.getInt(1);
                            verify(aId == ((i - 1) % countA) + 1);
                            verify(!rs0.next());
                            rs0.close();

                            stmt.setInt(1, aId);
                            ResultSet rs = stmt.executeQuery();
                            rs.next();
                            final int id = rs.getInt(1);
                            verify(id == aId);
                            final int j = getCommonAttributes(rs);
                            verify(j == aId);
                            verify(!rs.next());
                            rs.close();
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("navA->B0",
                           "SELECT id, cint, clong, cfloat, cdouble FROM b0 WHERE (a_id = ?)") {
                    public void run(int countA, int countB) throws SQLException {
                        int cnt = 0;
                        for (int i = 1; i <= countA; i++) {
                            stmt.setInt(1, i);
                            ResultSet rs = stmt.executeQuery();
                            while (rs.next()) {
                                final int id = rs.getInt(1);
                                verify(((id - 1) % countA) + 1 == i);
                                final int j = getCommonAttributes(rs);
                                verify(j == id);
                                cnt++;
                            }
                            rs.close();
                        }
                        verify(cnt == countB);
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("nullB0->A" + (batch ? "_batch" : ""),
                           "UPDATE b0 b0 SET b0.a_id = NULL WHERE (b0.id = ?)") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countB; i++) {
                            stmt.setInt(1, i);
                            if (batch) {
                                stmt.addBatch();
                            } else {
                                int cnt = stmt.executeUpdate();
                                verify(cnt == 1);
                            }
                        }
                        if (batch) {
                            int[] cnts = stmt.executeBatch();
                            for (int i = 0; i < cnts.length; i++) {
                                verify(cnts[i] == 1);
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                // MySQL rejects this syntax: "DELETE FROM b0 b0 WHERE b0.id = ?"
                new JdbcOp("delB0ByPK" + (batch ? "_batch" : ""),
                           "DELETE FROM b0 WHERE id = ?") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countB; i++) {
                            stmt.setInt(1, i);
                            if (batch) {
                                stmt.addBatch();
                            } else {
                                int cnt = stmt.executeUpdate();
                                verify(cnt == 1);
                            }
                        }
                        if (batch) {
                            int[] cnts = stmt.executeBatch();
                            for (int i = 0; i < cnts.length; i++) {
                                verify(cnts[i] == 1);
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                // MySQL rejects this syntax: "DELETE FROM a a WHERE a.id = ?"
                new JdbcOp("delAByPK" + (batch ? "_batch" : ""),
                           "DELETE FROM a WHERE id = ?") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countA; i++) {
                            stmt.setInt(1, i);
                            if (batch) {
                                stmt.addBatch();
                            } else {
                                int cnt = stmt.executeUpdate();
                                verify(cnt == 1);
                            }
                        }
                        if (batch) {
                            int[] cnts = stmt.executeBatch();
                            for (int i = 0; i < cnts.length; i++) {
                                verify(cnts[i] == 1);
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("insA_attr" + (batch ? "_batch" : ""),
                           "INSERT INTO a (id, cint, clong, cfloat, cdouble) VALUES (?, ?, ?, ?, ?)") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countA; i++) {
                            stmt.setInt(1, i);
                            setCommonAttributes(stmt, -i);
                            if (batch) {
                                stmt.addBatch();
                            } else {
                                int cnt = stmt.executeUpdate();
                                verify(cnt == 1);
                            }
                        }
                        if (batch) {
                            int[] cnts = stmt.executeBatch();
                            for (int i = 0; i < cnts.length; i++) {
                                verify(cnts[i] == 1);
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("insB0_attr" + (batch ? "_batch" : ""),
                           "INSERT INTO b0 (id, cint, clong, cfloat, cdouble) VALUES (?, ?, ?, ?, ?)") {
                    public void run(int countA, int countB) throws SQLException {
                        for (int i = 1; i <= countB; i++) {
                            stmt.setInt(1, i);
                            setCommonAttributes(stmt, -i);
                            if (batch) {
                                stmt.addBatch();
                            } else {
                                int cnt = stmt.executeUpdate();
                                verify(cnt == 1);
                            }
                        }
                        if (batch) {
                            int[] cnts = stmt.executeBatch();
                            for (int i = 0; i < cnts.length; i++) {
                                verify(cnts[i] == 1);
                            }
                        }
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("delAllB0",
                           "DELETE FROM b0") {
                    public void run(int countA, int countB) throws SQLException {
                        int cnt = stmt.executeUpdate();
                        verify(cnt == countB);
                        conn.commit();
                    }
                });

            ops.add(
                new JdbcOp("delAllA",
                           "DELETE FROM a") {
                    public void run(int countA, int countB) throws SQLException {
                        int cnt = stmt.executeUpdate();
                        verify(cnt == countA);
                        conn.commit();
                    }
                });
        }

        // prepare all statements
        for (Iterator<CrundDriver.Op> i = ops.iterator(); i.hasNext();) {
            ((JdbcOp)i.next()).init();
        }
        out.println("     [JdbcOp: " + ops.size() + "]");
    }

    protected void closeOperations() throws SQLException {
        out.print("closing statements ...");
        out.flush();
        // close all statements
        for (Iterator<CrundDriver.Op> i = ops.iterator(); i.hasNext();) {
            ((JdbcOp)i.next()).close();
        }
        ops.clear();
        out.println("          [ok]");
    }

    // ----------------------------------------------------------------------
    // JDBC datastore operations
    // ----------------------------------------------------------------------

    protected void initConnection() throws SQLException {
        out.print("initializing connection ...");
        out.flush();
        conn = DriverManager.getConnection(url, user, password);
        conn.setAutoCommit(autoCommit);
        delAllA = conn.prepareStatement("DELETE FROM a");
        delAllB0 = conn.prepareStatement("DELETE FROM b0");
        out.println("     [Conn: 1]");
    }

    protected void closeConnection() throws SQLException {
        out.print("closing connection ...");
        out.flush();
        if (delAllB0 != null)
            delAllB0.close();
        delAllB0 = null;
        if (delAllA != null)
            delAllA.close();
        delAllA = null;
        if (conn != null)
            conn.close();
        conn = null;
        out.println("          [ok]");
    }

    protected void clearPersistenceContext() {
        // nothing to do as long as we're not caching beyond Tx scope
    }

    protected void clearData() throws SQLException {
        out.print("deleting all rows ...");
        out.flush();
        int delB0 = delAllB0.executeUpdate();
        out.print("           [B0: " + delB0);
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
