/*****************************************************************************

Copyright (c) 2009, 2010 Facebook, Inc. All Rights Reserved.
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
 * extensions when available.  The polynomial constant has been changed to
 * match the one used by SSE 4.2 and does not return the same value as the
 * version used by zlib.  The original zlib copyright notice follows. */

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <string.h>

#include "univ.i"
#include "ut0crc32.h"

/** Pointer to CRC32 calculation function. */
ut_crc32_func_t	ut_crc32;

/** Pointer to CRC32 calculation function, which uses big-endian byte order
when converting byte strings to integers internally. */
ut_crc32_func_t	ut_crc32_legacy_big_endian;

/** Pointer to CRC32-byte-by-byte calculation function (byte order agnostic,
but very slow). */
ut_crc32_func_t	ut_crc32_byte_by_byte;

/** Swap the byte order of an 8 byte integer.
@param[in]	i	8-byte integer
@return 8-byte integer */
inline
uint64_t
ut_crc32_swap_byteorder(
	uint64_t	i)
{
	return(i << 56
	       | (i & 0x000000000000FF00ULL) << 40
	       | (i & 0x0000000000FF0000ULL) << 24
	       | (i & 0x00000000FF000000ULL) << 8
	       | (i & 0x000000FF00000000ULL) >> 8
	       | (i & 0x0000FF0000000000ULL) >> 24
	       | (i & 0x00FF000000000000ULL) >> 40
	       | i >> 56);
}

/* CRC32 hardware implementation. */

/* Flag that tells whether the CPU supports CRC32 or not */
bool	ut_crc32_sse2_enabled = false;

