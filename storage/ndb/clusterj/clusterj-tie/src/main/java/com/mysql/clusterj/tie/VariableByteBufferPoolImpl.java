/*
 *  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.clusterj.tie;

import java.nio.ByteBuffer;
import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentLinkedQueue;

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import sun.misc.Cleaner;

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

    /** The queues of ByteBuffer */
    TreeMap<Integer, ConcurrentLinkedQueue<ByteBuffer>> queues;

    /** The biggest size of any queue */
    int biggest = 0;

    /** The cleaner method for non-pooled buffers */
    static Field cleanerField = null;
    static {
        try {
            ByteBuffer buffer = ByteBuffer.allocateDirect(1);
            cleanerField = buffer.getClass().getDeclaredField("cleaner");
            cleanerField.setAccessible(true);
        } catch (Throwable t) {
            String message = local.message("WARN_Buffer_Cleaning_Unusable", t.getClass().getName(), t.getMessage());
            logger.warn(message);
            // cannot use cleaner
            cleanerField = null;
        }
    }

    /** Clean the non-pooled buffer after use. This frees the memory back to the system. Note that this
     * only supports the current implementation of DirectByteBuffer and this support may change in future.
     */
    static void clean(ByteBuffer buffer) {
        if (cleanerField != null) {
            try {
                ((Cleaner)cleanerField.get(buffer)).clean();
            } catch (Throwable t) {
                // oh well
            }
        }
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
        buffer.clear();
    }

    /** Check the guard bytes which immediately follow the data in the buffer. */
    void checkGuard(ByteBuffer buffer) {
        // only check if there is a direct buffer that is still viable
        if (buffer.limit() == 0) return;
        // the buffer has guard.length extra bytes in it, initialized with the guard bytes
        buffer.position(buffer.capacity() - guard.length);
        for (int i = 0; i < guard.length; ++i) {
            if (buffer.get() != guard[i]) {
                throw new RuntimeException("ByteBufferPool failed guard test with buffer of length " +
                        (buffer.capacity() - guard.length) + ": " + buffer.toString());
            }
        }
    }

    /** Construct empty queues based on maximum size buffer each queue will handle */
    public VariableByteBufferPoolImpl(int[] bufferSizes) {
        queues = new TreeMap<Integer, ConcurrentLinkedQueue<ByteBuffer>>();
        for (int bufferSize: bufferSizes) {
            queues.put(bufferSize + 1, new ConcurrentLinkedQueue<ByteBuffer>());
            if (biggest < bufferSize) {
                biggest = bufferSize;
            }
        }
        logger.info(local.message("MSG_ByteBuffer_Pools_Initialized", Arrays.toString(bufferSizes)));
    }

    /** Borrow a buffer from the pool. The pool is the smallest that has buffers of the size needed.
     * The buffer size is one less than the key because higherEntry is strictly higher.
     * There is no method that returns the entry equal to or higher which is what we really want.
     * If no buffer is in the pool, create a new one.
     */
    public ByteBuffer borrowBuffer(int sizeNeeded) {
        Map.Entry<Integer, ConcurrentLinkedQueue<ByteBuffer>> entry = queues.higherEntry(sizeNeeded);
        ByteBuffer buffer = null;
        if (entry == null) {
            // oh no, we need a bigger size than any buffer pool, so log a message and direct allocate a buffer
            if (logger.isDetailEnabled())
                logger.detail(local.message("MSG_Cannot_allocate_byte_buffer_from_pool", sizeNeeded, this.biggest));
            buffer = ByteBuffer.allocateDirect(sizeNeeded + guard.length);
            initializeGuard(buffer);
            return buffer;
        }
        ConcurrentLinkedQueue<ByteBuffer>pool = entry.getValue();
        int bufferSize = entry.getKey() - 1;
        buffer = pool.poll();
        if (buffer == null) {
            buffer = ByteBuffer.allocateDirect(bufferSize + guard.length);
            initializeGuard(buffer);
        }
        // reuse buffer without initializing the guard
        buffer.clear();
        return buffer;
    }

    /** Return a buffer to the pool. */
    public void returnBuffer(int sizeNeeded, ByteBuffer buffer) {
        checkGuard(buffer);
        Map.Entry<Integer, ConcurrentLinkedQueue<ByteBuffer>> entry = this.queues.higherEntry(sizeNeeded);
        // if this buffer came from a pool, return it
        if (entry != null) {
            int bufferSize = entry.getKey() - 1;
            ConcurrentLinkedQueue<ByteBuffer> pool = entry.getValue();
            pool.add(buffer);
        } else {
            // mark this buffer as unusable in case we ever see it again
            buffer.limit(0);
            // clean (deallocate memory) the buffer
            clean(buffer);
        }
    }

}
