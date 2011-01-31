/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

    /** The QueryHolder for this test */
    protected QueryHolder holder;

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
        public QueryDomainType<?> dobj;
        public String propertyName;
        public String extraPropertyName;
        public PredicateOperand propertyPredicate;
        public PredicateOperand paramEqualPredicate;
        public PredicateOperand paramLowerPredicate;
        public PredicateOperand paramUpperPredicate;
        public Predicate equal;
        public Predicate equalOrEqual;
        public Predicate greaterThan;
        public Predicate greaterEqual;
        public Predicate lessThan;
        public Predicate lessEqual;
        public Predicate between;
        public Predicate greaterThanAndLessThan;
        public Predicate greaterEqualAndLessThan;
        public Predicate greaterThanAndLessEqual;
        public Predicate greaterEqualAndLessEqual;
        public Predicate notEqual;
        public Predicate notGreaterThan;
        public Predicate notGreaterEqual;
        public Predicate notLessThan;
        public Predicate notLessEqual;
        public Predicate notBetween;
        public Predicate greaterThanAndNotGreaterThan;
        public Predicate greaterEqualAndNotGreaterThan;
        public Predicate greaterThanAndNotGreaterEqual;
        public Predicate greaterEqualAndNotGreaterEqual;
        public PredicateOperand extraParamEqualPredicate;
        public PredicateOperand extraParamLowerPredicate;
        public PredicateOperand extraParamUpperPredicate;
        public PredicateOperand extraProperty;
        public Predicate extraEqual;
        public Predicate extraGreaterThan;
        public Predicate extraGreaterEqual;
        public Predicate extraLessThan;
        public Predicate extraLessEqual;
        public Predicate extraBetween;
        public Predicate extraGreaterThanAndLessThan;
        public Predicate extraGreaterEqualAndLessThan;
        public Predicate extraGreaterThanAndLessEqual;
        public Predicate extraGreaterEqualAndLessEqual;
        public Query<?> query;
        public Set<Integer> expectedSet = new HashSet<Integer>();
        public String expectedIndex;
        private Predicate equalOrIn;
        private Predicate extraIn;
        public QueryHolder(Class<?> type, String propertyName, String expectedIndex) {
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
            notEqual = equal.not();
            notGreaterThan = greaterThan.not();
            notGreaterEqual = greaterEqual.not();
            notLessThan = lessThan.not();
            notLessEqual = lessEqual.not();
            notBetween = between.not();
            greaterThanAndNotGreaterThan = greaterThan.and(propertyPredicate.greaterThan(paramUpperPredicate).not());
            greaterEqualAndNotGreaterThan = greaterEqual.and(propertyPredicate.greaterThan(paramUpperPredicate).not());
            greaterThanAndNotGreaterEqual = greaterThan.and(propertyPredicate.greaterEqual(paramUpperPredicate).not());
            greaterEqualAndNotGreaterEqual = greaterEqual.and(propertyPredicate.greaterEqual(paramUpperPredicate).not());
        }
        public QueryHolder(Class<?> type, String propertyName, String expectedIndex,
                String extraPropertyName) {
            this(type, propertyName, expectedIndex);
            this.extraPropertyName = extraPropertyName;
            this.extraParamEqualPredicate = dobj.param("extraEqual");
            this.extraParamLowerPredicate = dobj.param("extraLower");
            this.extraParamUpperPredicate = dobj.param("extraUpper");
            // property
            this.extraProperty = dobj.get(extraPropertyName);
            // comparison operations
            this.extraEqual = extraProperty.equal(extraParamEqualPredicate);
            this.extraGreaterThan = extraProperty.greaterThan(extraParamLowerPredicate);
            this.extraGreaterEqual = extraProperty.greaterEqual(extraParamLowerPredicate);
            this.extraLessThan = extraProperty.lessThan(extraParamUpperPredicate);
            this.extraLessEqual = extraProperty.lessEqual(extraParamUpperPredicate);
            this.extraBetween = extraProperty.between(extraParamLowerPredicate, extraParamUpperPredicate);
            this.extraGreaterThanAndLessThan = extraLessThan.and(extraGreaterThan);
            this.extraGreaterEqualAndLessThan = extraLessThan.and(extraGreaterEqual);
            this.extraGreaterThanAndLessEqual = extraLessEqual.and(extraGreaterThan);
            this.extraGreaterEqualAndLessEqual = extraLessEqual.and(extraGreaterEqual);
            this.equalOrEqual = equal.or(extraEqual);
            this.extraIn = extraProperty.in(extraParamEqualPredicate);
            this.equalOrIn = equal.or(extraIn);
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
        public void setExtraParameterEqual(Object parameter) {
            query.setParameter("extraEqual", parameter);
        }
        public void setExtraParameterLower(Object parameter) {
            query.setParameter("extraLower", parameter);
        }
        public void setExtraParameterUpper(Object parameter) {
            query.setParameter("extraUpper", parameter);
        }

        @SuppressWarnings("unchecked")
        public void checkResults(String theQuery) {
            Set<Integer> actualSet = new HashSet<Integer>();
            List<IdBase> resultList = (List<IdBase>) query.getResultList();
            for (IdBase result: resultList) {
                printResultInstance(result);
                actualSet.add(result.getId());
            }
            errorIfNotEqual("Wrong index used in  " + theQuery + " query: ",
                    expectedIndex, query.explain().get("IndexUsed"));
            errorIfNotEqual("Wrong ids returned from " + theQuery + " query: ",
                    expectedSet, actualSet);
            }
    }

    /** This interface is for extra predicates. When the method is invoked, the
     * QueryHolder has not been created, so this callback is executed to
     * provide an extra query predicate after the holder is created.
     */
    public interface PredicateProvider {
        public Predicate getPredicate(QueryHolder holder);
    }

    PredicateProvider extraEqualPredicateProvider = 
        new PredicateProvider() {
            public Predicate getPredicate(QueryHolder holder) {
                return holder.extraEqual;
                }
            };
    
    PredicateProvider extraNotEqualPredicateProvider = 
        new PredicateProvider() {
            public Predicate getPredicate(QueryHolder holder) {
                return holder.extraEqual.not();
                }
            };
    
    PredicateProvider extraBetweenPredicateProvider = 
        new PredicateProvider() {
            public Predicate getPredicate(QueryHolder holder) {
                return holder.extraBetween;
                }
            };
            
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

    public void equalOrEqualQuery(String propertyName, Object parameterValue1,
            String extraPropertyName, Object parameterValue2, 
            String expectedIndex, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex, extraPropertyName);
        // specify the where clause
        holder.dobj.where(holder.equalOrEqual);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue1);
        holder.setExtraParameterEqual(parameterValue2);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " equal or equal");
        tx.commit();
    }

    public void equalOrInQuery(String propertyName, Object parameterValue1,
            String extraPropertyName, Object parameterValue2, 
            String expectedIndex, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex, extraPropertyName);
        // specify the where clause
        holder.dobj.where(holder.equalOrIn);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue1);
        holder.setExtraParameterEqual(parameterValue2);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " equal or in");
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

    public void equalAnd1ExtraQuery(String propertyName, Object parameterValue,
            String extraPropertyName, PredicateProvider extraPredicateProvider, Object extraParameterValue, 
            String expectedIndex, int... expected) {
        tx.begin();
        holder = new QueryHolder(instanceType, propertyName, expectedIndex,
                extraPropertyName);
        // specify the where clause
        Predicate extraPredicate = extraPredicateProvider.getPredicate(holder);
        holder.dobj.where(holder.equal.and(extraPredicate));
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue);
        holder.setParameterLower(parameterValue);
        holder.setParameterUpper(parameterValue);
        holder.setExtraParameterEqual(extraParameterValue);
        holder.setExtraParameterLower(extraParameterValue);
        holder.setExtraParameterUpper(extraParameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " equal and " + extraPredicate);
        tx.commit();
    }

    public void equalAnd2ExtraQuery(String propertyName, Object parameterValue,
            String extraPropertyName, PredicateProvider extraPredicateProvider,
            Object extraParameterValue1, Object extraParameterValue2,
            String expectedIndex, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex,
                extraPropertyName);
        // specify the where clause
        Predicate extraPredicate = extraPredicateProvider.getPredicate(holder);
        holder.dobj.where(holder.equal.and(extraPredicate));
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue);
        holder.setParameterLower(parameterValue);
        holder.setParameterUpper(parameterValue);
        holder.setExtraParameterEqual(extraParameterValue1);
        holder.setExtraParameterLower(extraParameterValue1);
        holder.setExtraParameterUpper(extraParameterValue2);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " equal and " + extraPredicate);
        tx.commit();
    }

    public void notEqualQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // specify the where clause
        holder.dobj.where(holder.notEqual);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " not equal");
        tx.commit();
    }

    public void notNotEqualQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // specify the where clause
        holder.dobj.where(holder.notEqual.not());
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " not not equal");
        tx.commit();
    }

    public void notNotNotEqualQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // specify the where clause
        holder.dobj.where(holder.dobj.not(holder.notEqual.not()));
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " not not not equal");
        tx.commit();
    }

    public void notGreaterThanQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query 
        holder.dobj.where(holder.notGreaterThan);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterLower(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " not greaterThan");
        tx.commit();
    }

    public void notGreaterEqualQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.notGreaterEqual);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterLower(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " not greaterEqual");
        tx.commit();
    }

    public void notLessThanQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.notLessThan);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " not lessThan");
        tx.commit();
    }

    public void notLessEqualQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.notLessEqual);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " not lessEqual");
        tx.commit();
    }

    public void notBetweenQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.notBetween);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " not between");
        tx.commit();
    }

    public void greaterThanAndNotGreaterThanQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterThanAndNotGreaterThan);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " greaterThanAndNotGreaterThan");
        tx.commit();
    }

    public void greaterEqualAndNotGreaterThanQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterEqualAndNotGreaterThan);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " greaterEqualAndNotGreaterThan");
        tx.commit();
    }

    public void greaterThanAndNotGreaterEqualQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterThanAndNotGreaterEqual);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " greaterThanAndNotGreaterEqual");
        tx.commit();
    }

    public void greaterEqualAndNotGreaterEqualQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(instanceType, propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterEqualAndNotGreaterEqual);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " greaterEqualAndNotGreaterEqual");
        tx.commit();
    }

}
