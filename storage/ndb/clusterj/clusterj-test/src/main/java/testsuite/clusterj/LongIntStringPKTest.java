/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.
   Use is subject to license terms.

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

import java.util.ArrayList;
import java.util.List;

import testsuite.clusterj.model.LongIntStringPK;

public class LongIntStringPKTest extends AbstractClusterJTest {

    protected int PK_MODULUS = 2;
    protected int NUMBER_OF_INSTANCES = PK_MODULUS * PK_MODULUS * PK_MODULUS;
    protected long PRETTY_BIG_NUMBER = 1000000000000000L;
    protected List<LongIntStringPK> instances = new ArrayList<LongIntStringPK>();

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();
        try {
            tx.begin();
            session.deletePersistentAll(LongIntStringPK.class);
            tx.commit();
        } catch (Throwable t) {
            // ignore errors while deleting
        }
        createInstances();
        addTearDownClasses(LongIntStringPK.class);
    }

    public void test() {
        insert();
        find();
        update();
        delete();
        failOnError();
    }

    /** Insert all instances.
     */
    protected void insert() {
        session.makePersistentAll(instances);
    }

    /** Find all instances.
     */
    protected void find() {
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            Object[] key = new Object[]{getPK1(i), getPK2(i), getPK3(i)};
            LongIntStringPK result = session.find(LongIntStringPK.class, key);
            verify(result, i, false);
        }
    }

    /** Blind update every fourth instance.
     */
    protected void update() {
        // update the instances
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 4) {
                LongIntStringPK instance = createInstance(i);
                instance.setStringvalue(getValue(NUMBER_OF_INSTANCES - i));
                session.updatePersistent(instance);
                verify(instance, i, true);
            }
        }
        // verify the updated instances
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 4) {
                Object[] key = new Object[]{getPK1(i), getPK2(i), getPK3(i)};
                LongIntStringPK instance = session.find(LongIntStringPK.class, key);
                verify(instance, i, true);
            }
        }
    }

    /** Blind delete every fifth instance.
     */
    protected void delete() {
        // delete the instances
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 5) {
                LongIntStringPK instance = createInstance(i);
                session.deletePersistent(instance);
            }
        }
        // verify they have been deleted
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 5) {
                Object[] key = new Object[]{getPK1(i), getPK2(i), getPK3(i)};
                LongIntStringPK instance = session.find(LongIntStringPK.class, key);
                errorIfNotEqual("Failed to delete instance: " + i, null, instance);
            }
        }
    }

    /** The strategy for instances is for the "instance number" to create 
     * the three keys, such that the value of the instance is:
     * pk1 * PK_MODULUS^2 + pk2 * PK_MODULUS + pk3
     * 
     */
    protected void createInstances() {
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            LongIntStringPK instance = createInstance(i);
//            System.out.println(toString(instance));
            instances.add(instance);
        }
    }

    /** Create an instance of LongIntStringPK.
     * @param index the index to use to generate data
     * @return the instance
     */
    protected LongIntStringPK createInstance(int index) {
        LongIntStringPK instance = session.newInstance(LongIntStringPK.class);
        instance.setLongpk(getPK1(index));
        instance.setIntpk(getPK2(index));
        instance.setStringpk(getPK3(index));
        instance.setStringvalue(getValue(index));
        return instance;
    }

    protected String toString(LongIntStringPK instance) {
        StringBuffer result = new StringBuffer();
        result.append("LongIntStringPK[");
        result.append(instance.getLongpk());
        result.append(",");
        result.append(instance.getIntpk());
        result.append(",\"");
        result.append(instance.getStringpk());
        result.append("\"]: ");
        result.append(instance.getStringvalue());
        result.append(".");
        return result.toString();
    }

    protected long getPK1(int index) {
        return PRETTY_BIG_NUMBER * ((index / PK_MODULUS / PK_MODULUS) % PK_MODULUS);
    }

    protected int getPK2(int index) {
        return ((index / PK_MODULUS) % PK_MODULUS);
    }

    protected String getPK3(int index) {
        return "" + (index % PK_MODULUS);
    }

    protected String getValue(int index) {
        return "Value " + index;
    }

    protected void verify(LongIntStringPK instance, int index, boolean updated) {
        errorIfNotEqual("PK1 failed", getPK1(index), instance.getLongpk());
        errorIfNotEqual("PK2 failed", getPK2(index), instance.getIntpk());
        errorIfNotEqual("PK3 failed", getPK3(index), instance.getStringpk());
        if (updated) {
            errorIfNotEqual("Value failed", getValue(NUMBER_OF_INSTANCES - index), instance.getStringvalue());
        } else {
            errorIfNotEqual("Value failed", getValue(index), instance.getStringvalue());

        }
    }

}
