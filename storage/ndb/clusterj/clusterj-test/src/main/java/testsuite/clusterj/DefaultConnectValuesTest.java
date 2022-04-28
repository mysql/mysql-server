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

import java.util.Properties;

import com.mysql.clusterj.Constants;

import testsuite.clusterj.model.Employee;

public class DefaultConnectValuesTest extends AbstractClusterJModelTest {

    private static final int NUMBER_TO_INSERT = 1;

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createEmployeeInstances(NUMBER_TO_INSERT);
        tx = session.currentTransaction();
        tx.begin();
        session.deletePersistentAll(Employee.class);
        tx.commit();
        addTearDownClasses(Employee.class);
    }

    /** Remove the properties that can be defaulted.
     */
    @Override
    protected Properties modifyProperties() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.remove(Constants.PROPERTY_CLUSTER_CONNECT_DELAY);
        modifiedProperties.remove(Constants.PROPERTY_CLUSTER_CONNECT_RETRIES);
        modifiedProperties.remove(Constants.PROPERTY_CLUSTER_CONNECT_TIMEOUT_AFTER);
        modifiedProperties.remove(Constants.PROPERTY_CLUSTER_CONNECT_TIMEOUT_BEFORE);
        modifiedProperties.remove(Constants.PROPERTY_CLUSTER_CONNECT_VERBOSE);
        modifiedProperties.remove(Constants.PROPERTY_CLUSTER_DATABASE);
        modifiedProperties.remove(Constants.PROPERTY_CLUSTER_MAX_TRANSACTIONS);
        return modifiedProperties;
    }

    public void testFind() {
        // first, create instances to find
        tx = session.currentTransaction();
        tx.begin();
        
        int count = 0;

        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            session.makePersistent(employees.get(i));
            ++count;
        }
        tx.commit();

        tx.begin();
        
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            Employee e = session.find(Employee.class, i);
            // make sure all fields were fetched
            consistencyCheck(e);
            // see if it is the right Employee
            int actualId = e.getId();
            if (actualId != i) {
                error("Expected Employee.id " + i + " but got " + actualId);
            }
        }
        tx.commit();
        failOnError();
    }
}
