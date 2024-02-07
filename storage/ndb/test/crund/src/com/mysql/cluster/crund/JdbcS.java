/*
  Copyright (c) 2010, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.cluster.crund;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;
import java.sql.PreparedStatement;
import java.sql.ResultSet;

import com.mysql.cluster.crund.CrundDriver.XMode;

class JdbcS extends CrundSLoad {
    // JDBC settings
    protected String jdbcDriver;
    protected String url;
    protected String username;
    protected String password;

    // JDBC resources
    protected Class jdbcDriverClass;
    protected Connection connection;
    protected String sqlIns0;
    protected String sqlSel0;
    protected String sqlUpd0;
    protected String sqlDel0;
    protected String sqlDelAll;
    protected PreparedStatement ins0;
    protected PreparedStatement sel0;
    protected PreparedStatement upd0;
    protected PreparedStatement del0;
    protected PreparedStatement delAll;

    public JdbcS(CrundDriver driver) {
        super(driver);
    }

    // ----------------------------------------------------------------------
    // JDBC intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        out.println();
        out.print("setting jdbc properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        // load the JDBC driver class
        jdbcDriver = driver.props.getProperty("jdbc.driver");
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

        url = driver.props.getProperty("jdbc.url");
        if (url == null) {
            throw new RuntimeException("Missing property: jdbc.url");
        }

        username = driver.props.getProperty("jdbc.user");
        password = driver.props.getProperty("jdbc.password");

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
        out.println("jdbc.driver:                    " + jdbcDriver);
        out.println("jdbc.url:                       " + url);
        out.println("jdbc.user:                      \"" + username + "\"");
        out.println("jdbc.password:                  \"" + password + "\"");
    }

    public void init() throws Exception {
        super.init();
        assert (jdbcDriverClass == null);

        // load the JDBC driver class
        out.print("loading jdbc driver ...");
        out.flush();
        try {
            jdbcDriverClass = Class.forName(jdbcDriver);
        } catch (ClassNotFoundException e) {
            out.println("Cannot load JDBC driver '" + jdbcDriver
                        + "' from classpath '"
                        + System.getProperty("java.class.path") + "'");
            throw new RuntimeException(e);
        }
        out.println("         [ok: " + jdbcDriverClass.getName() + "]");
    }

    public void close() throws Exception {
        assert (jdbcDriverClass != null);

        //out.println();
        jdbcDriverClass = null;

        super.close();
    }

    // ----------------------------------------------------------------------
    // JDBC datastore operations
    // ----------------------------------------------------------------------

    public void initConnection() throws SQLException {
        assert (jdbcDriverClass != null);
        assert (connection == null);

        out.println();
        out.println("initializing jdbc resources ...");

        // create a connection to the database
        out.print("starting jdbc connection ...");
        out.flush();
        try {
            connection = DriverManager.getConnection(url, username, password);
        } catch (SQLException e) {
            out.println("Cannot connect to database '" + url + "'");
            throw new RuntimeException(e);
        }
        out.println("    [ok: " + url + "]");

        out.print("setting isolation level ...");
        out.flush();
        // ndb storage engine only supports READ_COMMITTED
        final int il = Connection.TRANSACTION_READ_COMMITTED;
        connection.setTransactionIsolation(il);
        out.print("     [ok: ");
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

        initPreparedStatements();
    }

    public void closeConnection() throws SQLException {
        assert (connection != null);

        out.println();
        out.println("releasing jdbc resources ...");

        closePreparedStatements();

        out.print("closing jdbc connection ...");
        out.flush();
        connection.close();
        connection = null;
        out.println("     [ok]");
    }

    public void clearData() throws SQLException {
        connection.setAutoCommit(false);
        out.print("deleting all rows ...");
        out.flush();
        final int d = delAll.executeUpdate();
        connection.commit();
        out.println("           [S: " + d + "]");
    }

    public void initPreparedStatements() throws SQLException {
        assert (connection != null);
        assert (ins0 == null);
        assert (sel0 == null);
        assert (upd0 == null);
        assert (del0 == null);

        out.print("using lock mode for reads ...");
        out.flush();
        final String lm;
        switch (driver.lockMode) {
        case none:
            lm = "";
            break;
        case shared:
            lm = " LOCK IN share mode";
            break;
        case exclusive:
            lm = " FOR UPDATE";
            break;
        default:
            lm = "";
            assert false;
        }
        out.println("   [ok: " + "SELECT" + lm + ";]");

        out.print("compiling jdbc statements ...");
        out.flush();

        sqlIns0 = "INSERT INTO S (c0, c1, c2, c3, c5, c6, c7, c8) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        sqlSel0 = "SELECT * FROM S WHERE c0=?" + lm;
        sqlUpd0 = "UPDATE S SET c1 = ?, c2 = ?, c3 = ?, c5 = ?, c6 = ?, c7 = ?, c8 = ? WHERE c0=?";
        sqlDel0 = "DELETE FROM S WHERE c0=?";
        sqlDelAll = "DELETE FROM S";

        ins0 = connection.prepareStatement(sqlIns0);
        sel0 = connection.prepareStatement(sqlSel0);
        upd0 = connection.prepareStatement(sqlUpd0);
        del0 = connection.prepareStatement(sqlDel0);
        delAll = connection.prepareStatement(sqlDelAll);

        out.println("   [ok]");
    }

    protected void closePreparedStatements() throws SQLException {
        assert (ins0 != null);
        assert (sel0 != null);
        assert (upd0 != null);
        assert (del0 != null);
        assert (delAll != null);

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
        delAll.close();
        delAll = null;
        out.println("     [ok]");
    }

    // ----------------------------------------------------------------------

    protected void runInsert(XMode mode, int[] id) throws SQLException {
        final String name = "S_insAttr," + mode;
        final int n = id.length;
        driver.beginOp(name);
        connection.setAutoCommit(mode == XMode.indy);
        for(int i = 0; i < n; i++)
            insert(mode, id[i]);
        if (mode == XMode.bulk)
            ins0.executeBatch();
        if (mode != XMode.indy)
            connection.commit();
        driver.finishOp(name, n);
    }

    protected void insert(XMode mode, int id) throws SQLException {
        final int i = id;
        final String str = Integer.toString(i);
        ins0.setString(1, str); // key
        ins0.setString(2, str);
        ins0.setInt(3, i);
        ins0.setInt(4, i);
        ins0.setString(5, str);
        ins0.setString(6, str);
        ins0.setString(7, str);
        ins0.setString(8, str);
        if (mode == XMode.bulk) {
            ins0.addBatch();
        } else {
            int cnt = ins0.executeUpdate();
            assert (cnt == 1);
        }
    }

    // ----------------------------------------------------------------------

    protected void runLookup(XMode mode, int[] id) throws SQLException {
        final String name = "S_getAttr," + mode;
        final int n = id.length;
        driver.beginOp(name);
        connection.setAutoCommit(mode == XMode.indy);
        if (mode != XMode.bulk) {
            for(int i = 0; i < n; i++)
                lookup(id[i]);
            if (mode != XMode.indy)
                connection.commit();
        } else {
            lookup(id);
            connection.commit();
        }
        driver.finishOp(name, n);
    }

    protected void lookup(int[] id) throws SQLException {
        final int n = id.length;

        // use dynamic SQL for generic bulk queries
        // The mysql jdbc driver requires property allowMultiQueries=true
        // passed to DriverManager.getConnection() or in URL
        // jdbc:mysql://localhost/crunddb?allowMultiQueries=true
        final StringBuilder sb = new StringBuilder();
        for (int i = 0; i < n; i++)
            sb.append(sqlSel0.replace("?", "'" + id[i] + "'")).append(";");
        final String q = sb.toString();
        final Statement s = connection.createStatement();

        // allow for multi/single result sets with single/multi rows
        boolean hasRS = s.execute(q);
        int i = 0;
        while (hasRS) {
            final ResultSet rs = s.getResultSet();
            while (rs.next())
                check(id[i++], rs);
            hasRS = s.getMoreResults();
        }
        verify(n, i);
    }

    protected void lookup(int id) throws SQLException {
        sel0.setString(1, Integer.toString(id)); // key
        final ResultSet rs = sel0.executeQuery();
        int i = 0;
        while (rs.next()) {
            check(id, rs);
            i++;
        }
        verify(1, i);
        rs.close();
    }

    protected void check(int id, ResultSet rs) throws SQLException {
        // XXX not verifying at this time
        String ac0 = rs.getString(1);
        String c1 = rs.getString(2);
        int c2 = rs.getInt(3);
        int c3 = rs.getInt(4);
        int c4 = rs.getInt(5);
        String c5 = rs.getString(6);
        String c6 = rs.getString(7);
        String c7 = rs.getString(8);
        String c8 = rs.getString(9);
        String c9 = rs.getString(10);
        String c10 = rs.getString(11);
        String c11 = rs.getString(12);
        String c12 = rs.getString(13);
        String c13 = rs.getString(14);
        String c14 = rs.getString(15);
    }

    // ----------------------------------------------------------------------

    protected void runUpdate(XMode mode, int[] id) throws SQLException {
        final String name = "S_setAttr," + mode;
        final int n = id.length;
        driver.beginOp(name);
        connection.setAutoCommit(mode == XMode.indy);
        for(int i = 0; i < n; i++)
            update(mode, id[i]);
        if (mode == XMode.bulk)
            upd0.executeBatch();
        if (mode != XMode.indy)
            connection.commit();
        driver.finishOp(name, n);
    }

    protected void update(XMode mode, int id) throws SQLException {
        final String str0 = Integer.toString(id);
        final int r = -id;
        final String str1 = Integer.toString(r);
        upd0.setString(1, str1);
        upd0.setInt(2, r);
        upd0.setInt(3, r);
        upd0.setString(4, str1);
        upd0.setString(5, str1);
        upd0.setString(6, str1);
        upd0.setString(7, str1);
        upd0.setString(8, str0); // key
        if (mode == XMode.bulk) {
            upd0.addBatch();
        } else {
            int cnt = upd0.executeUpdate();
            assert (cnt == 1);
        }
    }

    // ----------------------------------------------------------------------

    protected void runDelete(XMode mode, int[] id) throws SQLException {
        final String name = "S_del," + mode;
        final int n = id.length;
        driver.beginOp(name);
        connection.setAutoCommit(mode == XMode.indy);
        for(int i = 0; i < n; i++)
            delete(mode, id[i]);
        if (mode == XMode.bulk)
            del0.executeBatch();
        if (mode != XMode.indy)
            connection.commit();
        driver.finishOp(name, n);
    }

    protected void delete(XMode mode, int id) throws SQLException {
        final String str = Integer.toString(id);
        del0.setString(1, str);
        if (mode == XMode.bulk) {
            del0.addBatch();
        } else {
            int cnt = del0.executeUpdate();
            assert (cnt == 1);
        }
    }

    // ----------------------------------------------------------------------

    protected void clearPersistenceContext() {
        // nothing to do as we're not caching beyond Tx scope
    }
}
