/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

package testsuite.clusterj;

import java.nio.ByteBuffer;

import org.junit.Ignore;

import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ColumnMetadata;
import com.mysql.clusterj.DynamicObject;

import testsuite.clusterj.AbstractClusterJModelTest;
import testsuite.clusterj.model.IdBase;

@Ignore
public class StressTest extends AbstractClusterJModelTest {

    static protected final Runtime rt = Runtime.getRuntime();

    private static final int NUMBER_TO_INSERT = 4000;

    private static final int ITERATIONS = 7;

    private static final int ITERATIONS_TO_DROP = 3;

    private static String tableName;

    private static final String STRESS_TEST_TABLE_PROPERTY_NAME = "com.mysql.clusterj.StressTestTable";

    static {
        String env = System.getenv(STRESS_TEST_TABLE_PROPERTY_NAME);
        String def = (env == null)?"stress":env;
        tableName = System.getProperty(STRESS_TEST_TABLE_PROPERTY_NAME, def);
    }

    private ColumnMetadata[] columnMetadatas;

    private Timer timer = new Timer();

    private static int BYTES_LENGTH = 12000;

    private static ByteBuffer BYTES = ByteBuffer.allocate(BYTES_LENGTH);

    static {
        for (int i = 0; i < BYTES_LENGTH; ++i) {
            // only printable bytes from ABC..^_`
            BYTES.put((byte)((i % 32) + 65));
        }
    }

    private static int STRING_LENGTH = 12000;

    private static String STRING;

