/*
 *  Copyright (c) 2011, 2022, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import java.util.ArrayList;
import java.util.List;

import testsuite.clusterj.model.BinaryPK;
import testsuite.clusterj.model.DynamicPK;

public class DynamicBinaryPKTest extends AbstractClusterJTest {

    protected int NUMBER_OF_INSTANCES = 15;
    protected List<DynamicPK> instances = new ArrayList<DynamicPK>();

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();
    }

    public void test() {
        run(DynamicBinaryPK.class);
        run(DynamicVarbinaryPK.class);
        run(DynamicLongvarbinaryPK.class);
        failOnError();
    }

    public void run(Class<? extends DynamicPK> cls) {
        deleteAll(cls);
        createInstances(cls);
        insert();
        find(cls);
        update(cls);
        delete(cls);
    }

    public static class DynamicBinaryPK extends DynamicPK {
        @Override
        public String table() {
            return "binarypk";
        }
    }

    public static class DynamicVarbinaryPK extends DynamicPK {
        @Override
        public String table() {
            return "varbinarypk";
        }
    }

    public static class DynamicLongvarbinaryPK extends DynamicPK {
        @Override
        public String table() {
            return "longvarbinarypk";
        }
    }

    protected void deleteAll(Class<? extends DynamicPK> cls) {
        try {
            tx.begin();
            session.deletePersistentAll(cls);
            tx.commit();
        } catch (Throwable t) {
            // ignore errors while deleting
        }
    }

    /** Insert all instances.
     */
    protected void insert() {
        session.makePersistentAll(instances);
    }

    /** Find all instances.
     */
    protected void find(Class<? extends DynamicPK> cls) {
        String where = "find " + cls.getName();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            byte[] key = getPK(i);
            DynamicPK result = session.find(cls, key);
            verify(where, result, i, false);
        }
    }

    /** Blind update every fourth instance.
     */
    protected void update(Class<? extends DynamicPK> cls) {
        // update the instances
        String where = "update before " + cls.getName();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 4) {
                DynamicPK instance = createInstance(cls, i);
                instance.setName(getValue(NUMBER_OF_INSTANCES - i));
                session.updatePersistent(instance);
                verify(where, instance, i, true);
            }
        }
        // verify the updated instances
        where = "update after " + cls.getName();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 4) {
                byte[] key = getPK(i);
                DynamicPK instance = session.find(cls, key);
                verify(where, instance, i, true);
            }
        }
    }

    /** Blind delete every fifth instance.
     */
    protected void delete(Class<? extends DynamicPK> cls) {
        // delete the instances
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 5) {
                DynamicPK instance = createInstance(cls, i);
                session.deletePersistent(instance);
            }
        }
        // verify they have been deleted
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 5) {
                byte[] key = getPK(i);
                DynamicPK instance = session.find(cls, key);
                errorIfNotEqual("Failed to delete instance: " + i, null, instance);
            }
        }
    }

    /** The strategy for instances is for the "instance number" to create 
     * the keys by creating a byte[] with the encoded number.
     */
    protected void createInstances(Class<? extends DynamicPK> cls) {
        instances.clear();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            DynamicPK instance = createInstance(cls, i);
            if (getDebug()) System.out.println(toString(instance));
            instances.add(instance);
        }
    }

    /** Create an instance of DynamicPK.
     * @param index the index to use to generate data
     * @return the instance
     */
    protected DynamicPK createInstance(Class<?extends DynamicPK> cls, int index) {
        DynamicPK instance = session.newInstance(cls);
        instance.setId(getPK(index));
        instance.setNumber(index);
        instance.setName(getValue(index));
        return instance;
    }

    protected String toString(DynamicPK instance) {
        StringBuffer result = new StringBuffer(instance.getClass().getName());
        result.append("[");
        result.append(toString(instance.getId()));
        result.append("]: ");
        result.append(instance.getNumber());
        result.append(", \"");
        result.append(instance.getName());
        result.append("\".");
        return result.toString();
    }

    protected byte[] getPK(int index) {
        byte[] result = new byte[255];
        result[1] = (byte)(index/256);
        result[2] = (byte)(index%256);
        return result;
    }

    protected String getValue(int index) {
        return "Value " + index;
    }

    protected void verify(String where, DynamicPK instance, int index, boolean updated) {
        errorIfNotEqual(where + "id failed", toString(getPK(index)), toString(instance.getId()));
        errorIfNotEqual(where + "number failed", index, instance.getNumber());
        if (updated) {
            errorIfNotEqual(where + " Value failed", getValue(NUMBER_OF_INSTANCES - index), instance.getName());
        } else {
            errorIfNotEqual(where + " Value failed", getValue(index), instance.getName());

        }
    }

    private String toString(byte[] id) {
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < id.length; ++i) {
            builder.append(String.valueOf(id[i]));
            builder.append('-');
        }
        return builder.toString();
    }

}
