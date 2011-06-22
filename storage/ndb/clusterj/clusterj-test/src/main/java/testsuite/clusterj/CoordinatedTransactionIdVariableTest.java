/*
 *  Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

package testsuite.clusterj;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import org.junit.Ignore;

/** Test that mysql session variable ndb_coordinated_transaction_id can be
 * read and written by jdbc.
 */
@Ignore
public class CoordinatedTransactionIdVariableTest extends AbstractClusterJTest {

    /** Format is Uint32+Uint32:Uint64 */
    private String newId = "1+1:9000000000000099";
    private String sqlQuery = "select id from t_basic where id = 0";
    private String getTransactionIdVariableName = "@@ndb_transaction_id";
    private String setTransactionIdVariableName = "@@ndb_join_transaction_id";

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

    /** Verify that the initial value of the variable ndb_coordinated_transaction_id is null.
     */
    public void checkInitialValue() {
        getConnection();
        String id = getJDBCCoordinatedTransactionId("checkInitialValue");
        errorIfNotEqual("Coordinated transaction id must default to null.", null, id);
    }

    /** Try to set the ndb_coordinated_transaction_id variable to a new value
     * and verify that it can be read back.
     */
    public void checkNewValue() {
        getConnection();
        // set the coordinated_transaction_id to some random value
        setJDBCCoordinatedTransactionId("checkNewValue", newId);
        String id = getJDBCCoordinatedTransactionId("checkNewValue");
        errorIfNotEqual("failed to set coordinated transaction id.", newId, id);
        executeJDBCQuery("checkNewValue");
        // close the connection so the value isn't accidentally used by a new transaction
        closeConnection();
    }

    /** Verify that after an ndb transaction is started the coordinated transaction id is not null
     * and is null after commit.
     */
    public void checkIdAfterTransactionStartAndCommit() {
        getConnection();
        // execute a query statement that will cause the server to start an ndb transaction
        executeJDBCQuery("checkIdAfterTransactionStartAndCommit");
        // the coordinated transaction id should now be available
        String id = getJDBCCoordinatedTransactionId("checkIdAfterTransactionStartAndCommit");
        // we can only test for not null since we cannot predict the transaction id
        errorIfEqual("Coordinated transaction must not be null after transaction start", null, id);
        commitConnection();
        id = getJDBCCoordinatedTransactionId("checkIdAfterTransactionStartAndCommit");
        errorIfNotEqual("Coordinated transaction id must be null after commit.", null, id);
        }

    /** Verify that after an ndb transaction is started the coordinated transaction id is not null
     * and is null after rollback.
     */
    public void checkIdAfterTransactionStartAndRollback() {
        getConnection();
        // execute a query statement that will cause the server to start an ndb transaction
        executeJDBCQuery("checkIdAfterTransactionStartAndRollback");
        // the coordinated transaction id should now be available
        String id = getJDBCCoordinatedTransactionId("checkIdAfterTransactionStartAndRollback");
        // we can only test for not null since we cannot predict the transaction id
        errorIfEqual("Coordinated transaction must not be null after transaction start", null, id);
        rollbackConnection();
        id = getJDBCCoordinatedTransactionId("checkIdAfterTransactionStartAndRollback");
        errorIfNotEqual("Coordinated transaction id must be null after rollback.", null, id);
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

    /** Set the coordinated_transaction_id variable in the server.
     * @param newId the id to set
     */
    protected void setJDBCCoordinatedTransactionId(String where, String newId) {
        if (newId == null) {
            fail(where + " test case error: coordinated transaction id must not be null.");
        }
        try {
            String setSql = "set " + setTransactionIdVariableName + " = '" + newId + "'";
            PreparedStatement setCoordinatedTransactionIdStatement = connection.prepareStatement(setSql);
            boolean result = setCoordinatedTransactionIdStatement.execute();
            errorIfNotEqual(where + " set coordinated transaction id returned true.", false, result);
        } catch (SQLException e) {
            error(where + " caught exception on set coordinated transaction id:", e);
        }
    }

    /** Get the coordinated_transaction_id variable from the server.
     * @return the id from the server
     */
    protected String getJDBCCoordinatedTransactionId(String where) {
        String getId = "select " + getTransactionIdVariableName;
        String result = null;
        try {
            PreparedStatement getCoordinatedTransactionIdStatement = connection.prepareStatement(getId);
            ResultSet rs = getCoordinatedTransactionIdStatement.executeQuery();
            boolean hasResult = rs.next();
            errorIfNotEqual(where + " select coordinated transaction id returned false.", true, hasResult);
            result = rs.getString(1);
            if (getDebug()) System.out.println(where + " getJDBCCoordinatedTransactionId returns " + result);
        } catch (SQLException e) {
            error(where + " caught exception on get coordinated transaction id.", e);
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
