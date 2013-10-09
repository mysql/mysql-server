/*
 Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

package testsuite.clusterj;

import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.core.store.ClusterConnectionService;
public class NegativeClusterConnectionServicePropertyTest extends AbstractClusterJCoreTest {

    public void testBadClassName() {
        try {
            ClusterJHelper.getServiceInstance(ClusterConnectionService.class, "some.random.name");
            fail("Expected ClassNotFoundException, got no exception");
        } catch (ClusterJFatalUserException e) {
            // make sure the enclosed exception is ClassNotFoundException
            Throwable cause = e.getCause();
            if (!(cause instanceof ClassNotFoundException)) {
                fail("Expected ClassNotFoundException, got " + cause.getClass() + " message: " + e.getMessage());
            }
        }
    }

    public void testClassNotClusterConnectionService() {
        try {
            ClusterJHelper.getServiceInstance(ClusterConnectionService.class, "testsuite.clusterj.util.DoesNotImplementClusterConnectionService");
            fail("Expected ClassCastException, got no exception");
        } catch (ClusterJFatalUserException e) {
            // make sure the enclosed exception is ClassCastException
            Throwable cause = e.getCause();
            if (!(cause instanceof ClassCastException)) {
                fail("Expected ClassCastException, got " + cause.getClass() + " message: " + e.getMessage());
            }
        }
    }

    public void testNotPublicConstructorClusterConnectionService() {
        try {
            ClusterJHelper.getServiceInstance(ClusterConnectionService.class, "testsuite.clusterj.util.NoPublicConstructorClusterConnectionService");
            fail("Expected IllegalAccessException, got no exception");
        } catch (ClusterJFatalUserException e) {
            // make sure the enclosed exception is IllegalAccessException
            Throwable cause = e.getCause();
            if (!(cause instanceof IllegalAccessException)) {
                fail("Expected IllegalAccessException, got " + cause.getClass() + " message: " + e.getMessage());
            }
        }
    }

}
