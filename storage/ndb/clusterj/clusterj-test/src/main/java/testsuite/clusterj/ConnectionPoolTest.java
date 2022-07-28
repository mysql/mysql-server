/*
 *  Copyright (c) 2011, 2022, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
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

import java.util.Arrays;
import java.util.List;
import java.util.Properties;

import testsuite.clusterj.model.Employee;

import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;

public class ConnectionPoolTest extends AbstractClusterJTest {

    @Override
    public boolean getDebug() {
        return false;
    }

    @Override
    public void localSetUp() {
        loadProperties();
        // close the existing session factory because it uses one of the cluster connection (api) nodes
        closeAllExistingSessionFactories();
    }

    public void testNoPooling() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        SessionFactory sessionFactory1 = null;
        SessionFactory sessionFactory2 = null;

        // with connection.pool.size set to 1 each session factory should be the same
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 1);
        sessionFactory1 = ClusterJHelper.getSessionFactory(modifiedProperties);
        sessionFactory2 = ClusterJHelper.getSessionFactory(modifiedProperties);
        sessionFactory1.close();
        sessionFactory2.close();
        errorIfNotEqual("With connection pooling, SessionFactory1 should be the same object as SessionFactory2",
                true, sessionFactory1 == sessionFactory2);

        // with connection.pool.size set to 0 each session factory should be unique
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 0);
        sessionFactory1 = ClusterJHelper.getSessionFactory(modifiedProperties);
        sessionFactory2 = ClusterJHelper.getSessionFactory(modifiedProperties);
        SessionFactory sessionFactory3 = null;
        String msg = "Creating SessionFactory with connection pool disabled but no free api nodes";
        try {
            // No more free API node ids.
            // Requesting another SessionFactory should throw an exception.
            modifiedProperties.put(Constants.PROPERTY_CLUSTER_CONNECT_RETRIES, 0);
            sessionFactory3 = ClusterJHelper.getSessionFactory(modifiedProperties);
        } catch (ClusterJFatalUserException ex) {
            // good catch
            verifyException(msg, ex, ".*No free node id found.*");
        }
        errorIfNotEqual(msg, null, sessionFactory3);
        sessionFactory1.close();
        sessionFactory2.close();
        errorIfNotEqual("With no connection pooling, SessionFactory1 should not be the same object as SessionFactory2",
                false, sessionFactory1 == sessionFactory2);

        // with connection.pool.size set to 0 and test node ids property
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 0);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_NODEIDS, "51,52");
        sessionFactory1 = null;
        msg = "Creating SessionFactory with connection pool disabled but with multiple node ids";
        try {
            sessionFactory1 = ClusterJHelper.getSessionFactory(modifiedProperties);
        } catch (ClusterJFatalUserException ex) {
            verifyException(msg, ex, "The nodeids property specifies multiple node ids .*");
        }
        errorIfNotEqual(msg, null, sessionFactory1);

        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_NODEIDS, "51");
        sessionFactory1 = ClusterJHelper.getSessionFactory(modifiedProperties);
        Session session1 = sessionFactory1.getSession();
        Employee e1 = session1.find(Employee.class, 0);
        checkSessions("testNoPooling after getSession", sessionFactory1, new Integer[] {1});
        sessionFactory1.close();

        failOnError();
    }

    public void testConnectionPoolSize() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 2);
        checkConnectionPoolSize2("testConnectionPoolSize", modifiedProperties);        
        failOnError();
    }

    public void testConnectionPoolSizeAndNodeIds() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 2);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_NODEIDS, "51;52");
        checkConnectionPoolSize2("testConnectionPoolSizeAndNodeIds", modifiedProperties);        
        failOnError();
    }

    public void testConnectionNodeIds() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_NODEIDS, "51,52");
        checkConnectionPoolSize2("testConnectionNodeIds", modifiedProperties);        
        failOnError();
    }

    public void testConnectionSingleNodeIdAndConnectionPoolSize() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 2);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_NODEIDS, "51");
        checkConnectionPoolSize2("testConnectionSingleNodeIdAndConnectionPoolSize", modifiedProperties);
        failOnError();
    }

    private void checkConnectionPoolSize2(String where, Properties modifiedProperties) {
        SessionFactory sessionFactory1 = null;
        SessionFactory sessionFactory2 = null;
        SessionFactory sessionFactory3 = null;
        sessionFactory1 = ClusterJHelper.getSessionFactory(modifiedProperties);
        sessionFactory2 = ClusterJHelper.getSessionFactory(modifiedProperties);
        sessionFactory3 = ClusterJHelper.getSessionFactory(modifiedProperties);
        errorIfNotEqual(where + " SessionFactory1 should be the same object as SessionFactory2", true, 
                sessionFactory1 == sessionFactory2);
        errorIfNotEqual(where + " SessionFactory1 should be the same object as SessionFactory3", true, 
                sessionFactory1 == sessionFactory3);
        Session session1 = sessionFactory1.getSession();
        Employee e1 = session1.find(Employee.class, 0);
        checkSessions(where + " after get session1", sessionFactory1, new Integer[] {1, 0});
        Session session2 = sessionFactory1.getSession();
        Employee e2 = session2.find(Employee.class, 0);
        checkSessions(where + " after get session2", sessionFactory1, new Integer[] {1, 1});
        Session session3 = sessionFactory1.getSession();
        checkSessions(where + " after get session3", sessionFactory1, new Integer[] {2, 1});
        Session session4 = sessionFactory1.getSession();
        checkSessions(where + " after get session4", sessionFactory1, new Integer[] {2, 2});
        Session session5 = sessionFactory1.getSession();
        checkSessions(where + " after get session5", sessionFactory1, new Integer[] {3, 2});
        Session session6 = sessionFactory1.getSession();
        checkSessions(where + " after get session6", sessionFactory1, new Integer[] {3, 3});

        session1.close();
        checkSessions(where + " after close session1", sessionFactory1, new Integer[] {2, 3});
        session4.close();
        checkSessions(where + " after close session4", sessionFactory1, new Integer[] {2, 2});
        session5.close();
        checkSessions(where + " after close session5", sessionFactory1, new Integer[] {1, 2});
        Session session7 = sessionFactory1.getSession();
        checkSessions(where + " after get session7", sessionFactory1, new Integer[] {2, 2});
        
        session2.close();
        session3.close();
        session6.close();
        session7.close();
        sessionFactory1.close();
    }

    public void testNegativeMismatchConnectionPoolSizeAndConnectionPoolNodeids() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 3);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_NODEIDS, "4\t5");
        try {
            ClusterJHelper.getSessionFactory(modifiedProperties);
        } catch (ClusterJFatalUserException ex) {
            if (getDebug()) ex.printStackTrace();
            // good catch
            String expected = "4\t5";
            if (!ex.getMessage().contains(expected)) {
                error("Mismatch error message should contain " + expected);
            }
        }
        failOnError();
    }

    public void testNegativeConnectionPoolNodeidsFormatError() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 2);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_NODEIDS, "7 t");
        try {
            ClusterJHelper.getSessionFactory(modifiedProperties);
        } catch (ClusterJFatalUserException ex) {
            if (getDebug()) ex.printStackTrace();
            // good catch
            String expected = "NumberFormatException";
            if (!ex.getMessage().contains(expected)) {
                error("Mismatch error message '" + ex.getMessage() + "' should contain '" + expected + '"');
            }
        }
        failOnError();
    }

    public void testNegativeConnectionPoolIllegalNodeids() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CLUSTER_CONNECT_RETRIES, 0);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_NODEIDS, "256");
        try {
            ClusterJHelper.getSessionFactory(modifiedProperties);
        } catch (ClusterJFatalUserException ex) {
            if (getDebug()) ex.printStackTrace();
            // good catch
            String expected = "illegal";
            if (!ex.getMessage().contains(expected)) {
                error("Mismatch error message '" + ex.getMessage() + "' should contain '" + expected + '"');
            }
        }
        failOnError();
    }

    public void testNegativeConnectionPoolNoNodeId() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CLUSTER_CONNECT_RETRIES, 0);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_NODEIDS, "48");
        try {
            ClusterJHelper.getSessionFactory(modifiedProperties);
        } catch (ClusterJFatalUserException ex) {
            if (getDebug()) ex.printStackTrace();
            // good catch
            String expected = "No node defined";
            if (!ex.getMessage().contains(expected)) {
                error("Mismatch error message '" + ex.getMessage() + "' should contain '" + expected + '"');
            }
        }
        failOnError();
    }

    private void checkSessions(String where, SessionFactory sessionFactory, Integer[] expected) {
        List<Integer> connectionCounts = sessionFactory.getConnectionPoolSessionCounts();
        if (getDebug()) System.out.println("connection counts: " + connectionCounts.toString());
        if (expected.length != connectionCounts.size()) {
            error(where + " wrong number of connections in pool\n"
                    + "Expected: " + Arrays.toString(expected)
                    + " Actual: " + connectionCounts);
            return;
        }
        int i = 0;
        for (Integer connectionCount: connectionCounts) {
            if (getDebug()) System.out.println("Connection " + i + " has " + connectionCount + " sessions.");
            if (i >= expected.length) break;
            errorIfNotEqual(where + " wrong count on connection " + i, expected[i], connectionCount);
            i++;
        }
        if (getDebug()) System.out.println();
    }
}
