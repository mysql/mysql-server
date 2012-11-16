/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package jdbctest;

import java.math.BigDecimal;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.util.Properties;

public class UpdateDecimalTypesTest extends testsuite.clusterj.DecimalTypesTest {

    /** Test all DecimalTypes columns.
drop table if exists decimaltypes;
create table decimaltypes (
 id int not null primary key,

 decimal_null_hash decimal(10,5),
 decimal_null_btree decimal(10,5),
 decimal_null_both decimal(10,5),
 decimal_null_none decimal(10,5)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_decimal_null_hash using hash on decimaltypes(decimal_null_hash);
create index idx_decimal_null_btree on decimaltypes(decimal_null_btree);
create unique index idx_decimal_null_both on decimaltypes(decimal_null_both);

     */

    /** One of two tests in the superclass that we don't want to run */
    @Override
    public void testWriteJDBCReadNDB() {
    }

    /** One of two tests in the superclass that we don't want to run */
    @Override
    public void testWriteNDBReadJDBC() {
   }

    @Override
    protected int getNumberOfInstances() {
        return 5;
    }

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        setAutoCommit(connection, false);
        if (getModelClass() != null && getCleanupAfterTest()) {
            addTearDownClasses(getModelClass());
        }
    }

    @Override
    protected boolean getCleanupAfterTest() {
        return true;
    }

    public void testAllVariants() {
        // initialization is the same for all variants
        generateInstances(getColumnDescriptors());
        writeToJDBC(columnDescriptors, instances);
        // test all variants of server-prepared statement and rewrite-batch
        rewrite_server();
        rewrite_server_autocommit();
        rewrite();
        rewrite_autocommit();
        server();
        server_autocommit();
        none();
        none_autocommit();
        failOnError();
    }

    public void rewrite_server() {
        Properties extraProperties = new Properties();
        extraProperties.put("rewriteBatchedStatements", "true");
        extraProperties.put("useServerPrepStmts", "true");
        extraProperties.put("cachePrepStmts", "true");
        updateAndCheck(extraProperties, 5, false, "rewrite_server");
    }

    public void rewrite_server_autocommit() {
        Properties extraProperties = new Properties();
        extraProperties.put("rewriteBatchedStatements", "true");
        extraProperties.put("useServerPrepStmts", "true");
        extraProperties.put("cachePrepStmts", "true");
        updateAndCheck(extraProperties, 5, true, "rewrite_server");
    }

    public void rewrite() {
        Properties extraProperties = new Properties();
        extraProperties.put("rewriteBatchedStatements", "true");
        extraProperties.put("useServerPrepStmts", "false");
        extraProperties.put("cachePrepStmts", "true");
        updateAndCheck(extraProperties, 4, false, "rewrite");
    }

    public void rewrite_autocommit() {
        Properties extraProperties = new Properties();
        extraProperties.put("rewriteBatchedStatements", "true");
        extraProperties.put("useServerPrepStmts", "false");
        extraProperties.put("cachePrepStmts", "true");
        updateAndCheck(extraProperties, 4, true, "rewrite");
    }

    public void server() {
        Properties extraProperties = new Properties();
        extraProperties.put("rewriteBatchedStatements", "false");
        extraProperties.put("useServerPrepStmts", "true");
        extraProperties.put("cachePrepStmts", "true");
        updateAndCheck(extraProperties, 3, false, "server");
    }

    public void server_autocommit() {
        Properties extraProperties = new Properties();
        extraProperties.put("rewriteBatchedStatements", "false");
        extraProperties.put("useServerPrepStmts", "true");
        extraProperties.put("cachePrepStmts", "true");
        updateAndCheck(extraProperties, 3, true, "server");
    }

    public void none() {
        Properties extraProperties = new Properties();
        extraProperties.put("rewriteBatchedStatements", "false");
        extraProperties.put("useServerPrepStmts", "false");
        extraProperties.put("cachePrepStmts", "true");
        updateAndCheck(extraProperties, 2, false, "none");
    }

    public void none_autocommit() {
        Properties extraProperties = new Properties();
        extraProperties.put("rewriteBatchedStatements", "false");
        extraProperties.put("useServerPrepStmts", "false");
        extraProperties.put("cachePrepStmts", "true");
        updateAndCheck(extraProperties, 2, true, "none");
    }

    private void updateAndCheck(Properties extraProperties, int updateMultiplier, boolean autocommit, String where) {
        // close the existing connection (it has the wrong connection properties)
        closeConnection();
        getConnection(extraProperties);
        updateByPrimaryKey(updateMultiplier, autocommit, where);
        updateByUniqueKey(updateMultiplier, autocommit, where);
    }

    /** Update by primary key */
    public void updateByPrimaryKey(int multiplier, boolean autocommit, String where) {
        // the expected results are e.g. [1, 1, 1, 1, 0] for number of instances 4
        // the last update is for a row that does not exist (one more than the number of rows in the table)
        int[] expected = new int[instances.size() + 1];
        for (int i = 0; i < getNumberOfInstances(); ++i) {
            expected[i] = 1;
        }
        try {
            connection.setAutoCommit(autocommit);
            PreparedStatement preparedStatement = connection.prepareStatement("UPDATE decimaltypes SET " +
                    "decimal_null_hash = ?, " +
                    "decimal_null_both = ?, " +
                    "decimal_null_none = ?, " +
                    "decimal_null_btree = ? " +
                    "WHERE id = ?");
            for (int i = 0; i < instances.size() + 1; ++i) {
                preparedStatement.setBigDecimal(1, new BigDecimal(i * multiplier));
                preparedStatement.setBigDecimal(2, new BigDecimal(2 * i * multiplier));
                preparedStatement.setBigDecimal(3, new BigDecimal(3 * i * multiplier));
                preparedStatement.setBigDecimal(4, new BigDecimal(4 * i * multiplier));
                preparedStatement.setInt(5, i);
                preparedStatement.addBatch();
            }
            int[] actual = preparedStatement.executeBatch();
            errorIfNotEqual("Results of executeBatch for update {server, rewrite}: " + where, expected, actual);
            queryAndVerifyResults("decimal_null_btree equal", columnDescriptors,
                    "decimal_null_btree = ?", new BigDecimal[] {BigDecimal.valueOf(4 * multiplier)}, 1);
        } catch (SQLException e) {
            error(e.getMessage());
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }

    /** Update by unique key */
    public void updateByUniqueKey(int multiplier, boolean autocommit, String where) {
        // the expected results are e.g. [1, 1, 1, 1, 0] for number of instances 4
        // the last update is for a row that does not exist (one more than the number of rows in the table)
        int[] expected = new int[instances.size() + 1];
        for (int i = 0; i < getNumberOfInstances(); ++i) {
            expected[i] = 1;
        }
        try {
            connection.setAutoCommit(autocommit);
            PreparedStatement preparedStatement = connection.prepareStatement("UPDATE decimaltypes SET " +
                    "decimal_null_both = ?, " +
                    "decimal_null_none = ?, " +
                    "decimal_null_btree = ? " +
                    "WHERE decimal_null_hash = ?");
            for (int i = 0; i < instances.size() + 1; ++i) {
                preparedStatement.setBigDecimal(1, new BigDecimal(2 * i * multiplier * 20));
                preparedStatement.setBigDecimal(2, new BigDecimal(3 * i * multiplier * 20));
                preparedStatement.setBigDecimal(3, new BigDecimal(4 * i * multiplier * 20));
                preparedStatement.setBigDecimal(4, new BigDecimal(i * multiplier));
                preparedStatement.addBatch();
            }
            int[] actual = preparedStatement.executeBatch();
            errorIfNotEqual("Results of executeBatch for update {server, rewrite}: " + where, expected, actual);
            queryAndVerifyResults("decimal_null_btree equal", columnDescriptors,
                    "decimal_null_btree = ?", new BigDecimal[] {BigDecimal.valueOf(4 * multiplier * 20)}, 1);
        } catch (SQLException e) {
            error(e.getMessage());
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }

}
