/*
 *  Copyright (c) 2023, Oracle and/or its affiliates.
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

import com.mysql.clusterj.DynamicObject;
import testsuite.clusterj.model.DynamicStringPKs;

public class DynamicStringPKTest extends AbstractClusterJTest {

    protected int NUMBER_OF_INSTANCES = 15;
    protected List<DynamicStringPKs> instances = new ArrayList<DynamicStringPKs>();

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();
    }

    public void test() {
        run(DynamicStringPK.class);
        failOnError();
    }

    public void run(Class<? extends DynamicStringPKs> cls) {
        deleteAll(cls);
        createInstances(cls);
        insert();
        find(cls);
        update(cls);
        delete(cls);
    }

    public static class DynamicStringPK extends DynamicStringPKs {
        @Override
        public String table() {
            return "dynamicstringpks";
        }
    }

    protected void deleteAll(Class<? extends DynamicStringPKs> cls) {
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
    protected void find(Class<? extends DynamicStringPKs> cls) {
        String where = "find " + cls.getName();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            tx.begin();
            String key = getPK(i);
            DynamicStringPKs dynObj = (DynamicStringPKs) session.newInstance(cls);
            dynObj.setKey1(key);
            dynObj.setKey2(key);
            dynObj.setKey3(key);
            dynObj.setKey4(i);
            dynObj.setKey5(key);
            dynObj.setKey6(i);
            dynObj.setKey7(key);
            DynamicStringPKs result = session.load(dynObj);
            session.flush();
            tx.commit();
            verify(where, result, i, false);
        }
    }

    /** Blind update every fourth instance.
     */
    protected void update(Class<? extends DynamicStringPKs> cls) {
        // update the instances
        String where = "update before " + cls.getName();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 4) {
                DynamicStringPKs instance = createInstance(cls, i);
                instance.setName(getValue(NUMBER_OF_INSTANCES - i));
                session.savePersistent(instance);
                verify(where, instance, i, true);
            }
        }
        // verify the updated instances
        where = "update after " + cls.getName();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 4) {
                tx.begin();
                String key = getPK(i);
                DynamicStringPKs dynObj = (DynamicStringPKs) session.newInstance(cls);
                dynObj.setKey1(key);
                dynObj.setKey2(key);
                dynObj.setKey3(key);
                dynObj.setKey4(i);
                dynObj.setKey5(key);
                dynObj.setKey6(i);
                dynObj.setKey7(key);
                DynamicStringPKs result = session.load(dynObj);
                session.flush();
                tx.commit();
                verify(where, result, i, true);
            }
        }
    }

    /** Blind delete every fifth instance.
     */
    protected void delete(Class<? extends DynamicStringPKs> cls) {
        // delete the instances
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 5) {
                DynamicStringPKs instance = createInstance(cls, i);
                session.deletePersistent(instance);
            }
        }
        // verify they have been deleted
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 5) {
                tx.begin();
                String key = getPK(i);
                DynamicStringPKs dynObj = (DynamicStringPKs) session.newInstance(cls);
                dynObj.setKey1(key);
                dynObj.setKey2(key);
                dynObj.setKey3(key);
                dynObj.setKey4(i);
                dynObj.setKey5(key);
                dynObj.setKey6(i);
                dynObj.setKey7(key);
                DynamicStringPKs result = session.load(dynObj);
                session.flush();
                tx.commit();
                Boolean found_object = session.found(dynObj);
                if (!found_object || found_object == null)
                {
                  result = null;
                }
                errorIfNotEqual("Failed to delete instance: " + i, null, result);
            }
        }
    }

    protected void createInstances(Class<? extends DynamicStringPKs> cls) {
        instances.clear();
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            DynamicStringPKs instance = createInstance(cls, i);
            if (getDebug()) System.out.println(toString(instance));
            instances.add(instance);
        }
    }

    /** Create an instance of DynamicStringPKs.
     * @param index the index to use to generate data
     * @return the instance
     */
    protected DynamicStringPKs createInstance(Class<?extends DynamicStringPKs> cls, int index) {
        DynamicStringPKs instance = session.newInstance(cls);
        instance.setKey1(getPK(index));
        instance.setKey2(getPK(index));
        instance.setKey3(getPK(index));
        instance.setKey4(index);
        instance.setKey5(getPK(index));
        instance.setKey6(index);
        instance.setKey7(getPK(index));
        instance.setNumber(index);
        instance.setName(getValue(index));
        return instance;
    }

    protected String toString(DynamicStringPKs instance) {
        StringBuffer result = new StringBuffer(instance.getClass().getName());
        result.append("[");
        result.append(instance.getKey1());
        result.append(", \"");
        result.append(instance.getKey2());
        result.append(", \"");
        result.append(instance.getKey3());
        result.append(", \"");
        result.append(instance.getKey4());
        result.append(", \"");
        result.append(instance.getKey5());
        result.append(", \"");
        result.append(instance.getKey6());
        result.append(", \"");
        result.append(instance.getKey7());
        result.append("]: ");
        result.append(instance.getNumber());
        result.append(", \"");
        result.append(instance.getName());
        result.append("\".");
        return result.toString();
    }

    protected String getPK(int index) {
        String result = "Text............. " + index;
        return result;
    }

    protected String getValue(int index) {
        return "Value " + index;
    }

    protected void verify(String where, DynamicStringPKs instance, int index, boolean updated) {
        errorIfNotEqual(where + "id failed", getPK(index), instance.getKey1());
        errorIfNotEqual(where + "number failed", index, instance.getNumber());
        if (updated) {
            errorIfNotEqual(where + " Value failed", getValue(NUMBER_OF_INSTANCES - index), instance.getName());
        } else {
            errorIfNotEqual(where + " Value failed", getValue(index), instance.getName());

        }
    }
}
