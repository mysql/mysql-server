/*
 *  Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import testsuite.clusterj.model.AutoPKInt;
import testsuite.clusterj.model.AutoPKBigint;
import testsuite.clusterj.model.AutoPKSmallint;
import testsuite.clusterj.model.AutoPKTinyint;

import testsuite.clusterj.model.BinaryPK;

public class AutoPKTest extends AbstractClusterJTest {

    protected interface Helper<KeyType> {
        KeyType valueOf(int i);
        KeyType valueOf(Number i);
        Class<KeyType> keyType();
    }
    
    protected Helper<Integer> intHelper = new Helper<Integer>() {
        public Integer valueOf(int i) {
            return new Integer(i);
        }
        public Integer valueOf(Number i) {
            return new Integer(i.intValue());
        }
        public Class<Integer> keyType() {
            return Integer.class;
        }
    };

    protected Helper<Long> bigintHelper = new Helper<Long>() {
        public Long valueOf(int i) {
            return new Long(i);
        }
        public Long valueOf(Number i) {
            return new Long(i.intValue());
        }
        public Class<Long> keyType() {
            return Long.class;
        }
    };

    protected Helper<Short> smallintHelper = new Helper<Short>() {
        public Short valueOf(int i) {
            return new Short((short)i);
        }
        public Short valueOf(Number i) {
            return new Short((short)i.intValue());
        }
        public Class<Short> keyType() {
            return Short.class;
        }
    };

    protected Helper<Byte> tinyintHelper = new Helper<Byte>() {
        public Byte valueOf(int i) {
            return new Byte((byte)i);
        }
        public Byte valueOf(Number i) {
            return new Byte((byte)i.intValue());
        }
        public Class<Byte> keyType() {
            return Byte.class;
        }
    };

    protected int NUMBER_OF_INSTANCES = 12;
    protected List<BinaryPK> instances = new ArrayList<BinaryPK>();

    protected Tester<Integer, AutoPKInt> intTester = new Tester<Integer, AutoPKInt>(intHelper, AutoPKInt.class);
    protected Tester<Long, AutoPKBigint> bigintTester = new Tester<Long, AutoPKBigint>(bigintHelper, AutoPKBigint.class);
    protected Tester<Short, AutoPKSmallint> smallintTester = new Tester<Short, AutoPKSmallint>(smallintHelper, AutoPKSmallint.class);
    protected Tester<Byte, AutoPKTinyint> tinyintTester = new Tester<Byte, AutoPKTinyint>(tinyintHelper, AutoPKTinyint.class);

    protected class Tester<KeyType extends Number, InstanceType> {
        Helper<KeyType> helper;
        Class<InstanceType> theInstanceType;
        Class<KeyType> theKeyType;
        List<InstanceType> instances = new ArrayList<InstanceType>();
        List<KeyType> ids = new ArrayList<KeyType>();
        Method setVal = null;
        Method getVal = null;
        Method getId = null;
        Tester(Helper<KeyType> helper, Class<InstanceType> instanceType) {
            this.helper = helper;
            this.theKeyType = helper.keyType();
            this.theInstanceType = instanceType;
            // get Method setVal
            try {
                setVal = theInstanceType.getMethod("setVal", theKeyType);
            } catch (Exception e) {
                error ("Failed to get Method instance for " + theInstanceType.getName() + ".setVal.");
            }
            // get Method getVal
            try {
                getVal = theInstanceType.getMethod("getVal");
            } catch (Exception e) {
                error ("Failed to get Method instance for " + theInstanceType.getName() + ".getVal.");
            }
            // get Method setVal
            try {
                getId = theInstanceType.getMethod("getId");
            } catch (Exception e) {
                error ("Failed to get Method instance for " + theInstanceType.getName() + ".getId.");
            }
        }
       InstanceType newInstance() {
            return session.newInstance(theInstanceType);
        }
       void deleteAll() {
           try {
               session.deletePersistentAll(theInstanceType);
           } catch (Exception e) {
               // ignore errors while deleting
           }
       }
       void deleteByKey(int i) {
           if (ids.size() < i + 1) {
               error("deleteByKey: no instance was created for " + theInstanceType.getName() + " for " + i);
           } else {
               try {
                   session.deletePersistent(theInstanceType, ids.get(i));
               } catch (Exception e) {
                   error("deleteByKey: caught " + e.getMessage() + " for " + theInstanceType.getName() + "  " + i);
               }
           }
       }
        void createAll() {
            for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
                InstanceType instance = newInstance();
                if (instance == null) {
                    error("createAll: instance was null for session.newInstance(" + theInstanceType.getName() + ".class)  " + i);
                } else {
                    try {
                        setVal.invoke(instance, helper.valueOf(i));
                    } catch (Exception e) {
                        error("createAll: setVal.invoke caught " + e.getMessage() + " for " + theInstanceType.getName() + "  " + i);
                    }
                    try {
                        instances.add(instance);
                    } catch (Exception e) {
                        error("createAll: instances.add caught " + e.getMessage() + " for " + theInstanceType.getName() + "  " + i);
                    }
                    try {
                        session.makePersistent(instance);
                    } catch (Exception e) {
                        error("createAll: caught exception " + e.getMessage() + " for " + theInstanceType.getName() + "  " + i);
                    }
                    try {
                        KeyType id = helper.valueOf((Number)getId.invoke(instance));
//                        System.out.println("createAll: created key: "  + id + " for " + theInstanceType.getName() + "  " + i);
                        // verify that the id has not already been used
                        if (ids.contains(id)) {
                            error("createAll: duplicate key for " + theInstanceType.getName() + "  " + i);
                        } else {
                            ids.add(id);                            
                        }
                    } catch (Exception e) {
                        error("createAll: getId.invoke caught " + e.getMessage() + " for " + theInstanceType.getName() + "  " + i);
                    }
                }
            }
        }
        void findAll() {
            for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
                if (ids.size() < i + 1) {
                    error("findAll: no instance was created for " + theInstanceType.getName() + " for " + i);
                } else {
                    InstanceType found = session.find(theInstanceType, ids.get(i));
                    if (found == null) {
                        error("findAll: instance is null for session.find(" + theInstanceType.getName() + ".class,  " + i + ")");
                    } else {
                        try {
                            Object val = getVal.invoke(found);
                            Object key = getId.invoke(found);
                            errorIfNotEqual("Error comparing key for " + theInstanceType.getName() + " for " + i, ids.get(i), key);
                            errorIfNotEqual("Error comparing val for " + i, helper.valueOf(i), val);
                        } catch (Exception e) {
                            error("findAll: caught exception " + e.getMessage() + " for " + theInstanceType.getName() + ".");
                        }
                    }
                }
            }
        }
        InstanceType findByKey(int i) {
            if (ids.size() < i + 1) {
                error("findByKey: no instance was created for " + theInstanceType.getName() + " for " + i);
                return null;
            } else {
                InstanceType found = session.find(theInstanceType, ids.get(i));
                return found;
            }
        }
    }


    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        try {
            intTester.deleteAll();
            bigintTester.deleteAll();
            smallintTester.deleteAll();
            tinyintTester.deleteAll();
        } catch (Throwable t) {
            t.printStackTrace();
            // ignore errors while deleting
        }
        try {
            intTester.createAll();
            bigintTester.createAll();
            smallintTester.createAll();
            tinyintTester.createAll();
        } catch (Throwable t) {
            t.printStackTrace();
        }
        addTearDownClasses(AutoPKInt.class, AutoPKBigint.class, AutoPKSmallint.class, AutoPKTinyint.class);
    }

    public void test() {
        find();
        delete();
        failOnError();
    }

    /** Find all instances.
     */
    protected void find() {
        intTester.findAll();
        bigintTester.findAll();
        smallintTester.findAll();
        tinyintTester.findAll();
    }

    protected void delete() {
        // delete every fifth instance by key
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            if (0 == i % 5) {
                intTester.deleteByKey(i);
                bigintTester.deleteByKey(i);
                smallintTester.deleteByKey(i);
                tinyintTester.deleteByKey(i);
                
            }
        }
        // verify they have been deleted
        for (int i = 0; i < NUMBER_OF_INSTANCES; ++i) {
            Object found = null;
            if (0 == i % 5) {
                found = intTester.findByKey(i);
                if (found != null) {
                    error ("failed to delete AutoPKInt for " + i);
                }
                found = bigintTester.findByKey(i);
                if (found != null) {
                    error ("failed to delete AutoPKBigint for " + i);
                }
                found = smallintTester.findByKey(i);
                if (found != null) {
                    error ("failed to delete AutoPKSmallint for " + i);
                }
                found = tinyintTester.findByKey(i);
                if (found != null) {
                    error ("failed to delete AutoPKTinyint for " + i);
                }                
            }
        }
    }

}
