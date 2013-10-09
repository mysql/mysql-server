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

import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

import com.mysql.clusterj.Constants;

import testsuite.clusterj.model.Employee2;

public class FindByPrimaryKey2Test extends AbstractClusterJModelTest {

    protected List<Employee2> employees2;

    private static final int NUMBER_TO_INSERT = 1;

    @Override
    protected Properties modifyProperties() {
        Properties modifiedProperties = new Properties();
        modifiedProperties.putAll(props);
        modifiedProperties.put(Constants.PROPERTY_CLUSTER_DATABASE, "test2");
        return modifiedProperties;
    }

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createEmployee2Instances(NUMBER_TO_INSERT);
        tx = session.currentTransaction();
        tx.begin();
        session.deletePersistentAll(Employee2.class);
        tx.commit();
        addTearDownClasses(Employee2.class);
    }

    public void testFind() {
        // first, create instances to find
        tx = session.currentTransaction();
        tx.begin();
        
        int count = 0;

        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            session.makePersistent(employees2.get(i));
            ++count;
        }
        tx.commit();

        tx.begin();
        
        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            Employee2 e = session.find(Employee2.class, i);
            // see if it is the right Employee
            int actualId = e.getId();
            if (actualId != i) {
                error("Expected Employee2.id " + i + " but got " + actualId);
            }
        }
        tx.commit();
        failOnError();
    }

    protected void createEmployee2Instances(int count) {
        employees2 = new ArrayList<Employee2>(count);
        for (int i = 0; i < count; ++i) {
            Employee2 emp = session.newInstance(Employee2.class);
            emp.setId(i);
            emp.setName("Employee number " + i);
            emp.setAge(i);
            emp.setMagic(i);
            employees2.add(emp);
        }
    }

}
