/*
 *  Copyright (c) 2010, 2024, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package testsuite.clusterj;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Types;

import org.junit.Ignore;

/** Test that mysql session variable ndb_coordinated_transaction_id can be
 * read and written by jdbc.
 */
@Ignore
public class CoordinatedTransactionIdVariableTest extends AbstractClusterJTest {

    /** Format is Uint32:Uint32:Uint32:Uint64 */
    private String newId = "1:0:1:9000000000000099";
    private String badIdTooLong = "123456789012345678901234567890123456789012345";
    private String badIdTooShort = "1:1";
    private String sqlQuery = "select id from t_basic where id = 0";
    private String transactionIdVariableName = "@@ndb_transaction_id";
    private String joinTransactionIdVariableName = "@@ndb_join_transaction_id";

    @Override
    protected void localSetUp() {
        createSessionFactory();
        closeConnection();
        getConnection();
        setAutoCommit(connection, false);
    }

    @Override
    protected boolean getDebug() {
        return false;
    }

    /** Verify that the initial value of the variable transaction_id is null.
     */
    public void checkTransactionIdInitialValue() {
        getConnection();
        String id = getJDBCTransactionId("checkTransactionIdInitialValue");
        errorIfNotEqual("Transaction id must default to null.", null, id);
    }

    /** Verify that the initial value of the variable join_transaction_id is null.
     */
    public void checkJoinTransactionIdInitialValue() {
        getConnection();
        String id = getJDBCJoinTransactionId("checkJoinTransactionIdInitialValue");
        errorIfNotEqual("Join transaction id must default to null.", null, id);
    }

    /** Verify that you cannot set the value of the variable transaction_id.
     */
    public void checkSetTransactionId() {
        getConnection();
        setAutoCommit(connection, false);
        // try set the transaction_id to a good value; expect error 1238 "read only variable"
        setJDBCVariable("checkSetTransactionId", transactionIdVariableName, newId, 1238);
        // close the connection so the value isn't accidentally used by a new transaction
        closeConnection();
    }

    /** Try to set the join_transaction_id variable to a good value
     * and verify that it can be read back. A null string is used to reset the value.
     */
    public void checkNewIdResetWithNullString() {
        getConnection();
        setAutoCommit(connection, false);
        // set the coordinated_transaction_id to some value
        setJDBCVariable("checkNewIdResetWithNullString", joinTransactionIdVariableName, newId, 0);
        String id = getJDBCJoinTransactionId("checkNewIdResetWithNullString");
        errorIfNotEqual("failed to set coordinated transaction id.", newId, id);
        setJDBCVariable("checkNewIdResetWithNullString", joinTransactionIdVariableName, null, 0);
        id = getJDBCJoinTransactionId("checkNewIdResetWithNullString");
        errorIfNotEqual("failed to set coordinated transaction id to null.", null, id);
        // close the connection so the value isn't accidentally used by a new transaction
        closeConnection();
    }

    /** Try to set the join_transaction_id variable to a good value
     * and verify that it can be read back. An empty string is used to reset the value.
     */
    public void checkNewIdResetWithEmptyString() {
        getConnection();
        setAutoCommit(connection, false);
        // set the coordinated_transaction_id to some value
        setJDBCVariable("checkNewIdResetWithEmptyString", joinTransactionIdVariableName, newId, 0);
        String id = getJDBCJoinTransactionId("checkNewIdResetWithEmptyString");
        errorIfNotEqual("failed to set coordinated transaction id.", newId, id);
        setJDBCVariable("checkNewIdResetWithEmptyString", joinTransactionIdVariableName, "", 0);
        id = getJDBCJoinTransactionId("checkNewIdResetWithEmptyString");
        errorIfNotEqual("failed to set coordinated transaction id to null.", null, id);
        // close the connection so the value isn't accidentally used by a new transaction
        closeConnection();
    }

    /** Try to set the join_transaction_id variable to a bad value
     * and verify that an exception is thrown.
     */
    public void checkBadIdTooLong() {
        getConnection();
        setAutoCommit(connection, false);
        // set the join_transaction_id to a bad value and expect 1210 "Incorrect arguments to SET"
        setJDBCVariable("checkBadIdTooLong", joinTransactionIdVariableName, badIdTooLong, 1210);
        String id = getJDBCJoinTransactionId("checkBadIdTooLong");
        errorIfNotEqual("failed to set coordinated transaction id.", null, id);
        // close the connection so the value isn't accidentally used by a new transaction
        closeConnection();
    }

    /** Try to set the join_transaction_id variable to a bad value
     * and verify that an exception is thrown.
     */
    public void checkBadIdTooShort() {
        getConnection();
        setAutoCommit(connection, false);
        // set the join_transaction_id to a bad value and expect 1210 "Incorrect arguments to SET"
        setJDBCVariable("checkBadIdTooShort", joinTransactionIdVariableName, badIdTooShort, 1210);
        String id = getJDBCJoinTransactionId("checkBadIdTooShort");
        errorIfNotEqual("failed to set coordinated transaction id.", null, id);
        // close the connection so the value isn't accidentally used by a new transaction
        closeConnection();
    }

