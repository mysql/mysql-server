/*
   Copyright 2011, Oracle and/or its affiliates. All rights reserved.

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

package jdbctest;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import testsuite.clusterj.AbstractClusterJModelTest;

public class BatchTest extends AbstractClusterJModelTest {

    private static final int NUMBER_OF_INSTANCES = 10;

    private static final Set<Integer> expecteds = new HashSet<Integer>();
    static {
        for (int i = 0; i < NUMBER_OF_INSTANCES;++i) {
            expecteds.add(i);
        }
    }
    @Override
    public boolean getDebug() {
        return false;
    }

    @Override
    public void localSetUp() {
        super.localSetUp();
    }

    public void testInsertBatch() {
        try {
            connection.setAutoCommit(false);
            deleteAll(connection);
            int[] counts = insertBatch(connection);
            if (getDebug()) System.out.println(Arrays.toString(counts));
            for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
                int count = counts[i];
                errorIfNotEqual("test executeBatch failure for " + i, 1, count);
            }
            connection.commit();
            selectAndVerifyAll(connection);
            closeConnection();
        } catch (SQLException e) {
            error("insert id, name, age, magic into t_basic values(?, ?, ?, ?) threw " + e.getMessage());
            if (getDebug()) e.printStackTrace();
            throw new RuntimeException("insert id, name, age, magic into t_basic values(?, ?, ?, ?)", e);
        } catch (RuntimeException e) {
            if (getDebug()) e.printStackTrace();
        }
        failOnError();
    }

    public void testInsertBatchAutocommit() {
        try {
            connection.setAutoCommit(false);
            deleteAll(connection);
            connection.setAutoCommit(true);
            insertBatch(connection);
            closeConnection();
        } catch (SQLException e) {
            error("insert id, name, age, magic into t_basic values(?, ?, ?, ?) threw " + e.getMessage());
            if (getDebug()) e.printStackTrace();
            throw new RuntimeException("insert id, name, age, magic into t_basic values(?, ?, ?, ?)", e);
        } catch (RuntimeException e) {
            if (getDebug()) e.printStackTrace();
        }
        failOnError();
    }

    private int deleteAll(Connection connection) throws SQLException {
        Statement deleteStatement = connection.createStatement();
        int result = deleteStatement.executeUpdate("delete from t_basic");
        deleteStatement.close();
        connection.commit();
        return result;
    }

    private int[] insertBatch(Connection connection) {
        PreparedStatement statement;
        int[] counts =  null;
        try {
            statement = connection.prepareStatement(
                    "insert into t_basic (id, name, age, magic) values(?, ?, ?, ?)");
            for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
                statement.setInt(1, i);
                statement.setString(2, "Employee " + i);
                statement.setInt(3, i);
                statement.setInt(4, i);
                statement.addBatch();
            }
            counts = statement.executeBatch();
            statement.close();
            return counts;
        } catch (SQLException e) {
            throw new RuntimeException("insertBatch.executeBatch threw " + e.getMessage(), e);
        }
        
    }

    private void selectAndVerifyAll(Connection connection) throws SQLException {
        PreparedStatement selectStatement = connection.prepareStatement(
                "select id, name, age, magic from t_basic");
        ResultSet rs = selectStatement.executeQuery();
        Set<Integer> actuals = new HashSet<Integer>();
        while (rs.next()) {
            int id = rs.getInt(1);
            verifyEmployee(rs, id);
            actuals.add(id);
        }
        errorIfNotEqual("Wrong number of instances in database.", expecteds, actuals);
    }

    private void verifyEmployee(ResultSet rs, int id) {
        try {
            String name = rs.getString(2);
            errorIfNotEqual("Verify name id: " + id, "Employee " + id, name);
            int age = rs.getInt(3);
            errorIfNotEqual("Verify age id: " + id, id, age);
            int magic = rs.getInt(4);
            errorIfNotEqual("Verify magic id: " + id, id, magic);
        } catch (SQLException e) {
            if (getDebug()) e.printStackTrace();
        }
    }

}
