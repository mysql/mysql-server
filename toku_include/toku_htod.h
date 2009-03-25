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

#include "toku_htonl.h"

#if !defined(__BYTE_ORDER) || \
    !defined(__LITTLE_ENDIAN) || \
    !defined(__BIG_ENDIAN)
#error Standard endianness things not all defined
#endif

#define NETWORK_BYTE_ORDER  (__BIG_ENDIAN)
#define INTEL_BYTE_ORDER    (__LITTLE_ENDIAN)
#define HOST_BYTE_ORDER     (__BYTE_ORDER)

//Switch DISK_BYTE_ORDER to INTEL_BYTE_ORDER to speed up intel.
//#define DISK_BYTE_ORDER     (NETWORK_BYTE_ORDER)
#define DISK_BYTE_ORDER     (INTEL_BYTE_ORDER)

#if defined(__PDP_ENDIAN) && (HOST_BYTE_ORDER==__PDP_ENDIAN)
#error "Are we in ancient Rome?  You REALLY want support for PDP_ENDIAN?"
#elif (HOST_BYTE_ORDER!=__BIG_ENDIAN) && (HOST_BYTE_ORDER!=__LITTLE_ENDIAN)
#error HOST_BYTE_ORDER not well defined
#endif

#if HOST_BYTE_ORDER==DISK_BYTE_ORDER
#define HTOD_NEED_SWAP 0
#else
#define HTOD_NEED_SWAP 1
#if (HOST_BYTE_ORDER==__BIG_ENDIAN)
#error Byte swapping on Big Endian is not coded in htod.c
#endif
#endif

#if !HTOD_NEED_SWAP
static inline uint32_t
toku_dtoh32(uint32_t i) {
    return i;
}

static inline uint32_t
toku_htod32(uint32_t i) {
    return i;
}

#elif HOST_BYTE_ORDER == __LITTLE_ENDIAN //HTOD_NEED_SWAP

static inline uint32_t
toku_dtoh32(uint32_t i) {
    return ntohl(i);
}

static inline uint32_t
toku_htod32(uint32_t i) {
    return htonl(i);
}

#elif HOST_BYTE_ORDER == __BIG_ENDIAN //!HTOD_NEED_SWAP

#error Byte swapping in big endian not yet supported

#endif

#endif

