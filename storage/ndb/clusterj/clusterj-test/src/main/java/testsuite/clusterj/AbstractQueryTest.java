/*
   Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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

    /**
     * Create instances required by the test, used for the queries.
     * @param number the number of instances to create
     */
    abstract void createInstances(int number);

    /** Most query tests use the same number of instances (10).
     * 
     */
    @Override
    protected int getNumberOfInstances() {
        return 10;
    }

    /**
     * Return the type of instances used for the queries.
     * @return the type of instances for the test
     */
    abstract Class<?> getInstanceType();

    /** The QueryHolder for this test */
    protected QueryHolder holder;

    private boolean autotransaction;

    @Override
    public void localSetUp() {
        setAutotransaction(false);
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();
        int numberOfInstances = getNumberOfInstances();
        createInstances(numberOfInstances);
        session.deletePersistentAll(getInstanceType());
        session.makePersistentAll(instances);
        if (getCleanupAfterTest())
            addTearDownClasses(getInstanceType());
    }

    protected void setAutotransaction(boolean b) {
        autotransaction = b;
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
        public PredicateOperand paramInPredicate;
        public Predicate equal;
        public Predicate equalOrEqual;
        public Predicate greaterThan;
        public Predicate greaterEqual;
        public Predicate in;
        public Predicate lessThan;
        public Predicate lessEqual;
        public Predicate between;
        public Predicate greaterThanAndLessThan;
        public Predicate greaterEqualAndLessThan;
        public Predicate greaterThanAndLessEqual;
        public Predicate greaterThanAndLike;
        public Predicate greaterEqualAndLessEqual;
        public Predicate greaterEqualAndLike;
        public Predicate notEqual;
        public Predicate notGreaterThan;
        public Predicate notGreaterEqual;
        public Predicate notLessThan;
        public Predicate notLessEqual;
        public Predicate notBetween;
        public Predicate like;
        public Predicate greaterThanAndNotGreaterThan;
        public Predicate greaterEqualAndNotGreaterThan;
        public Predicate greaterThanAndNotGreaterEqual;
        public Predicate greaterEqualAndNotGreaterEqual;
        public PredicateOperand extraParamEqualPredicate;
        public PredicateOperand extraParamLowerPredicate;
        public PredicateOperand extraParamUpperPredicate;
        public PredicateOperand extraParamInPredicate;
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
        private Predicate inAndIn;
        private Predicate inAndBetween;
        private Predicate betweenAndIn;
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
            paramInPredicate = dobj.param("in");
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
            in = propertyPredicate.in(paramInPredicate);
            notEqual = equal.not();
            notGreaterThan = greaterThan.not();
            notGreaterEqual = greaterEqual.not();
            notLessThan = lessThan.not();
            notLessEqual = lessEqual.not();
            notBetween = between.not();
            like = propertyPredicate.like(paramEqualPredicate);
            greaterThanAndNotGreaterThan = greaterThan.and(propertyPredicate.greaterThan(paramUpperPredicate).not());
            greaterEqualAndNotGreaterThan = greaterEqual.and(propertyPredicate.greaterThan(paramUpperPredicate).not());
            greaterThanAndNotGreaterEqual = greaterThan.and(propertyPredicate.greaterEqual(paramUpperPredicate).not());
            greaterEqualAndNotGreaterEqual = greaterEqual.and(propertyPredicate.greaterEqual(paramUpperPredicate).not());
            greaterThanAndLike = greaterThan.and(propertyPredicate.like(paramUpperPredicate));
            greaterEqualAndLike = greaterEqual.and(propertyPredicate.like(paramUpperPredicate));
        }
        public QueryHolder(Class<?> type, String propertyName, String expectedIndex,
                String extraPropertyName) {
            this(type, propertyName, expectedIndex);
            this.extraPropertyName = extraPropertyName;
            this.extraParamEqualPredicate = dobj.param("extraEqual");
            this.extraParamLowerPredicate = dobj.param("extraLower");
            this.extraParamUpperPredicate = dobj.param("extraUpper");
            this.extraParamInPredicate = dobj.param("extraIn");
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
            this.extraIn = extraProperty.in(extraParamInPredicate);
            this.equalOrIn = equal.or(extraIn);
            this.inAndIn = in.and(extraIn);
            this.inAndBetween = in.and(extraBetween);
            this.betweenAndIn = between.and(extraIn);
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
        public void setParameterIn(Object parameter) {
            query.setParameter("in", parameter);
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

        public void setExtraParameterIn(Object parameter) {
            query.setParameter("extraIn", parameter);
        }

        @SuppressWarnings("unchecked")
        public void checkResults(String theQuery) {
            Set<Integer> actualSet = new HashSet<Integer>();
            List<IdBase> resultList = (List<IdBase>) query.getResultList();
            for (IdBase result: resultList) {
                printResultInstance(result);
                actualSet.add(result.getId());
            }
            errorIfNotEqual("Wrong index used for " + theQuery + " query: ",
                    expectedIndex, query.explain().get("IndexUsed"));
            errorIfNotEqual("Wrong ids returned from " + theQuery + " query: ",
                    expectedSet, actualSet);
            }

        public void checkDeletePersistentAll(String where, int expectedNumberOfDeletedInstances) {
            int result = query.deletePersistentAll();
            errorIfNotEqual("Wrong index used for " + where + " delete  query: ",
                    expectedIndex, query.explain().get("IndexUsed"));
            errorIfNotEqual("Wrong number of instances deleted for " + where, 
                    expectedNumberOfDeletedInstances, result);
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
            public String toString() {
                return " equal";
            }
            };
    
    PredicateProvider extraNotEqualPredicateProvider = 
        new PredicateProvider() {
            public Predicate getPredicate(QueryHolder holder) {
                return holder.extraEqual.not();
                }
            public String toString() {
                return " not equal";
            }
            };
    
    PredicateProvider extraBetweenPredicateProvider = 
        new PredicateProvider() {
            public Predicate getPredicate(QueryHolder holder) {
                return holder.extraBetween;
                }
            public String toString() {
                return " between";
            }
            };
            
    PredicateProvider extraInPredicateProvider = 
        new PredicateProvider() {
            public Predicate getPredicate(QueryHolder holder) {
                return holder.extraIn;
                }
            public String toString() {
                return " in";
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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

    public void likeQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
        // specify the where clause
        holder.dobj.where(holder.like);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " like");
        tx.commit();
    }

    public void deleteEqualQuery(String propertyName, String expectedIndex,
            Object parameterValue, int expected) {
        if (!autotransaction) {
            tx.begin();
        }
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
        // specify the where clause
        holder.dobj.where(holder.equal);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue);
        // get the results
        holder.checkDeletePersistentAll(propertyName + " delete equal", expected);
        if (!autotransaction) {
            tx.commit();
        }
    }

    public void equalOrEqualQuery(String propertyName, Object parameterValue1,
            String extraPropertyName, Object parameterValue2, 
            String expectedIndex, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex, extraPropertyName);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex, extraPropertyName);
        // specify the where clause
        holder.dobj.where(holder.equalOrIn);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterEqual(parameterValue1);
        holder.setExtraParameterIn(parameterValue2);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " equal or in");
        tx.commit();
    }

    public void inQuery(String propertyName, Object parameterValue1,
            String expectedIndex, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
        // specify the where clause
        holder.dobj.where(holder.in);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterIn(parameterValue1);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " in");
        tx.commit();
    }

    public void inAndInQuery(String propertyName, Object parameterValue1,
            String extraPropertyName, Object parameterValue2, 
            String expectedIndex, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex, extraPropertyName);
        // specify the where clause
        holder.dobj.where(holder.inAndIn);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterIn(parameterValue1);
        holder.setExtraParameterIn(parameterValue2);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " in and " + extraPropertyName + " in");
        tx.commit();
    }

    public void inAndBetweenQuery(String propertyName, Object parameterValue1,
            String extraPropertyName, Object parameterValue2, Object parameterValue3,
            String expectedIndex, int...expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex, extraPropertyName);
        // specify the where clause
        holder.dobj.where(holder.inAndBetween);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterIn(parameterValue1);
        holder.setExtraParameterLower(parameterValue2);
        holder.setExtraParameterUpper(parameterValue3);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " in and " + extraPropertyName + " between");
        tx.commit();
    }

    public void betweenAndInQuery(String propertyName, Object parameterValue1, Object parameterValue2,
            String extraPropertyName, Object parameterValue3, 
            String expectedIndex, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex, extraPropertyName);
        // specify the where clause
        holder.dobj.where(holder.betweenAndIn);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterLower(parameterValue1);
        holder.setParameterUpper(parameterValue2);
        holder.setExtraParameterIn(parameterValue3);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " between and " + extraPropertyName + " in");
        tx.commit();
    }

    public void greaterThanQuery(String propertyName, String expectedIndex,
            Object parameterValue, int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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

    public void greaterThanAndLikeQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterThanAndLike);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " greaterThanAndLike");
        tx.commit();
    }

    public void deleteGreaterThanAndLessThanQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int expected) {
        if (!autotransaction) {
            tx.begin();
        }
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterThanAndLessThan);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.checkDeletePersistentAll(propertyName + " delete lessThanAndGreaterThan", expected);
        if (!autotransaction) {
            tx.commit();
        }
    }

    public void greaterEqualAndLessThanQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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

    public void greaterEqualAndLikeQuery(String propertyName, String expectedIndex,
            Object parameterLowerValue, Object parameterUpperValue,
            int... expected) {

        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
        // set the where clause into the query
        holder.dobj.where(holder.greaterEqualAndLike);
        // create the query
        holder.createQuery(session);
        // set the parameter value
        holder.setParameterUpper(parameterUpperValue);
        holder.setParameterLower(parameterLowerValue);
        // get the results
        holder.setExpectedResultIds(expected);
        holder.checkResults(propertyName + " greaterEqualAndLike");
        tx.commit();
    }

    public void equalAnd1ExtraQuery(String propertyName, Object parameterValue,
            String extraPropertyName, PredicateProvider extraPredicateProvider, Object extraParameterValue, 
            String expectedIndex, int... expected) {
        tx.begin();
        holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex,
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

    public void greaterThanAnd1ExtraQuery(String propertyName, Object parameterValue,
            String extraPropertyName, PredicateProvider extraPredicateProvider, Object extraParameterValue, 
            String expectedIndex, int... expected) {
        tx.begin();
        holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex,
                extraPropertyName);
        // specify the where clause
        Predicate extraPredicate = extraPredicateProvider.getPredicate(holder);
        holder.dobj.where(holder.greaterThan.and(extraPredicate));
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
        holder.checkResults(propertyName + " greater than and " + extraPropertyName + extraPredicateProvider.toString());
        tx.commit();
    }

    public void greaterEqualAnd1ExtraQuery(String propertyName, Object parameterValue,
            String extraPropertyName, PredicateProvider extraPredicateProvider, Object extraParameterValue, 
            String expectedIndex, int... expected) {
        tx.begin();
        holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex,
                extraPropertyName);
        // specify the where clause
        Predicate extraPredicate = extraPredicateProvider.getPredicate(holder);
        holder.dobj.where(holder.greaterEqual.and(extraPredicate));
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
        holder.checkResults(propertyName + " greater equal and " + extraPropertyName + extraPredicateProvider.toString());
        tx.commit();
    }

    public void lessThanAnd1ExtraQuery(String propertyName, Object parameterValue,
            String extraPropertyName, PredicateProvider extraPredicateProvider, Object extraParameterValue, 
            String expectedIndex, int... expected) {
        tx.begin();
        holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex,
                extraPropertyName);
        // specify the where clause
        Predicate extraPredicate = extraPredicateProvider.getPredicate(holder);
        holder.dobj.where(holder.lessThan.and(extraPredicate));
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
        holder.checkResults(propertyName + " less than and " + extraPropertyName + extraPredicateProvider.toString());
        tx.commit();
    }

    public void lessEqualAnd1ExtraQuery(String propertyName, Object parameterValue,
            String extraPropertyName, PredicateProvider extraPredicateProvider, Object extraParameterValue, 
            String expectedIndex, int... expected) {
        tx.begin();
        holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex,
                extraPropertyName);
        // specify the where clause
        Predicate extraPredicate = extraPredicateProvider.getPredicate(holder);
        holder.dobj.where(holder.lessEqual.and(extraPredicate));
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
        holder.checkResults(propertyName + " less equal and " + extraPropertyName + extraPredicateProvider.toString());
        tx.commit();
    }

    public void equalAnd2ExtraQuery(String propertyName, Object parameterValue,
            String extraPropertyName, PredicateProvider extraPredicateProvider,
            Object extraParameterValue1, Object extraParameterValue2,
            String expectedIndex, int... expected) {
        tx.begin();
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex,
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
        QueryHolder holder = new QueryHolder(getInstanceType(), propertyName, expectedIndex);
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