    /** Verify that after an ndb transaction is started the transaction id is not null
     * and is null after commit.
     */
    public void checkIdAfterTransactionStartAndCommit() {
        getConnection();
        setAutoCommit(connection, false);
       // execute a query statement that will cause the server to start an ndb transaction
        executeJDBCQuery("checkIdAfterTransactionStartAndCommit");
        // the coordinated transaction id should now be available
        String id = getJDBCTransactionId("checkIdAfterTransactionStartAndCommit");
        // we can only test for not null since we cannot predict the transaction id
        errorIfEqual("Transaction id must not be null after transaction start.", null, id);
        commitConnection();
        id = getJDBCTransactionId("checkIdAfterTransactionStartAndCommit");
        errorIfNotEqual("Transaction id must be null after commit.", null, id);
        }

    /** Verify that after an ndb transaction is started the coordinated transaction id is not null
     * and is null after rollback.
     */
    public void checkIdAfterTransactionStartAndRollback() {
        getConnection();
        setAutoCommit(connection, false);
        // execute a query statement that will cause the server to start an ndb transaction
        executeJDBCQuery("checkIdAfterTransactionStartAndRollback");
        // the coordinated transaction id should now be available
        String id = getJDBCTransactionId("checkIdAfterTransactionStartAndRollback");
        // we can only test for not null since we cannot predict the transaction id
        errorIfEqual("Transaction must not be null after transaction start.", null, id);
        rollbackConnection();
        id = getJDBCTransactionId("checkIdAfterTransactionStartAndRollback");
        errorIfNotEqual("Transaction id must be null after rollback.", null, id);
        }

    /** Execute a SQL query. Throw away the results. Keep the transaction open.
     */
    protected void executeJDBCQuery(String where) {
        PreparedStatement statement = null;
        ResultSet rs = null;
        try {
            statement = connection.prepareStatement(sqlQuery);
            rs = statement.executeQuery();
            boolean hasNext = rs.next();
            if (getDebug()) System.out.println(where + " executeJDBCQuery rs.next() returned " + hasNext);
        } catch (SQLException e) {
            error(where + " query threw exception ", e);
        } finally {
            if (rs != null) {
                try {
                    rs.close();
                } catch (SQLException e) {
                    error(where + " rs.close threw exception " + e.getMessage());
                }
            }
            if (statement != null) {
                try {
                    statement.close();
                } catch (SQLException e) {
                    error(where + " statement.close threw exception ", e);
                }
            }
        }
    }

    /** Set the variable in the server.
     * @param where the context
     * @param variableName the name of the server variable
     * @param newId the id to set
     */
    protected void setJDBCVariable(String where, String variableName, String newId, int expectedErrorCode) {
        try {
            String setSql = "set " + variableName + " = ?";
            PreparedStatement setVariableStatement = connection.prepareStatement(setSql);
            if (newId == null) {
                setVariableStatement.setNull(1, Types.VARCHAR);
            } else {
                setVariableStatement.setString(1, newId);
            }
            boolean resultNotUpdateCount = setVariableStatement.execute();
            errorIfNotEqual(where + " set join transaction id returned true.", false, resultNotUpdateCount);
            errorIfNotEqual(where + " set join transaction id failed to throw expected exception " +
                    expectedErrorCode + " for " + newId, expectedErrorCode, 0);
        } catch (SQLException e) {
            int errorCode = e.getErrorCode();
            errorIfNotEqual(where + " caught wrong exception on set coordinated transaction id:" +
                    " errorCode: " + errorCode + " SQLState: " + e.getSQLState(), expectedErrorCode, errorCode);
        }
    }

    /** Get the join_transaction_id variable from the server.
     * @return the id from the server
     */
    protected String getJDBCJoinTransactionId(String where) {
        String getId = "select " + joinTransactionIdVariableName;
        String result = executeSelect(where, getId);
        return result;
    }

    /** Get the join_transaction_id variable from the server.
     * @return the id from the server
     */
    protected String getJDBCTransactionId(String where) {
        String getId = "select " + transactionIdVariableName;
        String result = executeSelect(where, getId);
        return result;
    }

    private String executeSelect(String where, String getId) {
        String result = null;
        try {
            PreparedStatement getCoordinatedTransactionIdStatement = connection.prepareStatement(getId);
            ResultSet rs = getCoordinatedTransactionIdStatement.executeQuery();
            boolean hasResult = rs.next();
            errorIfNotEqual(where + " select " + getId + " returned false.", true, hasResult);
            result = rs.getString(1);
            if (getDebug()) System.out.println(where + " " + getId + " returns " + result);
        } catch (SQLException e) {
            error(where + " caught exception on select " + getId + ".", e);
        }
        return result;
    }

    /** Commit the connection to clean it up for the next use.
     */
    protected void commitConnection() {
        try {
            connection.commit();
        } catch (SQLException e) {
            error("connection.commit threw exception: ", e);
        }
    }

    /** Roll back the connection to clean it up for the next use.
     */
    protected void rollbackConnection() {
        try {
            connection.rollback();
        } catch (SQLException e) {
            error("connection.rollback threw exception: ", e);
        }
    }

}
