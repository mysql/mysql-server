/*
   Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

package testsuite.clusterj;

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;
import testsuite.clusterj.model.AutoPKInt;

import testsuite.clusterj.model.Employee;
import testsuite.clusterj.model.Employee2;

import java.util.Properties;

public class SessionFactoryTest extends AbstractClusterJTest {

    @Override
    protected void localSetUp() {
        createSessionFactory();
    }

    /* Test that an attempt to get a SessionFactory with multidb true
     * and connection pool size 0 results in a ClusterJUserException
     */
    public void testMultiDbNoPooling() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 0);
        modifiedProperties.put(Constants.PROPERTY_CLUSTER_MULTI_DB, "true");

        String msg = "Creating MultiDb SessionFactory with pooling disabled";
        SessionFactory factory = null;
        try {
            factory = ClusterJHelper.getSessionFactory(modifiedProperties);
        } catch (ClusterJFatalUserException e) {
            verifyException(msg, e, "Cannot create SessionFactory with multi.databases=true.*");
        }
        if(factory != null) factory.close();
        errorIfNotEqual(msg, null, factory);
        failOnError();
    }

    /* If PROPERTY_CLUSTER_MULTI_DB is not set, getSession(name) will fail,
       unless name is the name of the default database, or name is null.
       This can be tested using the default factory.
    */
    public void testGetSessionWithNamedDb() {
        SessionFactory factory = ClusterJHelper.getSessionFactory(props);
        Session session = null;
        try {
            session = factory.getSession("test2");
        } catch(ClusterJUserException e) {
            verifyException("testGetSessionWithNamedDb", e, ".*does not support multiple databases.");
        }
        errorIfNotEqual("getSession() should fail", null, session);

        try {
            session = factory.getSession("test");
         } catch (ClusterJUserException e) {
            error("getSession() with named default database should succeed");
        }
        errorIfEqual("testGetSessionWithNamedDb", null, session);
        if(session != null) session.close();

        String dbName = null;
         try {
            session = factory.getSession("test");
        } catch (ClusterJUserException e) {
            error("getSession() with null database name should succeed");
        }
        errorIfEqual("testGetSessionWithNamedDb", null, session);
        if(session != null) session.close();
        failOnError();
    }

    /* Some basic MultiDB tests
     */
    public void testMultiDb() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CLUSTER_MULTI_DB, "true");
        String msg;

        msg = "Getting MultiDbSessionFactory";
        SessionFactory factory = ClusterJHelper.getSessionFactory(modifiedProperties);
        errorIfEqual(msg, null, factory);

        msg = "Getting session from MultiDbSessionFactory";
        Session s1 = factory.getSession();
        errorIfEqual(msg, null, s1);

        msg = "Getting session from MultiDbSessionFactory with named database";
        Session s2 = factory.getSession("test2");
        errorIfEqual(msg, null, s2);

        msg = "Two sessions should use the same connection";
        errorIfNotEqual(msg, s1.getConnection().nodeId(), s2.getConnection().nodeId());

        msg = "Checking Db count (sessions open)";
        errorIfNotEqual(msg, 2, factory.getConnectionPoolSessionCounts().get(0));

        msg = "Using session1";
        Employee em1 = s1.find(Employee.class, 1);
        s1.close();
        errorIfNotEqual(msg, null, em1);

        msg = "Using session2";
        Employee2 emp = s2.newInstance(Employee2.class);
        emp.setId(1);
        emp.setName("Python");
        emp.setAge(40);
        emp.setMagic(50);
        s2.savePersistent(emp);
        s2.release(emp);
        Employee2 emp2 = s2.find(Employee2.class, 1);
        errorIfNotEqual(msg, emp2.getAge(), 40);
        s2.remove(emp2);
        s2.close();

        msg = "Checking Db count (sessions closed)";
        errorIfNotEqual(msg, 0, factory.getConnectionPoolSessionCounts().get(0));

        factory.close();
        failOnError();
    }


    /**
     * Test that the DomainTypeHandler are properly cleared when the SessionFactory
     * and the related connections are closed.
     *   a) Create a new unique SessionFactory by disabling connection pool. This
     *      factory and the exitsing factory will have separate new connections
     *      to the data nodes.
     *   b) Insert into autopkint table using both SessionFactories and
     *      verify that the underlying DomainTypeHandler are not shared.
     *   c) Delete all tuples from autopkint table.
     *   d) Close the sessions and session factories
     */
    public void testDomainTypeHandlerLifetime() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);

        // Disable connection pool and create two separate session factories
        modifiedProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 0);
        modifiedProperties.put(Constants.PROPERTY_CLUSTER_MULTI_DB, "false");
        SessionFactory sessionFactory1 = ClusterJHelper.getSessionFactory(modifiedProperties);
        SessionFactory sessionFactory2 = sessionFactory;

        try {
            // Write a row into AutoPKInt using sessionFactory1 and then close it
            Session session1 = sessionFactory1.getSession();
            AutoPKInt obj1 = session1.newInstance(AutoPKInt.class);
            obj1.setVal(10);
            session1.makePersistent(obj1);
            session1.close();
            sessionFactory1.close();

            // Write another row using sessionFactory2 and delete all the rows
            Session session2 = sessionFactory2.getSession();
            AutoPKInt obj2 = session2.newInstance(AutoPKInt.class);
            obj2.setVal(20);
            // Make persistent will use the underlying DomainTypeHandler and the NdbTable ref
            // pointed by it to retrieve the auto inc value for the obj2 row id.
            // A successful call to makePersistent would imply that the underlying NdbTable ref
            // is still valid and was not affected by the close of sessionFactory1.
            session2.makePersistent(obj2);
            session2.deletePersistentAll(AutoPKInt.class);
            session2.close();

        } catch (Exception ex) {
            ex.printStackTrace();
            // close the session factories and fail the test
            if (sessionFactory1.currentState() != SessionFactory.State.Closed) {
                sessionFactory1.close();
            }
            fail(ex.getMessage());
        }
    }
}
