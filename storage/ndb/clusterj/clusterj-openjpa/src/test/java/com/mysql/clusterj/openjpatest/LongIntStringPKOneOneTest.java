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

import com.mysql.clusterj.jpatest.model.LongIntStringFKOneOne;
import com.mysql.clusterj.jpatest.model.LongIntStringPKOneOne;
import com.mysql.clusterj.jpatest.model.LongIntStringOid;

/**
 *
 */
public class LongIntStringPKOneOneTest extends AbstractJPABaseTest {

    private int NUMBER_OF_INSTANCES = 5;
    private int OFFSET_A = 100;
    private int OFFSET_B = 10;

    private LongIntStringPKOneOne[] as = new LongIntStringPKOneOne[NUMBER_OF_INSTANCES];
    private LongIntStringFKOneOne[] bs = new LongIntStringFKOneOne[NUMBER_OF_INSTANCES];

    // set this to true for debug output
    private boolean print = false;

    @Override
    public void setUp() {
        super.setUp();
    }

    @Override
    protected String getPersistenceUnitName() {
        return "ndb";
//      return "jdbc";
    }

    /** This tests delete, insert, find, and update of entities with compound
     * primary and foreign keys.
     */
    public void test() {
        LongIntStringPKOneOne a;
        LongIntStringFKOneOne b;
        em = emf.createEntityManager();
        begin();
        // bulk remove all LongLongStringFKManyOne
        int countB = em.createQuery("DELETE FROM LongIntStringFKOneOne").executeUpdate();
        int countA = em.createQuery("DELETE FROM LongIntStringPKOneOne").executeUpdate();
        print ("Deleted " + countB + " instances of LongLongStringFKManyOne " +
                countA + " instances of LongLongStringFKManyOne ");
        commit();
        em.close();

        em = emf.createEntityManager();
        begin();
        print("Creating " + NUMBER_OF_INSTANCES + " instances of LongIntStringPKOneOne and LongIntStringFKOneOne.");
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            a = as[i] = LongIntStringPKOneOne.create(OFFSET_A + i);
            b = bs[i] = LongIntStringFKOneOne.create(OFFSET_B + i);
            a.setLongIntStringFKOneOne(b);
            b.setLongIntStringPKOneOne(a);
            em.persist(a);
            print(a.toString());
            em.persist(b);
            print(b.toString());
        }
        commit();
        em.close();

        em = emf.createEntityManager();
        begin();
        print("Checking " + NUMBER_OF_INSTANCES + " instances.");
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            b = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + i));
            a = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + i));
            errorIfNull("Instance of LongIntStringPKOneOne for value " + (OFFSET_A + i)  + " was not found.", a);
            errorIfNull("Instance of LongIntStringFKOneOne for value " + (OFFSET_B + i)  + " was not found.", b);
            if (b != null && a != null) {
                errorIfNotEqual("Mismatch in longIntStringFKOneOne.getLongIntStringPKOneOne",
                        a, b.getLongIntStringPKOneOne());
                errorIfNotEqual("Mismatch in longIntStringPKOneOne.getLongIntStringFKOneOne",
                        b, a.getLongIntStringFKOneOne());
            }
        }
        commit();
        em.close();

        em = emf.createEntityManager();
        print("Deleting some instances.");
        begin();
        // delete a0; set b0.a = null
        a = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + 0));
        b = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + 0));
        b.setLongIntStringPKOneOne(null);
        em.remove(a);
        a = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + 1));
        b = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + 1));
        a.setLongIntStringFKOneOne(null);
        em.remove(b);
        commit();
        em.close();

        em = emf.createEntityManager();
        begin();
        print("Checking deleted instances.");
        a = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + 0));
        errorIfNotEqual("finding deleted instance of LongIntStringPK should return null", null, a);
        b = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + 1));
        errorIfNotEqual("finding deleted instance of LongIntStringFK should return null", null, b);
        a = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + 1));
        b = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + 0));
        errorIfNull("Instance of LongIntStringPKOneOne for value " + (OFFSET_A + 1)  + " was not found.", a);
        errorIfNull("Instance of LongIntStringFKOneOne for value " + (OFFSET_B + 0)  + " was not found.", b);
        if (b != null && a != null) {
            errorIfNotEqual("field longIntStringFKOneOne should be null",
                    null, a.getLongIntStringFKOneOne());
            errorIfNotEqual("field longIntStringPKOneOne should be null",
                    null, b.getLongIntStringPKOneOne());
        }
        commit();
        em.close();

        em = emf.createEntityManager();
        begin();
        print("Setting fields to null");
        a = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + 2));
        b = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + 2));
        b.setLongIntStringPKOneOne(null);
        a.setLongIntStringFKOneOne(null);
        commit();
        em.close();

        em = emf.createEntityManager();
        begin();
        print("Checking fields set to null.");
        a = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + 2));
        b = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + 2));
        if (b != null && a != null) {
            errorIfNotEqual("field longIntStringFKOneOne should be null",
                    null, a.getLongIntStringFKOneOne());
            errorIfNotEqual("field longIntStringPKOneOne should be null",
                    null, b.getLongIntStringPKOneOne());
        }
        commit();
        em.close();

        em = emf.createEntityManager();
        begin();
        print("Swapping references.");
        LongIntStringPKOneOne a3 = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + 3));
        LongIntStringPKOneOne a4 = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + 4));
        LongIntStringFKOneOne b3 = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + 3));
        LongIntStringFKOneOne b4 = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + 4));
        a3.setLongIntStringFKOneOne(b4);
        b4.setLongIntStringPKOneOne(a3);
        a4.setLongIntStringFKOneOne(b3);
        b3.setLongIntStringPKOneOne(a4);
        errorIfNotEqual("Swapped field b4.longIntStringPKOneOne should be a3",
                a3, b4.getLongIntStringPKOneOne());
        errorIfNotEqual("Swapped field a3.longIntStringFKOneOne should be b4",
                b4, a3.getLongIntStringFKOneOne());
        errorIfNotEqual("Swapped field b3.longIntStringPKOneOne should be a4",
                a4, b3.getLongIntStringPKOneOne());
        errorIfNotEqual("Swapped field a4.longIntStringFKOneOne should be b3",
                b3, a4.getLongIntStringFKOneOne());
        commit();
        em.close();

        em = emf.createEntityManager();
        begin();
        print("Checking swapped references.");
        a3 = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + 3));
        a4 = em.find(LongIntStringPKOneOne.class, new LongIntStringOid(OFFSET_A + 4));
        b3 = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + 3));
        b4 = em.find(LongIntStringFKOneOne.class, new LongIntStringOid(OFFSET_B + 4));
        errorIfNotEqual("Swapped field b4.longIntStringPKOneOne should be a3",
                a3, b4.getLongIntStringPKOneOne());
        errorIfNotEqual("Swapped field a3.longIntStringFKOneOne should be b4",
                b4, a3.getLongIntStringFKOneOne());
        errorIfNotEqual("Swapped field b3.longIntStringPKOneOne should be a4",
                a4, b3.getLongIntStringPKOneOne());
        errorIfNotEqual("Swapped field a4.longIntStringFKOneOne should be b3",
                b3, a4.getLongIntStringFKOneOne());
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
    public void verify(LongIntStringOid oid, LongIntStringPKOneOne instance) {
        errorIfNotEqual("Mismatch longpk", oid.longpk, instance.getLongpk());
        errorIfNotEqual("Mismatch intpk", oid.intpk, instance.getIntpk());
        errorIfNotEqual("Mismatch stringpk", oid.stringpk, instance.getStringpk());
    }

}
