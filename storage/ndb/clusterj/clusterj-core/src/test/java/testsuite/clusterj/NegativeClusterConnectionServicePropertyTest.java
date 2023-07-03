/*
 Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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
            // make sure the enclosed exception is NoSuchMethodException
            Throwable cause = e.getCause();
            if (!(cause instanceof NoSuchMethodException)) {
                fail("Expected NoSuchMethodException, got " + cause.getClass() + " message: " + e.getMessage());
            }
        }
    }

}
