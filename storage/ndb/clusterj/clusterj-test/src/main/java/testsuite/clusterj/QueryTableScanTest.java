/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.
   Use is subject to license terms.

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

import testsuite.clusterj.model.Employee;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;
import com.mysql.clusterj.Query;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class QueryTableScanTest extends AbstractClusterJModelTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();
        createEmployeeInstances(10);
        try {
            tx.begin();
            session.deletePersistentAll(Employee.class);
            tx.commit();
        } catch (Throwable t) {
            // ignore errors while deleting
        }
        tx.begin();
        session.makePersistentAll(employees);
        tx.commit();
        addTearDownClasses(Employee.class);
    }

    /** Test all queries using the same setup.
     * Fail if any errors during the tests.
     */
    public void testTableScan() {
        tableScanEqualQuery();
        tableScanGreaterEqualQuery();
        tableScanGreaterThanQuery();
        tableScanLessEqualQuery();
        tableScanLessThanQuery();
        failOnError();
    }
    public void tableScanEqualQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand param = dobj.param("name");
        // property name
        PredicateOperand column = dobj.get("name");
        // compare the column with the parameter
        Predicate compare = column.equal(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter value
        query.setParameter("name", "Employee number 8");
        // get the results
        List<Employee> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instance
        errorIfNotEqual("Wrong employee id returned from equal query: ",
                8, results.get(0).getId());
        tx.commit();
    }

    public void tableScanGreaterThanQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand param = dobj.param("name");
        // property name
        PredicateOperand column = dobj.get("name");
        // compare the column with the parameter
        Predicate compare = column.greaterThan(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("name", "Employee number 6");
        // get the results
        List<Employee> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Integer> expected = new HashSet<Integer>();
        expected.add(7);
        expected.add(8);
        expected.add(9);
        Set<Integer> actual = new HashSet<Integer>();
        for (Employee emp: results) {
            actual.add(emp.getId());
        }
        errorIfNotEqual("Wrong employee ids returned from greaterThan query: ",
                expected, actual);
        tx.commit();
    }

    public void tableScanGreaterEqualQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand param = dobj.param("name");
        // property name
        PredicateOperand column = dobj.get("name");
        // compare the column with the parameter
        Predicate compare = column.greaterEqual(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("name", "Employee number 7");
        // get the results
        List<Employee> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Integer> expected = new HashSet<Integer>();
        expected.add(7);
        expected.add(8);
        expected.add(9);
        Set<Integer> actual = new HashSet<Integer>();
        for (Employee emp: results) {
            actual.add(emp.getId());
        }
        errorIfNotEqual("Wrong employee ids returned from greaterEqual query: ",
                expected, actual);
        tx.commit();
    }

    public void tableScanLessThanQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand param = dobj.param("name");
        // property name
        PredicateOperand column = dobj.get("name");
        // compare the column with the parameter
        Predicate compare = column.lessThan(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("name", "Employee number 3");
        // get the results
        List<Employee> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Integer> expected = new HashSet<Integer>();
        expected.add(0);
        expected.add(1);
        expected.add(2);
        Set<Integer> actual = new HashSet<Integer>();
        for (Employee emp: results) {
            actual.add(emp.getId());
        }
        errorIfNotEqual("Wrong employee ids returned from lessThan query: ",
                expected, actual);
        tx.commit();
    }

    public void tableScanLessEqualQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand param = dobj.param("name");
        // property name
        PredicateOperand column = dobj.get("name");
        // compare the column with the parameter
        Predicate compare = column.lessEqual(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("name", "Employee number 2");
        // get the results
        List<Employee> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Integer> expected = new HashSet<Integer>();
        expected.add(0);
        expected.add(1);
        expected.add(2);
        Set<Integer> actual = new HashSet<Integer>();
        for (Employee emp: results) {
            actual.add(emp.getId());
        }
        errorIfNotEqual("Wrong employee ids returned from lessEqual query: ",
                expected, actual);
        tx.commit();
    }

}