#if defined(__GNUC__) && defined(__x86_64__)
/********************************************************************//**
Fetches CPU info */
static
void
ut_cpuid(
/*=====*/
	uint32_t	vend[3],	/*!< out: CPU vendor */
	uint32_t*	model,		/*!< out: CPU model */
	uint32_t*	family,		/*!< out: CPU family */
	uint32_t*	stepping,	/*!< out: CPU stepping */
	uint32_t*	features_ecx,	/*!< out: CPU features ecx */
	uint32_t*	features_edx)	/*!< out: CPU features edx */
{
	uint32_t	sig;
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

/** Calculate CRC32 over 8-bit data using a hardware/CPU instruction.
@param[in,out]	crc	crc32 checksum so far when this function is called,
when the function ends it will contain the new checksum
@param[in,out]	data	data to be checksummed, the pointer will be advanced
with 1 byte
@param[in,out]	len	remaining bytes, it will be decremented with 1 */
inline
void
ut_crc32_8_hw(
	uint32_t*	crc,
	const byte**	data,
	ulint*		len)
{
	asm("crc32b %1, %0"
	    /* output operands */
	    : "+r" (*crc)
	    /* input operands */
	    : "rm" ((*data)[0]));

	(*data)++;
	(*len)--;
}

/** Calculate CRC32 over a 64-bit integer using a hardware/CPU instruction.
@param[in]	crc	crc32 checksum so far
@param[in]	data	data to be checksummed
@return resulting checksum of crc + crc(data) */
inline
uint32_t
ut_crc32_64_low_hw(
	uint32_t	crc,
	uint64_t	data)
{
	uint64_t	crc_64bit = crc;

	asm("crc32q %1, %0"
	    /* output operands */
	    : "+r" (crc_64bit)
	    /* input operands */
	    : "rm" (data));

	return(static_cast<uint32_t>(crc_64bit));
}

/** Calculate CRC32 over 64-bit byte string using a hardware/CPU instruction.
@param[in,out]	crc	crc32 checksum so far when this function is called,
when the function ends it will contain the new checksum
@param[in,out]	data	data to be checksummed, the pointer will be advanced
with 8 bytes
@param[in,out]	len	remaining bytes, it will be decremented with 8 */
inline
void
ut_crc32_64_hw(
	uint32_t*	crc,
	const byte**	data,
	ulint*		len)
{
	uint64_t	data_int = *reinterpret_cast<const uint64_t*>(*data);

#ifdef WORDS_BIGENDIAN
	/* Currently we only support x86_64 (little endian) CPUs. In case
	some big endian CPU supports a CRC32 instruction, then maybe we will
	need a byte order swap here. */
#error Dont know how to handle big endian CPUs
	/*
	data_int = ut_crc32_swap_byteorder(data_int);
	*/
#endif /* WORDS_BIGENDIAN */

	*crc = ut_crc32_64_low_hw(*crc, data_int);

	*data += 8;
	*len -= 8;
}

/** Calculate CRC32 over 64-bit byte string using a hardware/CPU instruction.
The byte string is converted to a 64-bit integer using big endian byte order.
@param[in,out]	crc	crc32 checksum so far when this function is called,
when the function ends it will contain the new checksum
@param[in,out]	data	data to be checksummed, the pointer will be advanced
with 8 bytes
@param[in,out]	len	remaining bytes, it will be decremented with 8 */
inline
void
ut_crc32_64_legacy_big_endian_hw(
	uint32_t*	crc,
	const byte**	data,
	ulint*		len)
{
	uint64_t	data_int = *reinterpret_cast<const uint64_t*>(*data);

#ifndef WORDS_BIGENDIAN
	data_int = ut_crc32_swap_byteorder(data_int);
#else
	/* Currently we only support x86_64 (little endian) CPUs. In case
	some big endian CPU supports a CRC32 instruction, then maybe we will
	NOT need a byte order swap here. */
#error Dont know how to handle big endian CPUs
#endif /* WORDS_BIGENDIAN */

	*crc = ut_crc32_64_low_hw(*crc, data_int);

	*data += 8;
	*len -= 8;
}

/** Calculates CRC32 using hardware/CPU instructions.
@param[in]	buf	data over which to calculate CRC32
@param[in]	len	data length
@return CRC-32C (polynomial 0x11EDC6F41) */
uint32_t
ut_crc32_hw(
	const byte*	buf,
	ulint		len)
{
	uint32_t	crc = 0xFFFFFFFFU;

	ut_a(ut_crc32_sse2_enabled);

	/* Calculate byte-by-byte up to an 8-byte aligned address. After
	this consume the input 8-bytes at a time. */
	while (len > 0 && (reinterpret_cast<uintptr_t>(buf) & 7) != 0) {
		ut_crc32_8_hw(&crc, &buf, &len);
	}

	/* Perf testing
	./unittest/gunit/innodb/merge_innodb_tests-t --gtest_filter=ut0crc32.perf
	on CPU "Intel(R) Core(TM) i7-4770 CPU @ 3.40GHz"
	with different N in "while (len >= N) {" shows:
	N=16
	2.867254 sec
	2.866860 sec
	2.867973 sec

	N=32
	2.715725 sec
	2.713008 sec
	2.712520 sec
	(5.36% speedup over N=16)

	N=64
	2.634140 sec
	2.636558 sec
	2.636488 sec
	(2.88% speedup over N=32)

	N=128
	2.599534 sec
	2.599919 sec
	2.598035 sec
	(1.39% speedup over N=64)

	N=256
	2.576993 sec
	2.576748 sec
	2.575700 sec
	(0.87% speedup over N=128)

	N=512
	2.693928 sec
	2.691663 sec
	2.692142 sec
	(4.51% slowdown over N=256)
	*/
	while (len >= 128) {
		/* This call is repeated 16 times. 16 * 8 = 128. */
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
		ut_crc32_64_hw(&crc, &buf, &len);
	}

	while (len >= 8) {
		ut_crc32_64_hw(&crc, &buf, &len);
	}

	while (len > 0) {
		ut_crc32_8_hw(&crc, &buf, &len);
	}

	return(~crc);
}

/** Calculates CRC32 using hardware/CPU instructions.
This function uses big endian byte ordering when converting byte sequence to
integers.
@param[in]	buf	data over which to calculate CRC32
@param[in]	len	data length
@return CRC-32C (polynomial 0x11EDC6F41) */
uint32_t
ut_crc32_legacy_big_endian_hw(
	const byte*	buf,
	ulint		len)
{
	uint32_t	crc = 0xFFFFFFFFU;

	ut_a(ut_crc32_sse2_enabled);

	/* Calculate byte-by-byte up to an 8-byte aligned address. After
	this consume the input 8-bytes at a time. */
	while (len > 0 && (reinterpret_cast<uintptr_t>(buf) & 7) != 0) {
		ut_crc32_8_hw(&crc, &buf, &len);
	}

	while (len >= 128) {
		/* This call is repeated 16 times. 16 * 8 = 128. */
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
	}

	while (len >= 8) {
		ut_crc32_64_legacy_big_endian_hw(&crc, &buf, &len);
	}

	while (len > 0) {
		ut_crc32_8_hw(&crc, &buf, &len);
	}

	return(~crc);
}

/** Calculates CRC32 using hardware/CPU instructions.
This function processes one byte at a time (very slow) and thus it does
not depend on the byte order of the machine.
@param[in]	buf	data over which to calculate CRC32
@param[in]	len	data length
@return CRC-32C (polynomial 0x11EDC6F41) */
uint32_t
ut_crc32_byte_by_byte_hw(
	const byte*	buf,
	ulint		len)
{
	uint32_t	crc = 0xFFFFFFFFU;

	ut_a(ut_crc32_sse2_enabled);

	while (len > 0) {
		ut_crc32_8_hw(&crc, &buf, &len);
	}

	return(~crc);
}
#endif /* defined(__GNUC__) && defined(__x86_64__) */

/* CRC32 software implementation. */

/* Precalculated table used to generate the CRC32 if the CPU does not
have support for it */
static uint32_t	ut_crc32_slice8_table[8][256];
static bool	ut_crc32_slice8_table_initialized = false;

/********************************************************************//**
Initializes the table that is used to generate the CRC32 if the CPU does
not have support for it. */
static
void
ut_crc32_slice8_table_init()
/*========================*/
{
	/* bit-reversed poly 0x1EDC6F41 (from SSE42 crc32 instruction) */
	static const uint32_t	poly = 0x82f63b78;
	uint32_t		n;
	uint32_t		k;
	uint32_t		c;

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

	ut_crc32_slice8_table_initialized = true;
}

/** Calculate CRC32 over 8-bit data using a software implementation.
@param[in,out]	crc	crc32 checksum so far when this function is called,
when the function ends it will contain the new checksum
@param[in,out]	data	data to be checksummed, the pointer will be advanced
with 1 byte
@param[in,out]	len	remaining bytes, it will be decremented with 1 */
inline
void
ut_crc32_8_sw(
	uint32_t*	crc,
	const byte**	data,
	ulint*		len)
{
	const uint8_t	i = (*crc ^ (*data)[0]) & 0xFF;

	*crc = (*crc >> 8) ^ ut_crc32_slice8_table[0][i];

	(*data)++;
	(*len)--;
}

/** Calculate CRC32 over a 64-bit integer using a software implementation.
@param[in]	crc	crc32 checksum so far
@param[in]	data	data to be checksummed
@return resulting checksum of crc + crc(data) */
inline
uint32_t
ut_crc32_64_low_sw(
	uint32_t	crc,
	uint64_t	data)
{
	const uint64_t	i = crc ^ data;

	return(
		ut_crc32_slice8_table[7][(i      ) & 0xFF] ^
		ut_crc32_slice8_table[6][(i >>  8) & 0xFF] ^
		ut_crc32_slice8_table[5][(i >> 16) & 0xFF] ^
		ut_crc32_slice8_table[4][(i >> 24) & 0xFF] ^
		ut_crc32_slice8_table[3][(i >> 32) & 0xFF] ^
		ut_crc32_slice8_table[2][(i >> 40) & 0xFF] ^
		ut_crc32_slice8_table[1][(i >> 48) & 0xFF] ^
		ut_crc32_slice8_table[0][(i >> 56)]
	);
}

/** Calculate CRC32 over 64-bit byte string using a software implementation.
@param[in,out]	crc	crc32 checksum so far when this function is called,
when the function ends it will contain the new checksum
@param[in,out]	data	data to be checksummed, the pointer will be advanced
with 8 bytes
@param[in,out]	len	remaining bytes, it will be decremented with 8 */
inline
void
ut_crc32_64_sw(
	uint32_t*	crc,
	const byte**	data,
	ulint*		len)
{
	uint64_t	data_int = *reinterpret_cast<const uint64_t*>(*data);

#ifdef WORDS_BIGENDIAN
	data_int = ut_crc32_swap_byteorder(data_int);
#endif /* WORDS_BIGENDIAN */

	*crc = ut_crc32_64_low_sw(*crc, data_int);

	*data += 8;
	*len -= 8;
}

/** Calculate CRC32 over 64-bit byte string using a software implementation.
The byte string is converted to a 64-bit integer using big endian byte order.
@param[in,out]	crc	crc32 checksum so far when this function is called,
when the function ends it will contain the new checksum
@param[in,out]	data	data to be checksummed, the pointer will be advanced
with 8 bytes
@param[in,out]	len	remaining bytes, it will be decremented with 8 */
inline
void
ut_crc32_64_legacy_big_endian_sw(
	uint32_t*	crc,
	const byte**	data,
	ulint*		len)
{
	uint64_t	data_int = *reinterpret_cast<const uint64_t*>(*data);

#ifndef WORDS_BIGENDIAN
	data_int = ut_crc32_swap_byteorder(data_int);
#endif /* WORDS_BIGENDIAN */

	*crc = ut_crc32_64_low_sw(*crc, data_int);

	*data += 8;
	*len -= 8;
}

/** Calculates CRC32 in software, without using CPU instructions.
@param[in]	buf	data over which to calculate CRC32
@param[in]	len	data length
@return CRC-32C (polynomial 0x11EDC6F41) */
uint32_t
ut_crc32_sw(
	const byte*	buf,
	ulint		len)
{
	uint32_t	crc = 0xFFFFFFFFU;

	ut_a(ut_crc32_slice8_table_initialized);

	/* Calculate byte-by-byte up to an 8-byte aligned address. After
	this consume the input 8-bytes at a time. */
	while (len > 0 && (reinterpret_cast<uintptr_t>(buf) & 7) != 0) {
		ut_crc32_8_sw(&crc, &buf, &len);
	}

	while (len >= 128) {
		/* This call is repeated 16 times. 16 * 8 = 128. */
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
		ut_crc32_64_sw(&crc, &buf, &len);
	}

	while (len >= 8) {
		ut_crc32_64_sw(&crc, &buf, &len);
	}

	while (len > 0) {
		ut_crc32_8_sw(&crc, &buf, &len);
	}

	return(~crc);
}

/** Calculates CRC32 in software, without using CPU instructions.
This function uses big endian byte ordering when converting byte sequence to
integers.
@param[in]	buf	data over which to calculate CRC32
@param[in]	len	data length
@return CRC-32C (polynomial 0x11EDC6F41) */
uint32_t
ut_crc32_legacy_big_endian_sw(
	const byte*	buf,
	ulint		len)
{
	uint32_t	crc = 0xFFFFFFFFU;

	ut_a(ut_crc32_slice8_table_initialized);

	/* Calculate byte-by-byte up to an 8-byte aligned address. After
	this consume the input 8-bytes at a time. */
	while (len > 0 && (reinterpret_cast<uintptr_t>(buf) & 7) != 0) {
		ut_crc32_8_sw(&crc, &buf, &len);
	}

	while (len >= 128) {
		/* This call is repeated 16 times. 16 * 8 = 128. */
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
	}

	while (len >= 8) {
		ut_crc32_64_legacy_big_endian_sw(&crc, &buf, &len);
	}

	while (len > 0) {
		ut_crc32_8_sw(&crc, &buf, &len);
	}

	return(~crc);
}

/** Calculates CRC32 in software, without using CPU instructions.
This function processes one byte at a time (very slow) and thus it does
not depend on the byte order of the machine.
@param[in]	buf	data over which to calculate CRC32
@param[in]	len	data length
@return CRC-32C (polynomial 0x11EDC6F41) */
uint32_t
ut_crc32_byte_by_byte_sw(
	const byte*	buf,
	ulint		len)
{
	uint32_t	crc = 0xFFFFFFFFU;

	ut_a(ut_crc32_slice8_table_initialized);

	while (len > 0) {
		ut_crc32_8_sw(&crc, &buf, &len);
	}

	return(~crc);
}

/********************************************************************//**
Initializes the data structures used by ut_crc32*(). Does not do any
allocations, would not hurt if called twice, but would be pointless. */
void
ut_crc32_init()
/*===========*/
{
#if defined(__GNUC__) && defined(__x86_64__)
	uint32_t	vend[3];
	uint32_t	model;
	uint32_t	family;
	uint32_t	stepping;
	uint32_t	features_ecx;
	uint32_t	features_edx;

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

	if (ut_crc32_sse2_enabled) {
		ut_crc32 = ut_crc32_hw;
		ut_crc32_legacy_big_endian = ut_crc32_legacy_big_endian_hw;
		ut_crc32_byte_by_byte = ut_crc32_byte_by_byte_hw;
	}

#endif /* defined(__GNUC__) && defined(__x86_64__) */

	if (!ut_crc32_sse2_enabled) {
		ut_crc32_slice8_table_init();
		ut_crc32 = ut_crc32_sw;
		ut_crc32_legacy_big_endian = ut_crc32_legacy_big_endian_sw;
		ut_crc32_byte_by_byte = ut_crc32_byte_by_byte_sw;
	}
}
