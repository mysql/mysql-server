/*****************************************************************************

Copyright (C) 2009, 2010 Facebook, Inc. All Rights Reserved.
Copyright (c) 2011, 2011, Oracle and/or its affiliates. All Rights Reserved.

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

/***************************************************************//**
@file ut/ut0crc32.cc
CRC32 implementation from Facebook, based on the zlib implementation.

Created Aug 8, 2011, Vasil Dimov, based on mysys/my_crc32.c and
mysys/my_perf.c, contributed by Facebook under the following license.
********************************************************************/

/* Copyright (C) 2009-2010 Facebook, Inc.  All Rights Reserved.

   Dual licensed under BSD license and GPLv2.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY FACEBOOK, INC. ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
   EVENT SHALL FACEBOOK, INC. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along with
   this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/* The below CRC32 implementation is based on the implementation included with
 * zlib with modifications to process 8 bytes at a time and using SSE 4.2
 * extentions when available.  The polynomial constant has been changed to
 * match the one used by SSE 4.2 and does not return the same value as the
 * version used by zlib.  This implementation only supports 64-bit
 * little-endian processors.  The original zlib copyright notice follows. */

/* crc32.c -- compute the CRC-32 of a buf stream
 * Copyright (C) 1995-2005 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Thanks to Rodney Brown <rbrown64@csc.com.au> for his contribution of faster
 * CRC methods: exclusive-oring 32 bits of buf at a time, and pre-computing
 * tables for updating the shift register in one step with three exclusive-ors
 * instead of four steps with four exclusive-ors.  This results in about a
 * factor of two increase in speed on a Power PC G4 (PPC7455) using gcc -O3.
 */

#include "univ.i"
#include "ut0crc32.h"

#include <string.h>

ib_ut_crc32_t	ut_crc32;

/* Precalculated table used to generate the CRC32 if the CPU does not
have support for it */
static ib_uint32_t	ut_crc32_slice8_table[8][256];
static ibool		ut_crc32_slice8_table_initialized = FALSE;

/* Flag that tells whether the CPU supports CRC32 or not */
UNIV_INTERN bool	ut_crc32_sse2_enabled = false;

/********************************************************************//**
Initializes the table that is used to generate the CRC32 if the CPU does
not have support for it. */
static
void
ut_crc32_slice8_table_init()
/*========================*/
{
	/* bit-reversed poly 0x1EDC6F41 (from SSE42 crc32 instruction) */
	static const ib_uint32_t	poly = 0x82f63b78;
	ib_uint32_t			n;
	ib_uint32_t			k;
	ib_uint32_t			c;

	for (n = 0; n < 256; n++) {
		c = n;
		for (k = 0; k < 8; k++) {
			c = (c & 1) ? (poly ^ (c >> 1)) : (c >> 1);
		}
		ut_crc32_slice8_table[0][n] = c;
	}

	for (n = 0; n < 256; n++) {
		c = ut_crc32_slice8_table[0][n];
		for (k = 1; k < 8; k++) {
			c = ut_crc32_slice8_table[0][c & 0xFF] ^ (c >> 8);
			ut_crc32_slice8_table[k][n] = c;
		}
	}

	ut_crc32_slice8_table_initialized = TRUE;
}

#if defined(__GNUC__) && defined(__x86_64__)
/********************************************************************//**
Fetches CPU info */
static
void
ut_cpuid(
/*=====*/
	ib_uint32_t	vend[3],	/*!< out: CPU vendor */
	ib_uint32_t*	model,		/*!< out: CPU model */
	ib_uint32_t*	family,		/*!< out: CPU family */
	ib_uint32_t*	stepping,	/*!< out: CPU stepping */
	ib_uint32_t*	features_ecx,	/*!< out: CPU features ecx */
	ib_uint32_t*	features_edx)	/*!< out: CPU features edx */
{
	ib_uint32_t	sig;
	asm("cpuid" : "=b" (vend[0]), "=c" (vend[2]), "=d" (vend[1]) : "a" (0));
	asm("cpuid" : "=a" (sig), "=c" (*features_ecx), "=d" (*features_edx)
	    : "a" (1)
	    : "ebx");

	*model = ((sig >> 4) & 0xF);
	*family = ((sig >> 8) & 0xF);
	*stepping = (sig & 0xF);

	if (memcmp(vend, "GenuineIntel", 12) == 0
	    || (memcmp(vend, "AuthenticAMD", 12) == 0 && *family == 0xF)) {

		*model += (((sig >> 16) & 0xF) << 4);
		*family += ((sig >> 20) & 0xFF);
	}
}

/* opcodes taken from objdump of "crc32b (%%rdx), %%rcx"
for RHEL4 support (GCC 3 doesn't support this instruction) */
#define ut_crc32_sse42_byte \
	asm(".byte 0xf2, 0x48, 0x0f, 0x38, 0xf0, 0x0a" \
	    : "=c"(crc) : "c"(crc), "d"(buf)); \
	len--, buf++

/* opcodes taken from objdump of "crc32q (%%rdx), %%rcx"
for RHEL4 support (GCC 3 doesn't support this instruction) */
#define ut_crc32_sse42_quadword \
	asm(".byte 0xf2, 0x48, 0x0f, 0x38, 0xf1, 0x0a" \
	    : "=c"(crc) : "c"(crc), "d"(buf)); \
	len -= 8, buf += 8
