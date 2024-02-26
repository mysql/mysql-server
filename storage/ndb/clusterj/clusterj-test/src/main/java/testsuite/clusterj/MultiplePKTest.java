/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

import com.mysql.clusterj.Query;

import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.QueryBuilder;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import testsuite.clusterj.model.Dn2id;

public class MultiplePKTest extends AbstractClusterJModelTest {

    private static final int NUMBER_OF_INSTANCES = 10;

    // QueryBuilder is the sessionFactory for queries
    QueryBuilder builder;
    // QueryDomainType is the main interface
    QueryDomainType dobj;

    // compare the columns with the parameters
    Predicate equalA0;
    Predicate equalA1;
    Predicate equalA2;
    Predicate equalA3;
    Predicate lessEqualA3;
    Predicate greaterEqualA3;
    Predicate lessA3;
    Predicate greaterA3;
    Predicate betweenA3;
    Predicate equalA4;
    Predicate equalA5;
    Predicate equalA6;
    Predicate equalA7;
    Predicate equalA8;
    Predicate equalA9;
    Predicate equalA10;
    Predicate equalA11;
    Predicate equalA12;
    Predicate equalA13;
    Predicate equalA14;
    Predicate equalA15;

        @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        try {
            tx.begin();
            session.deletePersistentAll(Dn2id.class);
            tx.commit();
        } catch (Throwable t) {
            // ignore errors while deleting
        }
        addTearDownClasses(Dn2id.class);
    }

    protected void setupQuery() {
        // QueryBuilder is the sessionFactory for queries
        builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        dobj = builder.createQueryDefinition(Dn2id.class);

        // compare the columns with the parameters
        equalA0 = dobj.get("a0").equal(dobj.param("a0"));
        equalA1 = dobj.get("a1").equal(dobj.param("a1"));
        equalA2 = dobj.get("a2").equal(dobj.param("a2"));
        equalA3 = dobj.get("a3").equal(dobj.param("a3"));
        lessA3 = dobj.get("a3").lessThan(dobj.param("a3Upper"));
        lessEqualA3 = dobj.get("a3").lessEqual(dobj.param("a3Upper"));
        greaterA3 = dobj.get("a3").greaterThan(dobj.param("a3Lower"));
        greaterEqualA3 = dobj.get("a3").greaterEqual(dobj.param("a3Lower"));
        betweenA3 = dobj.get("a3").between(dobj.param("a3Lower"), dobj.param("a3Upper"));
        equalA4 = dobj.get("a4").equal(dobj.param("a4"));
        equalA5 = dobj.get("a5").equal(dobj.param("a5"));
        equalA6 = dobj.get("a6").equal(dobj.param("a6"));
        equalA7 = dobj.get("a7").equal(dobj.param("a7"));
        equalA8 = dobj.get("a8").equal(dobj.param("a8"));
        equalA9 = dobj.get("a9").equal(dobj.param("a9"));
        equalA10 = dobj.get("a10").equal(dobj.param("a10"));
        equalA11 = dobj.get("a11").equal(dobj.param("a11"));
        equalA12 = dobj.get("a12").equal(dobj.param("a12"));
        equalA13 = dobj.get("a13").equal(dobj.param("a13"));
        equalA14 = dobj.get("a14").equal(dobj.param("a14"));
        equalA15 = dobj.get("a15").equal(dobj.param("a15"));
    }

    public void test() {
        insert();
        findByPrimaryKey();
        queryEqualPartialPrimaryKey();
        queryLessEqualGreaterEqualPartialPrimaryKey();
        queryLessGreaterEqualPartialPrimaryKey();
        queryLessEqualGreaterPartialPrimaryKey();
        queryLessGreaterPartialPrimaryKey();
        queryBetweenPartialPrimaryKey();
        queryCompletePrimaryKey();
//        queryUniqueKey();
        update();
        delete();
        failOnError();
    }

    private void insert() {
        createDn2idInstances(NUMBER_OF_INSTANCES);
        tx = session.currentTransaction();
        tx.begin();
        session.makePersistentAll(dn2ids);
        tx.commit();
    }

    protected void findByPrimaryKey() {
        tx.begin();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            dn2idPK[1] = getA1for(NUMBER_OF_INSTANCES, i);
            dn2idPK[3] = "employeenumber=100000" + i;
            Dn2id d = session.find(Dn2id.class, dn2idPK);
            // verify eid
            int expected = i;
            long actual = d.getEid();
            if (expected != actual) {
                error("findByPrimaryKey failed to find dn2id " + i
                        + " expected eid " + expected
                        + " actual eid " + actual);
            }
        }
        tx.commit();
    }

    private void queryEqualPartialPrimaryKey() {
        tx.begin();
        setupQuery();
        // compare the column with the parameter
        dobj.where(equalA0.and(equalA1.and(equalA2.and(equalA3))));
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("a0", "dc=com");
        query.setParameter("a1", getA1for(NUMBER_OF_INSTANCES, 8));
        query.setParameter("a2", "ou=people");
        query.setParameter("a3", getA3for(8));
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)8);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id d: results) {
            actual.add(d.getEid());
        }
        errorIfNotEqual("queryEqualPartialPrimaryKey wrong Dn2id eids returned from query: ",
                expected, actual);
        tx.commit();
    }

    private void queryLessEqualGreaterEqualPartialPrimaryKey() {
        tx.begin();
        setupQuery();
        // compare the column with the parameter
        dobj.where(equalA0.and(equalA1.and(equalA2.and(
                lessEqualA3.and(greaterEqualA3)))));
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("a0", "dc=com");
        query.setParameter("a1", getA1for(NUMBER_OF_INSTANCES, 8));
        query.setParameter("a2", "ou=people");
        query.setParameter("a3Upper", getA3for(9));
        query.setParameter("a3Lower", getA3for(8));
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)8);
        expected.add((long)9);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id d: results) {
            actual.add(d.getEid());
        }
        errorIfNotEqual("queryLessEqualGreaterEqualPartialPrimaryKey wrong Dn2id eids returned from query: ",
                expected, actual);
        tx.commit();
    }

    private void queryLessGreaterEqualPartialPrimaryKey() {
        tx.begin();
        setupQuery();
        // compare the column with the parameter
        dobj.where(equalA0.and(equalA1.and(equalA2.and(
                lessA3.and(greaterEqualA3)))));
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("a0", "dc=com");
        query.setParameter("a1", getA1for(NUMBER_OF_INSTANCES, 8));
        query.setParameter("a2", "ou=people");
        query.setParameter("a3Lower", getA3for(8));
        query.setParameter("a3Upper", getA3for(9));
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)8);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id d: results) {
            actual.add(d.getEid());
        }
        errorIfNotEqual("queryLessGreaterEqualPartialPrimaryKey wrong Dn2id eids returned from query: ",
                expected, actual);
        tx.commit();
    }

    private void queryLessEqualGreaterPartialPrimaryKey() {
        tx.begin();
        setupQuery();
        // compare the column with the parameter
        dobj.where(equalA0.and(equalA1.and(equalA2.and(
                lessEqualA3.and(greaterA3)))));
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("a0", "dc=com");
        query.setParameter("a1", getA1for(NUMBER_OF_INSTANCES, 8));
        query.setParameter("a2", "ou=people");
        query.setParameter("a3Lower", getA3for(7));
        query.setParameter("a3Upper", getA3for(8));
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)8);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id d: results) {
            actual.add(d.getEid());
        }
        errorIfNotEqual("queryLessEqualGreaterPartialPrimaryKey wrong Dn2id eids returned from query: ",
                expected, actual);
        tx.commit();
    }

    private void queryLessGreaterPartialPrimaryKey() {
        tx.begin();
        setupQuery();
        // compare the column with the parameter
        dobj.where(equalA0.and(equalA1.and(equalA2.and(
                lessA3.and(greaterA3)))));
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("a0", "dc=com");
        query.setParameter("a1", getA1for(NUMBER_OF_INSTANCES, 8));
        query.setParameter("a2", "ou=people");
        query.setParameter("a3Lower", getA3for(7));
        query.setParameter("a3Upper", getA3for(9));
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)8);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id d: results) {
            actual.add(d.getEid());
        }
        errorIfNotEqual("queryLessGreaterPartialPrimaryKey wrong Dn2id eids returned from query: ",
                expected, actual);
        tx.commit();
    }

    private void queryBetweenPartialPrimaryKey() {
        tx.begin();
        setupQuery();
        // compare the column with the parameter
        dobj.where(equalA0.and(equalA1.and(equalA2.and(
                betweenA3))));
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("a0", "dc=com");
        query.setParameter("a1", getA1for(NUMBER_OF_INSTANCES, 8));
        query.setParameter("a2", "ou=people");
        query.setParameter("a3Lower", getA3for(8));
        query.setParameter("a3Upper", getA3for(9));
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)8);
        expected.add((long)9);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id d: results) {
            actual.add(d.getEid());
        }
        errorIfNotEqual("queryBetweenPartialPrimaryKey wrong Dn2id eids returned from query: ",
                expected, actual);
        tx.commit();
    }

    private void queryCompletePrimaryKey() {
        tx.begin();
        setupQuery();

        // compare the columns with the parameters
        dobj.where(equalA0.and(equalA1.and(equalA2.and(equalA3)))
                .and(equalA4.and(equalA5.and(equalA6.and(equalA7))))
                .and(equalA8.and(equalA9.and(equalA10.and(equalA11))))
                .and(equalA12.and(equalA13.and(equalA14.and(equalA15)))));
        // create a query instance
        Query query = session.createQuery(dobj);

        // set the parameter values
        query.setParameter("a0", "dc=com");
        query.setParameter("a1", getA1for(NUMBER_OF_INSTANCES, 8));
        query.setParameter("a2", "ou=people");
        query.setParameter("a3", getA3for(8));
        query.setParameter("a4", "");
        query.setParameter("a5", "");
        query.setParameter("a6", "");
        query.setParameter("a7", "");
        query.setParameter("a8", "");
        query.setParameter("a9", "");
        query.setParameter("a10", "");
        query.setParameter("a11", "");
        query.setParameter("a12", "");
        query.setParameter("a13", "");
        query.setParameter("a14", "");
        query.setParameter("a15", "");
        // get the results
        List<Dn2id> results = query.getResultList();
        // consistency check the results
        consistencyCheck(results);
        // verify we got the right instances
        Set<Long> expected = new HashSet<Long>();
        expected.add((long)8);
        Set<Long> actual = new HashSet<Long>();
        for (Dn2id d: results) {
            actual.add(d.getEid());
        }
        errorIfNotEqual("queryCompletePrimaryKey wrong Dn2id eids returned from query: ",
                expected, actual);
        tx.commit();
    }

    private void queryUniqueKey() {
        throw new UnsupportedOperationException("Not yet implemented");
    }

    /** Update eid for all instances using find.
     *
     */
    protected void update() {
        tx.begin();
        List<Dn2id> updatedInstances = new ArrayList<Dn2id>();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            dn2idPK[1] = getA1for(NUMBER_OF_INSTANCES, i);
            dn2idPK[3] = "employeenumber=100000" + i;
            Dn2id d = session.find(Dn2id.class, dn2idPK);
            // verify eid
            long expected = i;
            long actual = d.getEid();
            if (expected != actual) {
                error("Failed to find dn2id " + i
                        + " expected eid " + expected
                        + " actual eid " + actual);
            }
            // now update the eid field
            d.setEid(NUMBER_OF_INSTANCES + d.getEid());
            updatedInstances.add(d);
        }
        session.updatePersistentAll(updatedInstances);
        tx.commit();
        // now find and verify
        tx.begin();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            dn2idPK[1] = getA1for(NUMBER_OF_INSTANCES, i);
            dn2idPK[3] = "employeenumber=100000" + i;
            Dn2id d = session.find(Dn2id.class, dn2idPK);
            // verify eid
            long expected = NUMBER_OF_INSTANCES + i;
            long actual = d.getEid();
            if (expected != actual) {
                error("Failed to find updated dn2id " + i
                        + " expected eid " + expected
                        + " actual eid " + actual);
            }
        }
        tx.commit();
    }

    private void delete() {
        tx.begin();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            dn2idPK[1] = getA1for(NUMBER_OF_INSTANCES, i);
            dn2idPK[3] = "employeenumber=100000" + i;
            session.deletePersistent(Dn2id.class, dn2idPK);
        }
        tx.commit();
    }

}
