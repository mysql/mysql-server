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

import testsuite.clusterj.model.Dn2id;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.PredicateOperand;
import com.mysql.clusterj.Query;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import testsuite.clusterj.model.Dn2id;

public class QueryUniqueKeyTest extends AbstractClusterJModelTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();
        createDn2idInstances(10);
        try {
            tx.begin();
            session.deletePersistentAll(Dn2id.class);
            tx.commit();
        } catch (Throwable t) {
            // ignore errors while deleting
        }
        tx.begin();
        session.makePersistentAll(dn2ids);
        tx.commit();
        addTearDownClasses(Dn2id.class);
    }

    /** Test all queries using the same setup.
     * Fail if any errors during the tests.
     */
    public void testUniqueKey() {
        uniqueKeyBetweenQuery();
        uniqueKeyEqualQuery();
        uniqueKeyGreaterEqualQuery();
        uniqueKeyGreaterThanQuery();
        uniqueKeyLessEqualQuery();
        uniqueKeyLessThanQuery();
        failOnError();
    }
    public void uniqueKeyEqualQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Dn2id.class);

        // parameter name
        PredicateOperand param = dobj.param("eid");
        // property name
        PredicateOperand column = dobj.get("eid");
        // compare the column with the parameter
        Predicate compare = column.equal(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter value
        query.setParameter("eid", (long)8);
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)8);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id dn2id: results) {
            actual.add(dn2id.getEid());
        }
        errorIfNotEqual("Wrong Dn2id eids returned from uniqueKeyEqualQuery query: ",
                expected, actual);
        tx.commit();
    }

    public void uniqueKeyGreaterThanQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Dn2id.class);

        // parameter name
        PredicateOperand param = dobj.param("eid");
        // property name
        PredicateOperand column = dobj.get("eid");
        // compare the column with the parameter
        Predicate compare = column.greaterThan(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("eid", (long)6);
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)7);
        expected.add((long)8);
        expected.add((long)9);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id dn2id: results) {
            actual.add(dn2id.getEid());
        }
        errorIfNotEqual("Wrong Dn2id eids returned from uniqueKeyGreaterThanQuery query: ",
                expected, actual);
        tx.commit();
    }

    public void uniqueKeyGreaterEqualQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Dn2id.class);

        // parameter name
        PredicateOperand param = dobj.param("eid");
        // property name
        PredicateOperand column = dobj.get("eid");
        // compare the column with the parameter
        Predicate compare = column.greaterEqual(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("eid", (long)7);
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)7);
        expected.add((long)8);
        expected.add((long)9);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id dn2id: results) {
            actual.add(dn2id.getEid());
        }
        errorIfNotEqual("Wrong Dn2id eids returned from uniqueKeyGreaterEqualQuery query: ",
                expected, actual);
        tx.commit();
    }

    public void uniqueKeyLessThanQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Dn2id.class);

        // parameter name
        PredicateOperand param = dobj.param("eid");
        // property name
        PredicateOperand column = dobj.get("eid");
        // compare the column with the parameter
        Predicate compare = column.lessThan(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("eid", (long)3);
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)0);
        expected.add((long)1);
        expected.add((long)2);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id dn2id: results) {
            actual.add(dn2id.getEid());
        }
        errorIfNotEqual("Wrong Dn2id eids returned from uniqueKeyLessThanQuery query: ",
                expected, actual);
        tx.commit();
    }

    public void uniqueKeyLessEqualQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Dn2id.class);

        // parameter name
        PredicateOperand param = dobj.param("eid");
        // property name
        PredicateOperand column = dobj.get("eid");
        // compare the column with the parameter
        Predicate compare = column.lessEqual(param);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("eid", (long)2);
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)0);
        expected.add((long)1);
        expected.add((long)2);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id dn2id: results) {
            actual.add(dn2id.getEid());
        }
        errorIfNotEqual("Wrong Dn2id eids returned from uniqueKeyLessEqualQuery query: ",
                expected, actual);
        tx.commit();
    }

    public void uniqueKeyBetweenQuery() {

        tx.begin();
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType dobj = builder.createQueryDefinition(Dn2id.class);

        // parameter name
        PredicateOperand lower = dobj.param("lower");
        // parameter name
        PredicateOperand upper = dobj.param("upper");
        // property name
        PredicateOperand column = dobj.get("eid");
        // compare the column with the parameter
        Predicate compare = column.between(lower, upper);
        // set the where clause into the query 
        dobj.where(compare);
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("lower", (long)5);
        query.setParameter("upper", (long)7);
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)5);
        expected.add((long)6);
        expected.add((long)7);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id dn2id: results) {
            actual.add(dn2id.getEid());
        }
        errorIfNotEqual("Wrong Dn2id eids returned from uniqueKeyBetweenQuery query: ",
                expected, actual);
        tx.commit();
    }
}
