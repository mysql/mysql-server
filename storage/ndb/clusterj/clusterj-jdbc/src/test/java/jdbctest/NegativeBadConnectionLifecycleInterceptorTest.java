/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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

package jdbctest;

import java.sql.SQLException;

import com.mysql.clusterj.ClusterJUserException;

import testsuite.clusterj.AbstractClusterJTest;

public class NegativeBadConnectionLifecycleInterceptorTest extends AbstractClusterJTest {
    public void test() {
        try {
            getConnection("target/test-classes/bad-connection-bad-connection-lifecycle-interceptor.properties");
            connection.prepareStatement("SELECT 1 FROM DUAL");
            fail("getConnection with a bad connection lifecycle interceptor should fail.");
        } catch (SQLException e) {
            // getConnection doesn't throw SQLException but wraps its exception in RuntimeException
        } catch (RuntimeException runtimeException) {
            Throwable sqlException = runtimeException.getCause();
            if (sqlException instanceof SQLException) {
                Throwable clusterJException = sqlException.getCause();
                if (clusterJException instanceof ClusterJUserException) {
                    if (clusterJException.getMessage().contains("BadConnectionLifecycleInterceptor")) {
                    // good catch
                    } else {
                        if (getDebug()) sqlException.printStackTrace();
                        fail("Exception should contain the string 'connectionLifecycleInterceptors'");
                    }
                } else {
                    if (getDebug()) sqlException.printStackTrace();
                    fail("Exception should be a ClusterJUserException wrapped in a SQLException.");
                }
            } else {
                if (getDebug()) sqlException.printStackTrace();
                fail("Exception should be a ClusterJUserException wrapped in a SQLException.");
            }
        }
    }

}
