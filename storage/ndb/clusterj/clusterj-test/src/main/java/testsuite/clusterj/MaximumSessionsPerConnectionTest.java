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

package testsuite.clusterj;

import java.util.Properties;

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.SessionFactory;


public class MaximumSessionsPerConnectionTest extends AbstractClusterJTest {

    @Override
    public void localSetUp() {
        loadProperties();
        // close the existing session factory because it uses one of the cluster connection (api) nodes
        if (sessionFactory != null) {
            sessionFactory.close();
            sessionFactory = null;
        }
    }

    public void test() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        SessionFactory sessionFactory1 = null;
        SessionFactory sessionFactory2 = null;

        // with maximum.sessions.per.connection set to 1 each session factory should be the same
        modifiedProperties.put(Constants.PROPERTY_MAXIMUM_SESSIONS_PER_CONNECTION, "1");
        sessionFactory1 = ClusterJHelper.getSessionFactory(modifiedProperties);
        sessionFactory2 = ClusterJHelper.getSessionFactory(modifiedProperties);
        sessionFactory1.close();
        sessionFactory2.close();
        errorIfNotEqual("SessionFactory1 should be the same object as SessionFactory2", true, 
            sessionFactory1 == sessionFactory2);

        // with maximum.sessions.per.connection set to 0 each session factory should be unique
        modifiedProperties.put(Constants.PROPERTY_MAXIMUM_SESSIONS_PER_CONNECTION, "0");
        sessionFactory1 = ClusterJHelper.getSessionFactory(modifiedProperties);
        sessionFactory2 = ClusterJHelper.getSessionFactory(modifiedProperties);
        sessionFactory1.close();
        sessionFactory2.close();
        errorIfNotEqual("SessionFactory1 should not be the same object as SessionFactory2", false, 
            sessionFactory1 == sessionFactory2);

        failOnError();
    }

}
