/*****************************************************************************

Copyright (c) 2009, 2010 Facebook, Inc. All Rights Reserved.
Copyright (c) 2011, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/***************************************************************//**
@file ut/crc32.cc
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

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#include "my_config.h"

#include <string.h>

#include "my_compiler.h"
#include "my_inttypes.h"

#if defined(__GNUC__) && defined(__x86_64__)
#define gnuc64
#endif

#if defined(gnuc64) || defined(_WIN32)
/*
  GCC 4.8 can't include intrinsic headers without -msse4.2.
  4.9 and newer can, so we can remove this test once we no longer
  support 4.8.
*/
#if defined(__SSE4_2__) || defined(__clang__) || !defined(__GNUC__) || __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9)
#include <nmmintrin.h>
#else
// GCC 4.8 without -msse4.2.
MY_ATTRIBUTE((target("sse4.2")))
ALWAYS_INLINE uint32 _mm_crc32_u8(uint32 __C, uint32 __V)
{
  return __builtin_ia32_crc32qi(__C, __V);
}

MY_ATTRIBUTE((target("sse4.2")))
ALWAYS_INLINE uint64 _mm_crc32_u64(uint64 __C, uint64 __V)
{
  return __builtin_ia32_crc32di(__C, __V);
}
#endif
#endif  // defined(gnuc64) || defined(_WIN32)

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

/** Flag that tells whether the CPU supports CRC32 or not.
The CRC32 instructions are part of the SSE4.2 instruction set. */
bool	ut_crc32_cpu_enabled = false;

#if defined(_WIN32)
#include <intrin.h>
#endif
#if defined(gnuc64) || defined(_WIN32)
/** Checks whether the CPU has the CRC32 instructions (part of the SSE4.2
instruction set).
@return true if CRC32 is available */
static
bool
ut_crc32_check_cpu()
{
#ifdef UNIV_DEBUG_VALGRIND
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
	return false;
#else

	uint32_t	features_ecx;

#if defined(gnuc64)
	uint32_t	sig;
	uint32_t	features_edx;

	asm("cpuid" : "=a" (sig), "=c" (features_ecx), "=d" (features_edx)
	    : "a" (1)
	    : "ebx");
#elif defined(_WIN32)
	int	cpu_info[4] = {-1, -1, -1, -1};

	__cpuid(cpu_info, 1 /* function 1 */);

	features_ecx = static_cast<uint32_t>(cpu_info[2]);
#else
#error Dont know how to handle non-gnuc64 and non-windows platforms.
#endif

	return features_ecx & (1 << 20);  // SSE4.2
#endif /* UNIV_DEBUG_VALGRIND */
}

/** Calculate CRC32 over 8-bit data using a hardware/CPU instruction.
@param[in,out]	crc	crc32 checksum so far when this function is called,
when the function ends it will contain the new checksum
@param[in,out]	data	data to be checksummed, the pointer will be advanced
with 1 byte
@param[in,out]	len	remaining bytes, it will be decremented with 1 */
MY_ATTRIBUTE((target("sse4.2")))
inline
void
ut_crc32_8_hw(
	uint64_t*	crc,
	const byte**	data,
	ulint*		len)
{
	*crc = _mm_crc32_u8(static_cast<unsigned>(*crc), (*data)[0]);
	(*data)++;
	(*len)--;
}

/** Calculate CRC32 over a 64-bit integer using a hardware/CPU instruction.
@param[in]	crc	crc32 checksum so far
@param[in]	data	data to be checksummed
@return resulting checksum of crc + crc(data) */
MY_ATTRIBUTE((target("sse4.2")))
inline
uint64_t
ut_crc32_64_low_hw(
	uint64_t	crc,
	uint64_t	data)
{
	uint64_t	crc_64bit = crc;
	crc_64bit = _mm_crc32_u64(crc_64bit, data);
	return(crc_64bit);
}

/** Calculate CRC32 over 64-bit byte string using a hardware/CPU instruction.
@param[in,out]	crc	crc32 checksum so far when this function is called,
when the function ends it will contain the new checksum
@param[in,out]	data	data to be checksummed, the pointer will be advanced
with 8 bytes
@param[in,out]	len	remaining bytes, it will be decremented with 8 */
MY_ATTRIBUTE((target("sse4.2")))
inline
void
ut_crc32_64_hw(
	uint64_t*	crc,
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
	uint64_t*	crc,
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
MY_ATTRIBUTE((target("sse4.2")))
static
uint32_t
ut_crc32_hw(
	const byte*	buf,
	ulint		len)
{
	uint64_t	crc = 0xFFFFFFFFU;

	ut_a(ut_crc32_cpu_enabled);

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

	return(~static_cast<uint32_t>(crc));
}

/** Calculates CRC32 using hardware/CPU instructions.
This function uses big endian byte ordering when converting byte sequence to
integers.
@param[in]	buf	data over which to calculate CRC32
@param[in]	len	data length
@return CRC-32C (polynomial 0x11EDC6F41) */
static
uint32_t
ut_crc32_legacy_big_endian_hw(
	const byte*	buf,
	ulint		len)
{
	uint64_t	crc = 0xFFFFFFFFU;

	ut_a(ut_crc32_cpu_enabled);

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

	return(~static_cast<uint32_t>(crc));
}

/** Calculates CRC32 using hardware/CPU instructions.
This function processes one byte at a time (very slow) and thus it does
not depend on the byte order of the machine.
@param[in]	buf	data over which to calculate CRC32
@param[in]	len	data length
@return CRC-32C (polynomial 0x11EDC6F41) */
static
uint32_t
ut_crc32_byte_by_byte_hw(
	const byte*	buf,
	ulint		len)
{
	uint64_t	crc = 0xFFFFFFFFU;

	ut_a(ut_crc32_cpu_enabled);

	while (len > 0) {
		ut_crc32_8_hw(&crc, &buf, &len);
	}

	return(~static_cast<uint32_t>(crc));
}
#endif /* defined(gnuc64) || defined(_WIN32) */

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
static
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
static
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
static
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
#if defined(gnuc64) || defined(_WIN32)
	ut_crc32_cpu_enabled = ut_crc32_check_cpu();

	if (ut_crc32_cpu_enabled) {
		ut_crc32 = ut_crc32_hw;
		ut_crc32_legacy_big_endian = ut_crc32_legacy_big_endian_hw;
		ut_crc32_byte_by_byte = ut_crc32_byte_by_byte_hw;
	}
#endif /* defined(gnuc64) || defined(_WIN32) */

	if (!ut_crc32_cpu_enabled) {
		ut_crc32_slice8_table_init();
		ut_crc32 = ut_crc32_sw;
		ut_crc32_legacy_big_endian = ut_crc32_legacy_big_endian_sw;
		ut_crc32_byte_by_byte = ut_crc32_byte_by_byte_sw;
	}
}
