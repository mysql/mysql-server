/*****************************************************************************

Copyright (c) 2011, 2015, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/ut0crc32.h
CRC32 implementation

Created Aug 10, 2011 Vasil Dimov
*******************************************************/

#ifndef ut0crc32_h
#define ut0crc32_h

#include <my_global.h>

#ifdef  __cplusplus
extern "C" {
#endif /* __cplusplus */

/********************************************************************//**
Initializes the data structures used by ut_crc32*(). Does not do any
allocations, would not hurt if called twice, but would be pointless. */
/* from UNIV_INTERN in storage/innobase/include/univ.i */
void
ut_crc32_init();
/*===========*/

/********************************************************************//**
Calculates CRC32.
@param ptr - data over which to calculate CRC32.
@param len - data length in bytes.
@return CRC32 (CRC-32C, using the GF(2) primitive polynomial 0x11EDC6F41,
or 0x1EDC6F41 without the high-order bit) */
typedef uint32_t	(*ut_crc32_func_t)(const uint8* ptr, my_ulonglong len);

/** Pointer to CRC32C calculation function. */
extern ut_crc32_func_t	ut_crc32c;

/** Pointer to CRC32C calculation function, which uses big-endian byte order
when converting byte strings to integers internally. */
extern ut_crc32_func_t	ut_crc32c_legacy_big_endian;

/** Pointer to CRC32C-byte-by-byte calculation function (byte order agnostic,
but very slow). */
extern ut_crc32_func_t	ut_crc32c_byte_by_byte;

/* Pointer to CRC32 calculation function - polynominal 0x04C11DB7 */
extern ut_crc32_func_t  ut_crc32;

typedef uint32 (*ut_crc32_ex_func_t)(uint32, const uint8* ptr, my_ulonglong len);
/* extended CRC32 function taking the partial CRC32 as an input */
extern ut_crc32_ex_func_t        ut_crc32_ex;

/** Text description of CRC32(C) implementation */
extern const char *ut_crc32_implementation;

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* ut0crc32_h */
