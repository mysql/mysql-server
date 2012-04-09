/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

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

import com.mysql.clusterj.Query;

import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.QueryBuilder;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import testsuite.clusterj.model.Employee;

public class AutoCommitTest extends AbstractClusterJModelTest {

    protected static final int NUMBER_TO_INSERT = 4;

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

    public void test() {
        nontransactionalMakePersistent(0);
        assertTransactionNotActive("nontransactionalMakePersistent(0)");
        nontransactionalFind(0);
        assertTransactionNotActive("nontransactionalFind(0)");
        nontransactionalQuery(0);
        assertTransactionNotActive("nontransactionalQuery(0)");
        nontransactionalUpdate(0, 9);
        assertTransactionNotActive("nontransactionalUpdate(0, 9)");
        nontransactionalMakePersistentAll(1, 4);
        assertTransactionNotActive("nontransactionalMakePersistentAll(1, 4)");
        nontransactionalUpdateAll(1, 3, 9);
        assertTransactionNotActive("nontransactionalUpdateAll(1, 3, 9)");
        nontransactionalQuery(0, 1, 2, 3);
        assertTransactionNotActive("nontransactionalQuery(0, 1, 2, 3");
        nontransactionalDeletePersistent(2);
        assertTransactionNotActive("nontransactionalDeletePersistent(2)");
        nontransactionalQuery(0, 1, 3);
        assertTransactionNotActive("nontransactionalQuery(0, 1, 3)");
        nontransactionalDeletePersistentAll(0, 1);
        assertTransactionNotActive("nontransactionalDeletePersistentAll(0, 1)");
        nontransactionalQuery(1, 3);
        assertTransactionNotActive("nontransactionalQuery(1, 3)");
        nontransactionalDeletePersistentAll();
        assertTransactionNotActive("nontransactionalDeletePersistentAll()");
        nontransactionalQuery();
        assertTransactionNotActive("nontransactionalQuery()");
        failOnError();
    }

    protected void nontransactionalDeletePersistent(int which) {
        session.deletePersistent(employees.get(which));
    }

    protected void nontransactionalDeletePersistentAll() {
        session.deletePersistentAll(Employee.class);
    }

    protected void nontransactionalDeletePersistentAll(int low, int high) {
        session.deletePersistentAll(employees.subList(low, high));
    }

    protected void nontransactionalMakePersistent(int which) {
        session.makePersistent(employees.get(which));
    }

    protected void nontransactionalMakePersistentAll(int low, int high) {
        List<Employee> emps = employees.subList(low, high);
        session.makePersistentAll(emps);
    }

    protected void nontransactionalQuery(int... expected) {
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);
        Query query = session.createQuery(dobj);
        List<Employee> result = query.getResultList();
        Set<Integer> expectedList = new HashSet<Integer>();
        for (int i: expected) {
            expectedList.add(i);
        }
        Set<Integer> actualList = new HashSet<Integer>();
        for (Employee e: result) {
            actualList.add(e.getId());
        }
        errorIfNotEqual("Mismatch in query result", expectedList, actualList);
    }

    protected void nontransactionalUpdate(int which, int newAge) {
        Employee e = session.find(Employee.class, which);
        e.setAge(newAge);
        session.updatePersistent(e);
        e = session.find(Employee.class, which);
        errorIfNotEqual("Mismatch in updateAll result", newAge, e.getAge());
    }

    protected void nontransactionalUpdateAll(int low, int high, int newAge) {
        List<Employee> emps = new ArrayList<Employee>();
        for (int i = low; i < high; ++i) {
            Employee e = session.find(Employee.class, i);
            e.setAge(newAge);
            emps.add(e);
        }
        session.updatePersistentAll(emps);
        for (int i = low; i < high; ++i) {
            Employee e = session.find(Employee.class, i);
            errorIfNotEqual("Mismatch in updateAll result", newAge, e.getAge());
        }
    }

    private void assertTransactionNotActive(String where) {
        if (tx.isActive()) {
            error("After " + where + " the transaction was active." );
        }
    }

    private void nontransactionalFind(int which) {
        Employee e = session.find(Employee.class, which);
    }
}
