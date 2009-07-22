/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."


/* Purpose of this file is to provide definitions of 
 * Host to Disk byte transposition functions, an abstraction of
 * htod32()/dtoh32() and htod16()/dtoh16() functions.
 *
 * These htod/dtoh functions will only perform the transposition
 * if the disk and host are defined to be in opposite endian-ness.
 * If we define the disk to be in host order, then no byte 
 * transposition is performed.  (We might do this to save the 
 * the time used for byte transposition.) 
 * 
 * This abstraction layer allows us to define the disk to be in
 * any byte order with a single compile-time switch (in htod.c).
 *
 * NOTE: THIS FILE DOES NOT CURRENTLY SUPPORT A BIG-ENDIAN
 *       HOST AND A LITTLE-ENDIAN DISK.
 */

#ifndef HTOD_H
#define HTOD_H

static const int64_t toku_byte_order_host = 0x0102030405060708LL;

#include <endian.h>
#if !defined(__BYTE_ORDER) || \
    !defined(__LITTLE_ENDIAN) || \
    !defined(__BIG_ENDIAN)
#error Standard endianness things not all defined
#endif

#define NETWORK_BYTE_ORDER  (__BIG_ENDIAN)
#define INTEL_BYTE_ORDER    (__LITTLE_ENDIAN)
#define HOST_BYTE_ORDER     (__BYTE_ORDER)

//DISK_BYTE_ORDER is the byte ordering for integers written to disk.
//If DISK_BYTE_ORDER is the same as HOST_BYTE_ORDER no conversions are necessary.
//Otherwise some structures require conversion to HOST_BYTE_ORDER on loading from disk (HOST_BYTE_ORDER in memory), and
//others require conversion to HOST_BYTE_ORDER on every access/mutate (DISK_BYTE_ORDER in memory).
#define DISK_BYTE_ORDER     (INTEL_BYTE_ORDER)

#if HOST_BYTE_ORDER!=INTEL_BYTE_ORDER
//Even though the functions are noops if DISK==HOST, we do not have the logic to test whether the file was moved from another BYTE_ORDER machine.
#error Only intel byte order supported so far.
#endif

#if DISK_BYTE_ORDER == HOST_BYTE_ORDER
static inline uint64_t
toku_dtoh64(uint64_t i) {
    return i;
}

static inline uint64_t
toku_htod64(uint64_t i) {
    return i;
}

static inline uint32_t
toku_dtoh32(uint32_t i) {
    return i;
}

static inline uint32_t
toku_htod32(uint32_t i) {
    return i;
}
#else
#error Not supported
#endif

#endif

