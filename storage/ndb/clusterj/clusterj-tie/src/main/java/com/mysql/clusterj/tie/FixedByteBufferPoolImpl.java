/*
 *  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.clusterj.tie;

import java.nio.ByteBuffer;
import java.util.concurrent.ConcurrentLinkedQueue;

import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 * This class implements a simple monotonically-growing pool of ByteBuffer. Each NdbRecord
 * has its own pool, including the value NdbRecord (all columns) and each index NdbRecord used for
 * index scans and delete operations.
 */
class FixedByteBufferPoolImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(FixedByteBufferPoolImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(FixedByteBufferPoolImpl.class);

    /** The guard initialization bytes; to add a guard after the record, change the array size */
    static byte[] guard = new byte[0];
    static {
        for (int i = 0; i < guard.length; ++i) {
            guard[i] = (byte)10;
        }
    }

    /** Initialize the guard for a specific buffer. */
    void initializeGuard(ByteBuffer buffer) {
     // the buffer has guard.length extra bytes in it, initialized with the guard bytes array
        buffer.position(buffer.capacity() - guard.length);
        buffer.put(guard);
        buffer.clear();
    }
    /** Check the guard */
    void checkGuard(ByteBuffer buffer, String where) {
        buffer.clear();
        boolean fail = false;
        // the buffer has guard.length extra bytes in it, initialized with the guard bytes
        buffer.position(buffer.capacity() - guard.length);
        for (int i = 0; i < guard.length; ++i) {
            byte actual = buffer.get();
            byte expected = guard[i];
            if (expected != actual) {
                fail = true;
                logger.warn(local.message("WARN_Buffer_Pool_Guard_Check_Failed",
                        where, (buffer.capacity() - guard.length), expected, actual, buffer.toString()));
            }
        }
        // reset it for next time
        initializeGuard(buffer);
    }

    /** The pool of ByteBuffer */
    ConcurrentLinkedQueue<ByteBuffer> pool;

    /** The name of this pool */
    String name;

    /** The length of each buffer */
    int bufferSize;

    /** The high water mark of this pool */
    int highWaterMark= 0;

    /** Construct an empty pool */
    public FixedByteBufferPoolImpl(int bufferSize, String name) {
        this.bufferSize = bufferSize;
        this.name = name;
        this.pool = new ConcurrentLinkedQueue<ByteBuffer>();
        logger.info("FixedByteBufferPoolImpl<init> for " + name + " bufferSize " + bufferSize);
    }

    /** Borrow a buffer from the pool. If none in the pool, create a new one. */
    public ByteBuffer borrowBuffer() {
        ByteBuffer buffer = pool.poll();
        if (buffer == null) {
            buffer = ByteBuffer.allocateDirect(bufferSize + guard.length);
            initializeGuard(buffer);
            if (logger.isDetailEnabled()) logger.detail("FixedByteBufferPoolImpl for " + name +
                    " got new  buffer: position " + buffer.position() +
                    " capacity " + buffer.capacity() + " limit " + buffer.limit());
        } else {
            if (logger.isDetailEnabled()) logger.detail("FixedByteBufferPoolImpl for " + name +
                    " got used buffer: position " + buffer.position() +
                    " capacity " + buffer.capacity() + " limit " + buffer.limit());
        }
        buffer.clear();
        return buffer;
    }

    /** Return a buffer to the pool. */
    public void returnBuffer(ByteBuffer buffer) {
//        checkGuard(buffer, "returnBuffer"); // uncomment this to enable checking
        if (buffer.capacity() != bufferSize + guard.length) {
            String message = local.message("ERR_Wrong_Buffer_Size_Returned_To_Pool",
                    name, bufferSize, buffer.capacity());
            throw new ClusterJFatalInternalException(message);
        }
        pool.add(buffer);
    }
}