    static {
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < STRING_LENGTH; ++i) {
            // only printable bytes from ABC..^_`
            builder.append((byte)((i % 32) + 65));
        }
        STRING = builder.toString();
    }

    @Override
    java.lang.Class<? extends IdBase> getModelClass() {
        return Stress.class;
    }

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();
        session.deletePersistentAll(Stress.class);
        columnMetadatas = session.newInstance(Stress.class).columnMetadata();
    }

    public void testIndy() {
        insAattr_indy();
        getA_indy();
        delA_indy();
    }

    public void testEach() {
        insAattr_each();
        getA_each();
        delA_each();
    }

    public void testBulk() {
        insAattr_bulk();
        getA_bulk();
        delA_bulk();
    }

    public void insAattr_indy() {
        long total = 0;
        for (int i = 0; i < ITERATIONS; ++i) {
            // garbage collect what we can before each test
            gc();
            timer.start();
            for (int key = 0; key < NUMBER_TO_INSERT; ++key) {
                Stress instance = createObject(key);
                session.makePersistent(instance);
            }
            // drop the first iteration
            timer.stop();
            if (i >= ITERATIONS_TO_DROP) total += timer.time();
            System.out.println("insAattr_indy: " + timer.time());
        }
        System.out.println("Excluding " + ITERATIONS_TO_DROP + " Average: " + total/(ITERATIONS - ITERATIONS_TO_DROP) + "\n");
    }

    public void insAattr_each() {
        long total = 0;
        for (int i = 0; i < ITERATIONS; ++i) {
            // garbage collect what we can before each test
            gc();
            timer.start();
            session.currentTransaction().begin();
            for (int key = 0; key < NUMBER_TO_INSERT; ++key) {
                Stress instance = createObject(key);
                session.makePersistent(instance);
                session.flush();
            }
            session.currentTransaction().commit();
            // drop the first iteration
            timer.stop();
            if (i >= ITERATIONS_TO_DROP) total += timer.time();
            System.out.println("insAattr_each: " + timer.time());
        }
        System.out.println("Excluding " + ITERATIONS_TO_DROP + " Average: " + total/(ITERATIONS - ITERATIONS_TO_DROP) + "\n");
    }

    public void insAattr_bulk() {
        long total = 0;
        for (int i = 0; i < ITERATIONS; ++i) {
            // garbage collect what we can before each test
            gc();
            timer.start();
            session.currentTransaction().begin();
            for (int key = 0; key < NUMBER_TO_INSERT; ++key) {
                Stress instance = createObject(key);
                session.makePersistent(instance);
            }
            session.currentTransaction().commit();
            // drop the first iteration
            timer.stop();
            if (i >= ITERATIONS_TO_DROP) total += timer.time();
            System.out.println("insAattr_bulk: " + timer.time());
        }
        System.out.println("Excluding " + ITERATIONS_TO_DROP + " Average: " + total/(ITERATIONS - ITERATIONS_TO_DROP) + "\n");
    }

    public void getA_indy() {
        long total = 0;
        for (int i = 0; i < ITERATIONS; ++i) {
            // garbage collect what we can before each test
            gc();
            timer.start();
            for (int key = 0; key < NUMBER_TO_INSERT; ++key) {
                session.find(Stress.class, key);
            }
            // drop the first iteration
            timer.stop();
            if (i >= ITERATIONS_TO_DROP) total += timer.time();
            System.out.println("getA_indy: " + timer.time());
        }
        System.out.println("Excluding " + ITERATIONS_TO_DROP + " Average: " + total/(ITERATIONS - ITERATIONS_TO_DROP) + "\n");
    }

    public void getA_each() {
        long total = 0;
        for (int i = 0; i < ITERATIONS; ++i) {
            // garbage collect what we can before each test
            gc();
            timer.start();
            session.currentTransaction().begin();
            for (int key = 0; key < NUMBER_TO_INSERT; ++key) {
                session.find(Stress.class, key);
            }
            session.currentTransaction().commit();
            // drop the first iteration
            timer.stop();
            if (i >= ITERATIONS_TO_DROP) total += timer.time();
            System.out.println("getA_each: " + timer.time());
        }
        System.out.println("Excluding " + ITERATIONS_TO_DROP + " Average: " + total/(ITERATIONS - ITERATIONS_TO_DROP) + "\n");
    }

    public void getA_bulk() {
        long total = 0;
        for (int i = 0; i < ITERATIONS; ++i) {
            // garbage collect what we can before each test
            gc();
            timer.start();
            session.currentTransaction().begin();
            for (int key = 0; key < NUMBER_TO_INSERT; ++key) {
                Stress instance = createObject(key);
                session.load(instance);
            }
            session.currentTransaction().commit();
            // drop the first iteration
            timer.stop();
            if (i >= ITERATIONS_TO_DROP) total += timer.time();
            System.out.println("getA_bulk: " + timer.time());
        }
        System.out.println("Excluding " + ITERATIONS_TO_DROP + " Average: " + total/(ITERATIONS - ITERATIONS_TO_DROP) + "\n");
    }

    public void delA_indy() {
        long total = 0;
        for (int i = 0; i < ITERATIONS; ++i) {
            // garbage collect what we can before each test
            gc();
            timer.start();
            for (int key = 0; key < NUMBER_TO_INSERT; ++key) {
                session.deletePersistent(Stress.class, key);
            }
            // drop the first iteration
            timer.stop();
            if (i >= ITERATIONS_TO_DROP) total += timer.time();
            System.out.println("delA_indy: " + timer.time());
        }
        System.out.println("Excluding " + ITERATIONS_TO_DROP + " Average: " + total/(ITERATIONS - ITERATIONS_TO_DROP) + "\n");
    }

    public void delA_each() {
        long total = 0;
        for (int i = 0; i < ITERATIONS; ++i) {
            // garbage collect what we can before each test
            gc();
            timer.start();
            session.currentTransaction().begin();
            for (int key = 0; key < NUMBER_TO_INSERT; ++key) {
                session.deletePersistent(Stress.class, key);
                session.flush();
            }
            session.currentTransaction().commit();
            // drop the first iteration
            timer.stop();
            if (i >= ITERATIONS_TO_DROP) total += timer.time();
            System.out.println("delA_each: " + timer.time());
        }
        System.out.println("Excluding " + ITERATIONS_TO_DROP + " Average: " + total/(ITERATIONS - ITERATIONS_TO_DROP) + "\n");
    }

    public void delA_bulk() {
        long total = 0;
        for (int i = 0; i < ITERATIONS; ++i) {
            // garbage collect what we can before each test
            gc();
            timer.start();
            session.currentTransaction().begin();
            for (int key = 0; key < NUMBER_TO_INSERT; ++key) {
                session.deletePersistent(Stress.class, key);
            }
            session.currentTransaction().commit();
            // drop the first iteration
            timer.stop();
            if (i >= ITERATIONS_TO_DROP) total += timer.time();
            System.out.println("delA_bulk: " + timer.time());
        }
        System.out.println("Excluding " + ITERATIONS_TO_DROP + " Average: " + total/(ITERATIONS - ITERATIONS_TO_DROP) + "\n");
    }

    protected Stress createObject(int key) {
        Stress instance = session.newInstance(Stress.class, key);
        for (int columnNumber = 0; columnNumber < columnMetadatas.length; ++columnNumber) {
            Object value = null;
            // create value based on java type
            ColumnMetadata columnMetadata = columnMetadatas[columnNumber];
            if (columnMetadata.isPrimaryKey()) {
                // we have already set the value of the primary key in newInstance
                continue;
            }
            Class<?> cls = columnMetadata.javaType();
            int length = columnMetadata.maximumLength();
            if (int.class == cls) {
                value = key + columnNumber;
            } else if (long.class == cls) {
                value = (long)(key + columnNumber);
            } else if (float.class == cls) {
                value = (float)(key + columnNumber);
            } else if (double.class == cls) {
                value = (double)(key + columnNumber);
            } else if (short.class == cls) {
                value = (short)(key + columnNumber);
            } else if (byte.class == cls) {
                value = (byte)(key + columnNumber);
            } else if (Integer.class == cls) {
                value = (int)(key + columnNumber);
            } else if (Long.class == cls) {
                value = (long)(key + columnNumber);
            } else if (Float.class == cls) {
                value = (float)(key + columnNumber);
            } else if (Double.class == cls) {
                value = (double)(key + columnNumber);
            } else if (Short.class == cls) {
                value = (short)(key + columnNumber);
            } else if (Byte.class == cls) {
                value = (byte)(key + columnNumber);
            } else if (String.class == cls) {
                // take 'n' characters from the static String
                value = STRING.substring(key + columnNumber, key + columnNumber + length);
            } else if (byte[].class == cls) {
                // take 'n' bytes from the static byte array
                value = new byte[length];
                BYTES.position((key + columnNumber));
                BYTES.get((byte[])value);
            } else {
                throw new ClusterJFatalUserException("Unsupported column type " + cls.getName()
                        + " for column " + columnMetadata.name());
            }
            instance.set(columnNumber, value);
        }
        return instance;
    }

    public static class Stress extends DynamicObject implements IdBase {
        public Stress() {}

        public String table() {
            System.out.println("Stress table being used: " + tableName);
            return tableName;
        }

        public int getId() {
            return (Integer) get(0);
        }

        public void setId(int id) {
            set(0, id);
        }
    }

    static private void gc() {
        // empirically determined limit after which no further
        // reduction in memory usage has been observed
        //final int nFullGCs = 5;
        final int nFullGCs = 10;
        for (int i = 0; i < nFullGCs; i++) {
            //out.print("gc: ");
            long oldfree;
            long newfree = rt.freeMemory();
            do {
                oldfree = newfree;
                rt.runFinalization();
                rt.gc();
                newfree = rt.freeMemory();
                //out.print('.');
            } while (newfree > oldfree);
            //out.println();
        }
    }

    private static class Timer {

        private long time;

        public void start() {
            time = System.nanoTime() / 1000000;
        }

        public long stop() {
            time = (System.nanoTime() / 1000000) - time;
            return time;
        }

        public long time() {
            return time;
        }
    }

}
