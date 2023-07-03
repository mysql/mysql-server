/*
   Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

package testsuite.clusterj;

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;
import testsuite.clusterj.model.AutoPKInt;

import java.util.Properties;

public class SessionFactoryTest extends AbstractClusterJTest {

    @Override
    protected void localSetUp() {
        // close any existing session factory
        closeAllExistingSessionFactories();
    }

    /**
     * Test that the DomainTypeHandler are properly cleared when the SessionFactory
     * and the related connections are closed.
     *   a) Create two unique SessionFactories by disabling connection pool. Both will start
     *      separate new connections to the data nodes.
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
        SessionFactory sessionFactory1 = ClusterJHelper.getSessionFactory(modifiedProperties);
        SessionFactory sessionFactory2 = ClusterJHelper.getSessionFactory(modifiedProperties);

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
            sessionFactory2.close();

        } catch (Exception ex) {
            ex.printStackTrace();
            // close the session factories and fail the test
            if (sessionFactory1.currentState() != SessionFactory.State.Closed) {
                sessionFactory1.close();
            }
            if (sessionFactory2.currentState() != SessionFactory.State.Closed) {
                sessionFactory2.close();
            }
            fail(ex.getMessage());
        }
    }
}
