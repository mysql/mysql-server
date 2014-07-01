/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.jpatest;

import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.jpatest.model.ShortPK;

public class ShortPKTest extends AbstractJPABaseTest {

    /** Number of instances */
    protected short NUMBER_TO_INSERT = 10;

    /** The blob instances for testing. */
    protected List<ShortPK> instances = new ArrayList<ShortPK>();

    /** Subclasses can override this method to get debugging info printed to System.out */
    protected boolean getDebug() {
        return false;
    }

    public void test() {
        createInstances(NUMBER_TO_INSERT);
        remove();
        insert();
        update();
        failOnError();
    }

    protected void remove() {
        removeAll(ShortPK.class);
    }

    protected void insert() {
        // insert instances
        tx = em.getTransaction();
        tx.begin();
        
        int count = 0;

        for (short i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            em.persist(instances.get(i));
            ++count;
        }
        tx.commit();
    }

    protected void update() {

        tx.begin();

        for (short i = 0; i < 10; ++i) {
            // must be done with an active transaction
            ShortPK e = em.find(ShortPK.class, i);
            // see if it is the right one
            short actualId = e.getId();
            if (actualId != i) {
                error("Expected ShortPK.id " + i + " but got " + actualId);
            }
            e.setShort_null_none((short)(i + 10));
            e.setShort_null_btree((short)(i + 10));
            e.setShort_null_hash((short)(i + 10));
            e.setShort_null_both((short)(i + 10));
        }
        tx.commit();
        tx.begin();

        for (short i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            ShortPK e = em.find(ShortPK.class, i);
            // see if it is the right one
            short actualId = e.getId();
            if (actualId != i) {
                error("Expected BlobTypes.id " + i + " but got " + actualId);
            }
            // check to see that the fields have the right data
            errorIfNotEqual("Mismatch in short_null_none", (short)(i + 10), e.getShort_null_none());
            errorIfNotEqual("Mismatch in short_null_btree", (short)(i + 10), e.getShort_null_btree());
            errorIfNotEqual("Mismatch in short_null_hash", (short)(i + 10), e.getShort_null_hash());
            errorIfNotEqual("Mismatch in short_null_both", (short)(i + 10), e.getShort_null_both());
        }
        tx.commit();
    }

    protected void createInstances(int number) {
        for (short i = 0; i < number; ++i) {
            ShortPK instance = new ShortPK();
            instance.setId(i);
            instance.setShort_null_none(i);
            instance.setShort_null_btree(i);
            instance.setShort_null_hash(i);
            instance.setShort_null_both(i);
            instances.add(instance);
        }
    }

}
