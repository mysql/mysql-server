/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbUtil.java,v 11.5 2002/01/11 15:52:41 bostic Exp $
 */

package com.sleepycat.db;

/**
 *
 * @author David M. Krinsky
 */

// DbUtil is a simple, package-private wrapper class that holds a few
// static utility functions other parts of the package share and that don't
// have a good home elsewhere.  (For now, that's limited to byte-array-to-int
// conversion and back.)

class DbUtil
{
    // Get the u_int32_t stored beginning at offset "offset" into
    // array "arr".  We have to do the conversion manually since it's
    // a C-native int, and we're not really supposed to make this kind of
    // cast in Java.
    static int array2int(byte[] arr, int offset)
    {
        int b1, b2, b3, b4;
        int pos = offset;

        // Get the component bytes;  b4 is most significant, b1 least.
        if (big_endian) {
            b4 = arr[pos++];
            b3 = arr[pos++];
            b2 = arr[pos++];
            b1 = arr[pos];
        } else {
            b1 = arr[pos++];
            b2 = arr[pos++];
            b3 = arr[pos++];
            b4 = arr[pos];
        }

        // Bytes are signed.  Convert [-128, -1] to [128, 255].
        if (b1 < 0) { b1 += 256; }
        if (b2 < 0) { b2 += 256; }
        if (b3 < 0) { b3 += 256; }
        if (b4 < 0) { b4 += 256; }

        // Put the bytes in their proper places in an int.
        b2 <<= 8;
        b3 <<= 16;
        b4 <<= 24;

        // Return their sum.
        return (b1 + b2 + b3 + b4);
    }

    // Store the specified u_int32_t, with endianness appropriate
    // to the platform we're running on, into four consecutive bytes of
    // the specified byte array, starting from the specified offset.
    static void int2array(int n, byte[] arr, int offset)
    {
        int b1, b2, b3, b4;
        int pos = offset;

        b1 = n & 0xff;
        b2 = (n >> 8) & 0xff;
        b3 = (n >> 16) & 0xff;
        b4 = (n >> 24) & 0xff;

        // Bytes are signed.  Convert [128, 255] to [-128, -1].
        if (b1 >= 128) { b1 -= 256; }
        if (b2 >= 128) { b2 -= 256; }
        if (b3 >= 128) { b3 -= 256; }
        if (b4 >= 128) { b4 -= 256; }

        // Put the bytes in the appropriate place in the array.
        if (big_endian) {
            arr[pos++] = (byte)b4;
            arr[pos++] = (byte)b3;
            arr[pos++] = (byte)b2;
            arr[pos] = (byte)b1;
        } else {
            arr[pos++] = (byte)b1;
            arr[pos++] = (byte)b2;
            arr[pos++] = (byte)b3;
            arr[pos] = (byte)b4;
        }
    }

    private static final boolean big_endian = is_big_endian();
    private static native boolean is_big_endian();
}

// end of DbUtil.java
