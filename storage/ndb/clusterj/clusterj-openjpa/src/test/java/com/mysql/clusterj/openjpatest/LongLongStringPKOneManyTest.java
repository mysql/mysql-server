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

package com.mysql.clusterj.openjpatest;

import com.mysql.clusterj.jpatest.AbstractJPABaseTest;
import com.mysql.clusterj.jpatest.model.LongLongStringFKManyOne;
import com.mysql.clusterj.jpatest.model.LongLongStringPKOneMany;
import com.mysql.clusterj.jpatest.model.LongLongStringOid;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;

/**
 *
 */
public class LongLongStringPKOneManyTest extends AbstractJPABaseTest {

    private int NUMBER_OF_A = 2;
    private int OFFSET_A = 100;
    private int NUMBER_OF_B = 4;
    private int OFFSET_B = 10;

    private LongLongStringPKOneMany a0 = null;
    private List<LongLongStringPKOneMany> as = new ArrayList<LongLongStringPKOneMany>();

    // set this to true for debug output
    private boolean print = false;

    @Override
    public void setUp() {
        super.setUp();
    }

    @Override
    protected String getPersistenceUnitName() {
        return "ndb";
    }

    /** This tests delete, insert, find, and update of entities with compound
     * primary and foreign keys.
     */
    public void test() {
        LongLongStringPKOneMany a;
        em = emf.createEntityManager();
        begin();
        // bulk remove all LongLongStringFKManyOne
        int countB = em.createQuery("DELETE FROM LongLongStringFKManyOne").executeUpdate();
        int countA = em.createQuery("DELETE FROM LongLongStringPKOneMany").executeUpdate();
        print ("Deleted " + countB + " instances of LongLongStringFKManyOne " +
                countA + " instances of LongLongStringFKManyOne ");
        commit();
        em.close();

        em = emf.createEntityManager();
        begin();
        print("Creating " + NUMBER_OF_A + " instances of LongLongStringPKOneMany.");
        for (int i = OFFSET_A; i < OFFSET_A + NUMBER_OF_A; ++i) {
            a0 = LongLongStringPKOneMany.create(i);
            em.persist(a0);
        }
        Collection<LongLongStringFKManyOne> bs = new HashSet<LongLongStringFKManyOne>();
        print("Creating " + NUMBER_OF_B + " instances of LongLongStringFKManyOne.");
        for (int i = OFFSET_B; i < OFFSET_B + NUMBER_OF_B; ++i) {
            LongLongStringFKManyOne b = LongLongStringFKManyOne.create(i);
            b.setLongLongStringPKRelationship(a0);
            bs.add(b);
            em.persist(b);
        }
        a0.setLongLongStringFKRelationships(bs);
        print("Before commit, " + a0.toString());
        for (LongLongStringFKManyOne b:bs){print(b.toString());}
        commit();
        em.close();

        em = emf.createEntityManager();
        print("Finding " + NUMBER_OF_A + " instances of LongLongStringPKOneMany.");
        begin();
        for (int i = OFFSET_A; i < OFFSET_A + NUMBER_OF_A; ++i) {
            LongLongStringOid oid = new LongLongStringOid(i);
            a = em.find(LongLongStringPKOneMany.class, oid);
        }
        print("Finding " + NUMBER_OF_B + " instances of LongLongStringFKManyOne.");
        for (int i = OFFSET_B; i < OFFSET_B + NUMBER_OF_B; ++i) {
            LongLongStringOid oid = new LongLongStringOid(i);
            LongLongStringFKManyOne b = em.find(LongLongStringFKManyOne.class, oid);
            print(b.toString());
        }
        commit();
        em.close();

        /** Update every other instance of B to refer to a different A. */
        em = emf.createEntityManager();
        print("Finding 1 instance of A.");
        begin();
        LongLongStringOid oid = new LongLongStringOid(OFFSET_A);
        a = em.find(LongLongStringPKOneMany.class, oid);
        print("Finding 2 instances of B.");
        for (int i = OFFSET_B; i < OFFSET_B + NUMBER_OF_B; i += 2) {
            oid = new LongLongStringOid(i);
            LongLongStringFKManyOne b = em.find(LongLongStringFKManyOne.class, oid);
            // update every other one
            b.setLongLongStringPKRelationship(a);
            print(b.toString());
        }
        print("After update: " + a0.toString());
        commit();
        em.close();

        em = emf.createEntityManager();
        print("Finding " + NUMBER_OF_A + " instances of A.");
        begin();
        for (int i = OFFSET_A; i < OFFSET_A + NUMBER_OF_A; ++i) {
            oid = new LongLongStringOid(i);
            a = em.find(LongLongStringPKOneMany.class, oid);
            as.add(a);
            print(a.toString());
        }
        print("Finding " + NUMBER_OF_B + " instances of B.");
        for (int i = OFFSET_B; i < OFFSET_B + NUMBER_OF_B; ++i) {
            oid = new LongLongStringOid(i);
            LongLongStringFKManyOne b = em.find(LongLongStringFKManyOne.class, oid);
            print(b.toString());
            if (0 == i%2) {
                errorIfNotEqual("Mismatch in relationship longLongStringPKRelationship",
                        as.get(0), b.getLongLongStringPKOneMany());
                errorIfNotEqual("A.longLongStringFKRelationships should contain longLongStringFKRelationship",
                        true, as.get(0).getLongLongStringFKRelationships().contains(b));
            } else {
                errorIfNotEqual("Mismatch in relationship longLongStringPKRelationship",
                        as.get(1), b.getLongLongStringPKOneMany());
                errorIfNotEqual("A.longLongStringFKRelationships should contain longLongStringFKRelationship",
                        true, as.get(1).getLongLongStringFKRelationships().contains(b));
            }
        }
        commit();
        em.close();
        failOnError();
    }

    private void print(String string) {
        if (print) {
            System.out.println(string);
        }
    }

    /** Verify that the primary keys match the oid.
     *
     * @param oid the oid used to find the instance
     */
    public void verify(LongLongStringOid oid, LongLongStringPKOneMany instance) {
        errorIfNotEqual("Mismatch longpk1", oid.longpk1, instance.getLongpk1());
        errorIfNotEqual("Mismatch longpk2", oid.longpk2, instance.getLongpk2());
        errorIfNotEqual("Mismatch stringpk", oid.stringpk, instance.getStringpk());
    }

}