#endif /* defined(__GNUC__) && defined(__x86_64__) */

/********************************************************************//**
Calculates CRC32 using CPU instructions.
@return CRC-32C (polynomial 0x11EDC6F41) */
UNIV_INLINE
ib_uint32_t
ut_crc32_sse42(
/*===========*/
	const byte*	buf,	/*!< in: data over which to calculate CRC32 */
	ulint		len)	/*!< in: data length */
{
#if defined(__GNUC__) && defined(__x86_64__)
	ib_uint64_t	crc = (ib_uint32_t) (-1);

	ut_a(ut_crc32_sse2_enabled);

	while (len && ((ulint) buf & 7)) {
		ut_crc32_sse42_byte;
	}

	while (len >= 32) {
		ut_crc32_sse42_quadword;
		ut_crc32_sse42_quadword;
		ut_crc32_sse42_quadword;
		ut_crc32_sse42_quadword;
	}

	while (len >= 8) {
		ut_crc32_sse42_quadword;
	}

	while (len) {
		ut_crc32_sse42_byte;
	}

	return((ib_uint32_t) ((~crc) & 0xFFFFFFFF));
#else
	ut_error;
	/* silence compiler warning about unused parameters */
	return((ib_uint32_t) buf[len]);
#endif /* defined(__GNUC__) && defined(__x86_64__) */
}

#define ut_crc32_slice8_byte \
	crc = (crc >> 8) ^ ut_crc32_slice8_table[0][(crc ^ *buf++) & 0xFF]; \
	len--

#define ut_crc32_slice8_quadword \
	crc ^= *(ib_uint64_t*) buf; \
	crc = ut_crc32_slice8_table[7][(crc      ) & 0xFF] ^ \
	      ut_crc32_slice8_table[6][(crc >>  8) & 0xFF] ^ \
	      ut_crc32_slice8_table[5][(crc >> 16) & 0xFF] ^ \
	      ut_crc32_slice8_table[4][(crc >> 24) & 0xFF] ^ \
	      ut_crc32_slice8_table[3][(crc >> 32) & 0xFF] ^ \
	      ut_crc32_slice8_table[2][(crc >> 40) & 0xFF] ^ \
	      ut_crc32_slice8_table[1][(crc >> 48) & 0xFF] ^ \
	      ut_crc32_slice8_table[0][(crc >> 56)]; \
	len -= 8, buf += 8

/********************************************************************//**
Calculates CRC32 manually.
@return CRC-32C (polynomial 0x11EDC6F41) */
UNIV_INLINE
ib_uint32_t
ut_crc32_slice8(
/*============*/
	const byte*	buf,	/*!< in: data over which to calculate CRC32 */
	ulint		len)	/*!< in: data length */
{
	ib_uint64_t	crc = (ib_uint32_t) (-1);

	ut_a(ut_crc32_slice8_table_initialized);

	while (len && ((ulint) buf & 7)) {
		ut_crc32_slice8_byte;
	}

	while (len >= 32) {
		ut_crc32_slice8_quadword;
		ut_crc32_slice8_quadword;
		ut_crc32_slice8_quadword;
		ut_crc32_slice8_quadword;
	}

	while (len >= 8) {
		ut_crc32_slice8_quadword;
	}

	while (len) {
		ut_crc32_slice8_byte;
	}

	return((ib_uint32_t) ((~crc) & 0xFFFFFFFF));
}

/********************************************************************//**
Initializes the data structures used by ut_crc32(). Does not do any
allocations, would not hurt if called twice, but would be pointless. */
UNIV_INTERN
void
ut_crc32_init()
/*===========*/
{
#if defined(__GNUC__) && defined(__x86_64__)
	ib_uint32_t	vend[3];
	ib_uint32_t	model;
	ib_uint32_t	family;
	ib_uint32_t	stepping;
	ib_uint32_t	features_ecx;
	ib_uint32_t	features_edx;

	ut_cpuid(vend, &model, &family, &stepping,
		 &features_ecx, &features_edx);

	/* Valgrind does not understand the CRC32 instructions:

	vex amd64->IR: unhandled instruction bytes: 0xF2 0x48 0xF 0x38 0xF0 0xA
	valgrind: Unrecognised instruction at address 0xad3db5.
	Your program just tried to execute an instruction that Valgrind
	did not recognise.  There are two possible reasons for this.
	1. Your program has a bug and erroneously jumped to a non-code
	   location.  If you are running Memcheck and you just saw a
	   warning about a bad jump, it's probably your program's fault.
	2. The instruction is legitimate but Valgrind doesn't handle it,
	   i.e. it's Valgrind's fault.  If you think this is the case or
	   you are not sure, please let us know and we'll try to fix it.
	Either way, Valgrind will now raise a SIGILL signal which will
	probably kill your program.

	*/
#ifndef UNIV_DEBUG_VALGRIND
	ut_crc32_sse2_enabled = (features_ecx >> 20) & 1;
#endif /* UNIV_DEBUG_VALGRIND */

#endif /* defined(__GNUC__) && defined(__x86_64__) */

	if (ut_crc32_sse2_enabled) {
		ut_crc32 = ut_crc32_sse42;
	} else {
		ut_crc32_slice8_table_init();
		ut_crc32 = ut_crc32_slice8;
	}
}
