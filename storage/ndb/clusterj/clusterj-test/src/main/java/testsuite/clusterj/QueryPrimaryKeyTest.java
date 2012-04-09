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

import testsuite.clusterj.model.Employee;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;

import com.mysql.clusterj.Query;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class QueryPrimaryKeyTest extends AbstractClusterJModelTest {

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
    public void testPrimaryKey() {
        primaryKeyBetweenQuery();
        primaryKeyEqualQuery();
        primaryKeyGreaterEqualQuery();
        primaryKeyGreaterThanQuery();
        primaryKeyLessEqualQuery();
        primaryKeyLessThanQuery();
        failOnError();
    }
    public void primaryKeyEqualQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand param = dobj.param("id");
        // property name
        PredicateOperand column = dobj.get("id");
        // compare the column with the parameter
        Predicate compare = column.equal(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter value
        query.setParameter("id", 8);
        // get the results
        List<Employee> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instance
        errorIfNotEqual("Wrong employee id returned from query: ",
                8, results.get(0).getId());
        tx.commit();
    }

    public void primaryKeyGreaterThanQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand param = dobj.param("id");
        // property name
        PredicateOperand column = dobj.get("id");
        // compare the column with the parameter
        Predicate compare = column.greaterThan(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("id", 6);
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
        errorIfNotEqual("Wrong employee ids returned from query: ",
                expected, actual);
        tx.commit();
    }

    public void primaryKeyGreaterEqualQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand param = dobj.param("id");
        // property name
        PredicateOperand column = dobj.get("id");
        // compare the column with the parameter
        Predicate compare = column.greaterEqual(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("id", 7);
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
        errorIfNotEqual("Wrong employee ids returned from query: ",
                expected, actual);
        tx.commit();
    }

    public void primaryKeyLessThanQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand param = dobj.param("id");
        // property name
        PredicateOperand column = dobj.get("id");
        // compare the column with the parameter
        Predicate compare = column.lessThan(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("id", 3);
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
        errorIfNotEqual("Wrong employee ids returned from query: ",
                expected, actual);
        tx.commit();
    }

    public void primaryKeyLessEqualQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand param = dobj.param("id");
        // property name
        PredicateOperand column = dobj.get("id");
        // compare the column with the parameter
        Predicate compare = column.lessEqual(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("id", 2);
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
        errorIfNotEqual("Wrong employee ids returned from query: ",
                expected, actual);
        tx.commit();
    }

    public void primaryKeyBetweenQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Employee.class);

        // parameter name
        PredicateOperand lower = dobj.param("lower");
        // parameter name
        PredicateOperand upper = dobj.param("upper");
        // property name
        PredicateOperand column = dobj.get("id");
        // compare the column with the parameter
        Predicate compare = column.between(lower, upper);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("lower", 5);
        query.setParameter("upper", 7);
        // get the results
        List<Employee> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Integer> expected = new HashSet<Integer>();
        expected.add(5);
        expected.add(6);
        expected.add(7);
        Set<Integer> actual = new HashSet<Integer>();
        for (Employee emp: results) {
            actual.add(emp.getId());
        }
        errorIfNotEqual("Wrong employee ids returned from query: ",
                expected, actual);
        tx.commit();
    }
}
