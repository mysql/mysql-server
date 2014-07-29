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

import com.mysql.clusterj.jpatest.model.BytePK;

public class BytePKTest extends AbstractJPABaseTest {

    /** Number of instances */
    protected byte NUMBER_TO_INSERT = 10;

    /** The blob instances for testing. */
    protected List<BytePK> instances = new ArrayList<BytePK>();

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
        removeAll(BytePK.class);
    }

    protected void insert() {
        // insert instances
        tx = em.getTransaction();
        tx.begin();
        
        int count = 0;

        for (byte i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            em.persist(instances.get(i));
            ++count;
        }
        tx.commit();
    }

    protected void update() {

        tx.begin();

        for (byte i = 0; i < 10; ++i) {
            // must be done with an active transaction
            BytePK e = em.find(BytePK.class, i);
            // see if it is the right one
            byte actualId = e.getId();
            if (actualId != i) {
                error("Expected BytePK.id " + i + " but got " + actualId);
            }
            e.setByte_null_none((byte)(i + 10));
            e.setByte_null_btree((byte)(i + 10));
            e.setByte_null_hash((byte)(i + 10));
            e.setByte_null_both((byte)(i + 10));
        }
        tx.commit();
        tx.begin();

        for (byte i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            BytePK e = em.find(BytePK.class, i);
            // see if it is the right one
            byte actualId = e.getId();
            if (actualId != i) {
                error("Expected BlobTypes.id " + i + " but got " + actualId);
            }
            // check to see that the fields have the right data
            errorIfNotEqual("Mismatch in byte_null_none", (byte)(i + 10), e.getByte_null_none());
            errorIfNotEqual("Mismatch in byte_null_btree", (byte)(i + 10), e.getByte_null_btree());
            errorIfNotEqual("Mismatch in byte_null_hash", (byte)(i + 10), e.getByte_null_hash());
            errorIfNotEqual("Mismatch in byte_null_both", (byte)(i + 10), e.getByte_null_both());
        }
        tx.commit();
    }

    protected void createInstances(int number) {
        for (byte i = 0; i < number; ++i) {
            BytePK instance = new BytePK();
            instance.setId(i);
            instance.setByte_null_none(i);
            instance.setByte_null_btree(i);
            instance.setByte_null_hash(i);
            instance.setByte_null_both(i);
            instances.add(instance);
        }
    }

}
