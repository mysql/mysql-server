/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

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

package com.mysql.clusterj.openjpatest;

import com.mysql.clusterj.jpatest.AbstractJPABaseTest;
import com.mysql.clusterj.jpatest.model.LongLongStringPK;
import com.mysql.clusterj.jpatest.model.LongLongStringOid;

/**
 *
 */
public class LongLongStringPKTest extends AbstractJPABaseTest {

    private int NUMBER_OF_A = 2;
    private int OFFSET_A = 100;
    // set this to true for debug output
    private boolean print = false;

    @Override
    public void setUp() {
        super.setUp();
    }

    public void test() {
        LongLongStringPK a;
        em = emf.createEntityManager();
        print("Removing " + NUMBER_OF_A + " instances of LongLongStringPK.");
        begin();
        for (int i = OFFSET_A; i < OFFSET_A + NUMBER_OF_A; ++i) {
            LongLongStringOid oid = new LongLongStringOid(i);
            a = em.find(LongLongStringPK.class, oid);
            if (a != null) {
                verify(oid, a);
                em.remove(a);
            }
        }
        commit();
        em.close();

        em = emf.createEntityManager();
        begin();
        print("Creating " + NUMBER_OF_A + " instances of LongLongStringPK.");
        for (int i = OFFSET_A; i < OFFSET_A + NUMBER_OF_A; ++i) {
            a = LongLongStringPK.create(i);
            em.persist(a);
        }
        commit();
        em.close();

        em = emf.createEntityManager();
        print("Finding " + NUMBER_OF_A + " instances of LongLongStringPK.");
        begin();
        for (int i = OFFSET_A; i < OFFSET_A + NUMBER_OF_A; ++i) {
            LongLongStringOid oid = new LongLongStringOid(i);
            a = em.find(LongLongStringPK.class, oid);
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
    public void verify(LongLongStringOid oid, LongLongStringPK instance) {
        errorIfNotEqual("Mismatch longpk1", oid.longpk1, instance.getLongpk1());
        errorIfNotEqual("Mismatch longpk2", oid.longpk2, instance.getLongpk2());
        errorIfNotEqual("Mismatch stringpk", oid.stringpk, instance.getStringpk());
    }

}
