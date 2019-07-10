/*****************************************************************************

Copyright (c) 2011, 2011, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

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

#include "univ.i"

/********************************************************************//**
Initializes the data structures used by ut_crc32(). Does not do any
allocations, would not hurt if called twice, but would be pointless. */
UNIV_INTERN
void
ut_crc32_init();
/*===========*/

/********************************************************************//**
Calculates CRC32.
@param ptr	- data over which to calculate CRC32.
@param len	- data length in bytes.
@return CRC32 (CRC-32C, using the GF(2) primitive polynomial 0x11EDC6F41,
or 0x1EDC6F41 without the high-order bit) */
typedef ib_uint32_t (*ib_ut_crc32_t)(const byte* ptr, ulint len);

extern ib_ut_crc32_t	ut_crc32;

extern bool	ut_crc32_sse2_enabled;

#endif /* ut0crc32_h */
