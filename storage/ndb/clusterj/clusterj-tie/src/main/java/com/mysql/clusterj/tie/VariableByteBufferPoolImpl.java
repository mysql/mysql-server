/*
 *  Copyright (c) 2015, 2026, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
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

package com.mysql.clusterj.tie;

import java.nio.ByteBuffer;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.Map;
import java.util.HashMap;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentLinkedQueue;

import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 * This class implements a pool consisting of several size-based monotonically-growing queues of ByteBuffer.
 * The number and sizes of the queues is determined by the constructor parameter int[] which
 * specifies the sizes.
 */
class VariableByteBufferPoolImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(VariableByteBufferPoolImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(VariableByteBufferPoolImpl.class);

    /** Hash table maps size-spec-array to VariableByteBufferPoolImpl instance */
    private static final Map<String, VariableByteBufferPoolImpl>
        bufferPoolsTable = new HashMap<String, VariableByteBufferPoolImpl>();

    /** The queues of ByteBuffer */
    final TreeMap<Integer, ConcurrentLinkedQueue<ByteBuffer>> queues;

    /** The biggest size of any queue */
    int biggest = 0;

    /** Portable Cleaner */
    interface ByteBufferCleaner {
        void clean(ByteBuffer b);
    }

    abstract static class Cleaner0 implements ByteBufferCleaner {
        protected Class<?> implClass = null;
        protected Method cleanerMethod = null;

        abstract void test() throws ReflectiveOperationException;
        abstract void invoke(ByteBuffer b) throws ReflectiveOperationException;
        public void clean(ByteBuffer b) {
            try {
                invoke(b);
            } catch (ReflectiveOperationException e) {
                // oh well
            }
        }
    }

    /* OpenJDK 9 build 150 added Unsafe.invokeCleaner in sun.misc.Unsafe
       (OpenJDK bug 8171377); it is usable up through OpenJDK 23, where, with
       the release of the FFM API, it becomes deprecated for removal.
    */
    static class SunMiscUnsafeCleaner extends Cleaner0 {
        protected Object obj = null;

        SunMiscUnsafeCleaner() throws ReflectiveOperationException {
            implClass = Class.forName("sun.misc.Unsafe");
            Field theUnsafe = implClass.getDeclaredField("theUnsafe");
            theUnsafe.setAccessible(true);
            obj = theUnsafe.get(null);
            cleanerMethod = implClass.getMethod("invokeCleaner", ByteBuffer.class);
        }

        void test() throws ReflectiveOperationException {
            ByteBuffer b = ByteBuffer.allocateDirect(1);
            invoke(b);
        }

        void invoke(ByteBuffer buffer) throws ReflectiveOperationException {
            cleanerMethod.invoke(obj, buffer);
        }
    }

    static ByteBufferCleaner theByteBufferCleaner = null;
    static {
        Cleaner0 cleaner = null;

        if(cleaner == null) {
            try {
                cleaner = new SunMiscUnsafeCleaner();
            } catch (ReflectiveOperationException e) { }
        }

        try {
            cleaner.test();
        } catch (Throwable t) {
            logger.warn(local.message("WARN_Buffer_Cleaning_Unusable", t.getClass().getName(), t.getMessage()));
        }
        theByteBufferCleaner = cleaner;
    }

    /** Clean the non-pooled DirectByteBuffer after use, freeing the memory back to the system.
     */
    static void clean(ByteBuffer buffer) {
        if(theByteBufferCleaner != null)
            theByteBufferCleaner.clean(buffer);
    }

    /** The guard initialization bytes. To enable the guard, change the size of the guard array. */
    static byte[] guard = new byte[0];

    /** Initialize the guard */
    static {
        for (int i = 0; i < guard.length; ++i) {
            guard[i] = (byte)10;
        }
    }

    /** Initialize the guard bytes following the allocated data in the buffer. */
    void initializeGuard(ByteBuffer buffer) {
     // the buffer has guard.length extra bytes in it, initialized with the guard bytes
        buffer.position(buffer.capacity() - guard.length);
        buffer.put(guard);
        buffer.position(0);
    }

    /** Check the guard bytes which immediately follow the data in the buffer. */
    void checkGuard(ByteBuffer buffer) {
        // only check if there is a direct buffer that is still viable
        if (buffer.limit() == 0) return;
        // the buffer has guard.length extra bytes in it, initialized with the guard bytes
        buffer.limit(buffer.capacity());
        buffer.position(buffer.capacity() - guard.length);
        for (int i = 0; i < guard.length; ++i) {
            if (buffer.get() != guard[i]) {
                throw new RuntimeException("ByteBufferPool failed guard test with buffer of length " +
                        (buffer.capacity() - guard.length) + ": " + buffer.toString());
            }
        }
    }

    /** Construct empty queues based on maximum size buffer each queue will handle */
    private VariableByteBufferPoolImpl(int[] bufferSizes) {
        queues = new TreeMap<Integer, ConcurrentLinkedQueue<ByteBuffer>>();
        for (int bufferSize: bufferSizes) {
            queues.put(bufferSize, new ConcurrentLinkedQueue<ByteBuffer>());
            if (biggest < bufferSize) {
                biggest = bufferSize;
            }
        }
        logger.info(() -> local.message("MSG_ByteBuffer_Pools_Initialized",
                                        Arrays.toString(bufferSizes)));
    }

    /** Borrow a buffer from the pool. The pool is the smallest that has buffers
     *  of the size needed. If no buffer is in the pool, create a new one.
     */
    public ByteBuffer borrowBuffer(int sizeNeeded) {
        Map.Entry<Integer, ConcurrentLinkedQueue<ByteBuffer>> entry = queues.ceilingEntry(sizeNeeded);
        ByteBuffer buffer = null;
        if (entry == null) {
            // oh no, we need a bigger size than any buffer pool, so log a message and direct allocate a buffer
            logger.warn(local.message("MSG_Cannot_allocate_byte_buffer_from_pool", sizeNeeded, this.biggest));
            buffer = ByteBuffer.allocateDirect(sizeNeeded + guard.length);
            initializeGuard(buffer);
            return buffer;
        }
        ConcurrentLinkedQueue<ByteBuffer>pool = entry.getValue();
        int bufferSize = entry.getKey();
        buffer = pool.poll();
        if (buffer == null) {
            // no buffer currently in the pool, so allocate a new one
            buffer = ByteBuffer.allocateDirect(bufferSize + guard.length);
            initializeGuard(buffer);
        }
        // reuse buffer without initializing the guard
        buffer.limit(sizeNeeded);
        buffer.position(0);
        return buffer;
    }

    /** Return a buffer to the pool. The appropriate pool is determined using
     *  buffer.capacity(). If the buffer did not come from a pool (because the
     *  requested size too big for any pool) then attempt to clean it. An
     *  exception in the try block would result from a buffer whose size is
     *  mismatched with all of the pools managed here.
     */
    public void returnBuffer(ByteBuffer buffer) {
        int key = buffer.capacity() - guard.length;
        if(key > biggest) {
            // mark this buffer as unusable in case we ever see it again
            buffer.limit(0);
            // clean (deallocate memory) the buffer
            clean(buffer);
        } else {
            try {
                queues.get(key).add(buffer);
            } catch(NullPointerException npe) {
                throw new ClusterJFatalInternalException(npe);
            }
        }
    }

    /* Get the VariableByteBufferPoolImpl for a particular size spec */
    public static VariableByteBufferPoolImpl getPool(int[] bufferSizes) {
        String key = Arrays.toString(bufferSizes);
        synchronized(bufferPoolsTable) {
            VariableByteBufferPoolImpl impl = bufferPoolsTable.get(key);
            if(impl == null) {
                impl = new VariableByteBufferPoolImpl(bufferSizes);
                bufferPoolsTable.put(key, impl);
            }
            return impl;
        }
    }
}
