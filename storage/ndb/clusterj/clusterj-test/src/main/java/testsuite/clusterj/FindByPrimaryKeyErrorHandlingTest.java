/*
   Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

import java.io.IOException;

import org.junit.DebugTest;

import com.mysql.clusterj.ClusterJDatastoreException;

import testsuite.clusterj.model.Employee;
import testsuite.clusterj.util.MgmClient;

@DebugTest("Test uses an error insert")
public class FindByPrimaryKeyErrorHandlingTest
        extends AbstractClusterJModelTest {

    private static final int NUMBER_TO_INSERT = 50;

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        try {
            session.deletePersistentAll(Employee.class);
        } catch (Throwable t) {
            // ignore - possibly no tuples to delete
        }
        createEmployeeInstances(NUMBER_TO_INSERT);
        session.makePersistentAll(employees);
        addTearDownClasses(Employee.class);
    }

    public void test() {
        testErrorHandling();
        failOnError();
    }

    private void testErrorHandling() {
        try (MgmClient mgmClient = new MgmClient(props)) {
            // Insert error to simulate a temporary error while reading
            if (!mgmClient.insertErrorOnAllDataNodes(5098)) {
                error("Failed to insert error on data nodes");
                // reset any partially inserted errors
                mgmClient.insertErrorOnAllDataNodes(0);
                return;
            }

            // Lookup for an existing row and expect it to throw error
            try {
                session.find(Employee.class, 5);
                // The previous find() call should have thrown an exception.
                // If it didn't, log it as an error.
                error("session.find() failed to throw a proper exception on temporary error");
            } catch (ClusterJDatastoreException cjde) {
                // Verify that the expected error has been caught
                verifyException("Simulating temporary read error in session.find()",
                        cjde, ".*Error code: 1,218.*");
            } catch (Exception ex) {
                // Any other exception caught is invalid
                error("Caught exception : " + ex.getMessage());
            }

            // Reset the error inserted to data nodes
            if (!mgmClient.insertErrorOnAllDataNodes(0)) {
                error("Failed to reset error on data nodes");
            }
        } catch (IOException e) {
            error("Failed to connect to Management Server. Caught exception : " + e.getMessage());
            return;
        }
    }
}
