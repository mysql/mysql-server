/*
   Copyright (C) 2009 Sun Microsystems Inc.
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
import com.mysql.clusterj.Session;

import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import testsuite.clusterj.model.IdBase;

abstract public class AbstractQueryTest extends AbstractClusterJModelTest {

    /** The class of instances, supplied by a subclass. */
    Class<?> instanceType = getInstanceType();

    /**
     * Create instances required by the test, used for the queries.
     * @param number the number of instances to create
     */
    abstract void createInstances(int number);

    /**
     * Return the type of instances used for the queries.
     * @return the type of instances for the test
     */
    abstract Class<?> getInstanceType();

    /** Clean up instances after test. Can be overridden by subclasses.
     * 
     */
    protected boolean cleanupAfterTest = getCleanupAfterTest();

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();
        createInstances(10);
        try {
            tx.begin();
            session.deletePersistentAll(instanceType);
            tx.commit();
        } catch (Throwable t) {
            // ignore errors while deleting
        }
        tx.begin();
        session.makePersistentAll(instances);
        tx.commit();
        if (cleanupAfterTest)
            addTearDownClasses(instanceType);
    }

    protected boolean getCleanupAfterTest() {
        return true;
    }

    class QueryHolder {
        public QueryBuilder builder;
        public QueryDomainType dobj;
        public String propertyName;
        public PredicateOperand propertyPredicate;
        public PredicateOperand paramEqualPredicate;
        public PredicateOperand paramLowerPredicate;
        public PredicateOperand paramUpperPredicate;
        public Predicate equal;
        public Predicate greaterThan;
        public Predicate greaterEqual;
        public Predicate lessThan;
        public Predicate lessEqual;
        public Predicate between;
        public Predicate greaterThanAndLessThan;
        public Predicate greaterEqualAndLessThan;
        public Predicate greaterThanAndLessEqual;
        public Predicate greaterEqualAndLessEqual;
        public Query query;
        public Set<Integer> expectedSet = new HashSet<Integer>();
        public String expectedIndex;
        public QueryHolder(Class type, String propertyName, String expectedIndex) {
            this.propertyName = propertyName;
            // QueryBuilder is the sessionFactory for queries
            builder = session.getQueryBuilder();
            // QueryDomainType is the main interface
            dobj = builder.createQueryDefinition(type);
            this.expectedIndex = expectedIndex;
            // parameter
            paramEqualPredicate = dobj.param("equal");
            paramLowerPredicate = dobj.param("lower");
            paramUpperPredicate = dobj.param("upper");
            // property
            propertyPredicate = dobj.get(propertyName);
            // comparison operations
            equal = propertyPredicate.equal(paramEqualPredicate);
            greaterThan = propertyPredicate.greaterThan(paramLowerPredicate);
            greaterEqual = propertyPredicate.greaterEqual(paramLowerPredicate);
            lessThan = propertyPredicate.lessThan(paramUpperPredicate);
            lessEqual = propertyPredicate.lessEqual(paramUpperPredicate);
            between = propertyPredicate.between(paramLowerPredicate, paramUpperPredicate);
            greaterThanAndLessThan = lessThan.and(greaterThan);
            greaterEqualAndLessThan = lessThan.and(greaterEqual);
            greaterThanAndLessEqual = lessEqual.and(greaterThan);
            greaterEqualAndLessEqual = lessEqual.and(greaterEqual);
        }
        public void createQuery(Session session) {
            query = session.createQuery(dobj);
        }
        public void setParameterEqual(Object parameter) {
            query.setParameter("equal", parameter);
        }
        public void setParameterLower(Object parameter) {
            query.setParameter("lower", parameter);
        }
        public void setParameterUpper(Object parameter) {
            query.setParameter("upper", parameter);
        }
        public void setExpectedResultIds(int... expecteds) {
            for (int expected:expecteds) {
                expectedSet.add(expected);
            }
        }
        @SuppressWarnings("unchecked")
        public void checkResults(String theQuery) {
            Set<Integer> actualSet = new HashSet<Integer>();
            List<IdBase> resultList = query.getResultList();
            for (IdBase result: resultList) {
                printResultInstance(result);
                actualSet.add(result.getId());
            }
            errorIfNotEqual("Wrong index used in  " + theQuery + " query: ",
                    expectedIndex, query.getTheIndexUsed());
            errorIfNotEqual("Wrong ids returned from " + theQuery + " query: ",
                    expectedSet, actualSet);
            }
    }

    /** Print the result instance. Override this in a subclass if needed.
     * 
     * @param instance the instance to print if needed
     */
    protected void printResultInstance(IdBase instance) {
    }

    public void equalQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // specify the where clause
        holder.dobj.where(holder.equal);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " equal");
        tx.commit();
    }

    public void greaterThanQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query 
        holder.dobj.where(holder.greaterThan);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterLower(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " greaterThan");
        tx.commit();
    }

    public void greaterEqualQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterEqual);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterLower(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " greaterEqual");
        tx.commit();
    }

    public void lessThanQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.lessThan);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " lessThan");
        tx.commit();
    }

    public void lessEqualQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.lessEqual);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " lessEqual");
        tx.commit();
    }

    public void betweenQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.between);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " between");
        tx.commit();
    }

    public void greaterThanAndLessThanQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterThanAndLessThan);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " lessThanAndGreaterThan");
        tx.commit();
    }

    public void greaterEqualAndLessThanQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterEqualAndLessThan);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " lessThanAndGreaterEqual");
        tx.commit();
    }

    public void greaterThanAndLessEqualQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterThanAndLessEqual);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " lessEqualAndGreaterThan");
        tx.commit();
    }

    public void greaterEqualAndLessEqualQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterEqualAndLessEqual);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " lessEqualAndGreaterEqual");
        tx.commit();
    }

}
