/*
   Copyright (c) 2005, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */

/*
  InnoDB offline file checksum utility.  85% of the code in this file
  was taken wholesale fron the InnoDB codebase.

  The final 15% was originally written by Mark Smith of Danga
  Interactive, Inc. <junior@danga.com>

  Published with a permission.
*/

#ifndef INNOCHECKSUM_SOLARIS

/* On non-Solaris we can link with libinnobase.a and the linker does not
complain about undefined symbols in libinnobase.a (the ones that are defined
in mysql code). */

#include "univ.i"
#include "buf0checksum.h" /* buf_calc_page_*() */
#include "fil0fil.h" /* FIL_* */
#include "mach0data.h" /* mach_read_from_4() */
#include "ut0crc32.h" /* ut_crc32_init() */

#else /* INNOCHECKSUM_SOLARIS */

/* Unfortunately on Solaris the linker seems to be too picky, requiring all
symbols to be defined. Using "-z nodefs" and the innochecksum binary gets
compiled but it cannot be executed later. */

#include <assert.h>
#include <stdio.h>
#include <time.h>

#define ut_a		assert
#define ut_ad		assert
#define ut_error	assert(0)

/* univ.i { */
# ifndef __WIN__
#  ifndef UNIV_HOTBACKUP
#   include "config.h"
#  endif /* UNIV_HOTBACKUP */
# endif

#if defined(__GNUC__) && (__GNUC__ >= 4) && !defined(sun) || defined(__INTEL_COMPILER)
# define UNIV_INTERN __attribute__((visibility ("hidden")))
#else
# define UNIV_INTERN
#endif

#ifdef __WIN__
# define UNIV_INLINE	__inline
#elif defined(__SUNPRO_CC) || defined(__SUNPRO_C)
# define UNIV_INLINE static inline
#else
# define UNIV_INLINE static __inline__
#endif

/** The 2-logarithm of UNIV_PAGE_SIZE: */
#define UNIV_PAGE_SIZE_SHIFT	14

/** The universal page size of the database */
#define UNIV_PAGE_SIZE		(1 << UNIV_PAGE_SIZE_SHIFT)

/* Note that inside MySQL 'byte' is defined as char on Linux! */
#define byte			unsigned char

/* Define an unsigned integer type that is exactly 32 bits. */

#if SIZEOF_INT == 4
typedef unsigned int		ib_uint32_t;
#define UINT32PF		"%u"
#elif SIZEOF_LONG == 4
typedef unsigned long		ib_uint32_t;
#define UINT32PF		"%lu"
#else
#error "Neither int or long is 4 bytes"
#endif

/* Another basic type we use is unsigned long integer which should be equal to
the word size of the machine, that is on a 32-bit platform 32 bits, and on a
64-bit platform 64 bits. We also give the printf format for the type as a
macro ULINTPF. */

#ifdef _WIN64
typedef unsigned __int64	ulint;
#define ULINTPF			"%I64u"
typedef __int64			lint;
#else
typedef unsigned long int	ulint;
#define ULINTPF			"%lu"
typedef long int		lint;
#endif

#ifdef __WIN__
typedef __int64			ib_int64_t;
typedef unsigned __int64	ib_uint64_t;
#elif !defined(UNIV_HOTBACKUP)
/** Note: longlong and ulonglong come from MySQL headers. */
typedef long long		ib_int64_t;
typedef unsigned long long	ib_uint64_t;
#endif

/** This 'ibool' type is used within Innobase. Remember that different included
headers may define 'bool' differently. Do not assume that 'bool' is a ulint! */
#define ibool			ulint

#ifndef TRUE

#define TRUE    1
#define FALSE   0

#endif
/* } univ.i */

/* fil0fil.h { */
#define FIL_PAGE_SPACE_OR_CHKSUM 0	/*!< in < MySQL-4.0.14 space id the
					page belongs to (== 0) but in later
					versions the 'new' checksum of the
					page */
#define FIL_PAGE_OFFSET		4	/*!< page offset inside space */
#define FIL_PAGE_LSN		16	/*!< lsn of the end of the newest
					modification log record to the page */
#define FIL_PAGE_FILE_FLUSH_LSN	26	/*!< this is only defined for the
					first page in a system tablespace
					data file (ibdata*, not *.ibd):
					the file has been flushed to disk
					at least up to this lsn */
#define FIL_PAGE_DATA		38	/*!< start of the data on the page */
#define FIL_PAGE_END_LSN_OLD_CHKSUM 8	/*!< the low 4 bytes of this are used
					to store the page checksum, the
					last 4 bytes should be identical
					to the last 4 bytes of FIL_PAGE_LSN */
/* } fil0fil.h */

/* mach0data.ic { */
/********************************************************//**
The following function is used to fetch data from 4 consecutive
bytes. The most significant byte is at the lowest address.
@return	ulint integer */
UNIV_INLINE
ulint
mach_read_from_4(
/*=============*/
	const byte*	b)	/*!< in: pointer to four bytes */
{
	ut_ad(b);
	return( ((ulint)(b[0]) << 24)
		| ((ulint)(b[1]) << 16)
		| ((ulint)(b[2]) << 8)
		| (ulint)(b[3])
		);
}
/* } mach0data.ic */

/* ut0ut.c { */
/**********************************************************//**
Prints a timestamp to a file. */
UNIV_INTERN
void
ut_print_timestamp(
/*===============*/
	FILE*  file) /*!< in: file where to print */
{
#ifdef __WIN__
	SYSTEMTIME cal_tm;

	GetLocalTime(&cal_tm);

	fprintf(file,"%02d%02d%02d %2d:%02d:%02d",
		(int)cal_tm.wYear % 100,
		(int)cal_tm.wMonth,
		(int)cal_tm.wDay,
		(int)cal_tm.wHour,
		(int)cal_tm.wMinute,
		(int)cal_tm.wSecond);
#else
	struct tm* cal_tm_ptr;
	time_t	   tm;

#ifdef HAVE_LOCALTIME_R
	struct tm  cal_tm;
	time(&tm);
	localtime_r(&tm, &cal_tm);
	cal_tm_ptr = &cal_tm;
#else
	time(&tm);
	cal_tm_ptr = localtime(&tm);
#endif
	fprintf(file,"%02d%02d%02d %2d:%02d:%02d",
		cal_tm_ptr->tm_year % 100,
		cal_tm_ptr->tm_mon + 1,
		cal_tm_ptr->tm_mday,
		cal_tm_ptr->tm_hour,
		cal_tm_ptr->tm_min,
		cal_tm_ptr->tm_sec);
#endif
}
/* } ut0ut.c */

/* ut0crc32.h { */
/********************************************************************//**
Calculates CRC32.
@return CRC32 (CRC-32C, using the GF(2) primitive polynomial 0x11EDC6F41,
or 0x1EDC6F41 without the high-order bit) */
UNIV_INTERN
ib_uint32_t
(*ut_crc32)(
/*========*/
	const byte*	buf,	/*!< in: data over which to calculate CRC32 */
	ulint		len);	/*!< in: data length */
/* } ut0crc32.h */

/* ut0crc32.c { */
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
@file ut/ut0crc32.c
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
   this program; if not, write to the Free Software Foundation, Inc., 59 Temple
   Place, Suite 330, Boston, MA  02111-1307  USA */

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

#include <string.h> /* memcmp() */

/* Precalculated table used to generate the CRC32 if the CPU does not
have support for it */
static ib_uint32_t	ut_crc32_slice8_table[8][256];
static ibool		ut_crc32_slice8_table_initialized = FALSE;

/* Flag that tells whether the CPU supports CRC32 or not */
static ibool		ut_crc32_sse2_enabled = FALSE;

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

	if (memcmp(vend, "GenuineIntel", 12) == 0 ||
	    (memcmp(vend, "AuthenticAMD", 12) == 0 && *family == 0xF)) {

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

	ut_print_timestamp(stderr);
	fprintf(stderr, " InnoDB: CPU %s crc32 instructions\n",
		ut_crc32_sse2_enabled ? "supports" : "does not support");
}
/* } ut0crc32.c */

/* ut0rnd.ic { */
#define UT_HASH_RANDOM_MASK	1463735687
#define UT_HASH_RANDOM_MASK2	1653893711
/*************************************************************//**
Folds a pair of ulints.
@return	folded value */
UNIV_INLINE
ulint
ut_fold_ulint_pair(
/*===============*/
	ulint	n1,	/*!< in: ulint */
	ulint	n2)	/*!< in: ulint */
{
	return(((((n1 ^ n2 ^ UT_HASH_RANDOM_MASK2) << 8) + n1)
		^ UT_HASH_RANDOM_MASK) + n2);
}

/*************************************************************//**
Folds a binary string.
@return	folded value */
UNIV_INLINE
ulint
ut_fold_binary(
/*===========*/
	const byte*	str,	/*!< in: string of bytes */
	ulint		len)	/*!< in: length */
{
	ulint		fold = 0;
	const byte*	str_end	= str + (len & 0xFFFFFFF8);

	ut_ad(str || !len);

	while (str < str_end) {
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
	}

	switch (len & 0x7) {
	case 7:
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
	case 6:
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
	case 5:
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
	case 4:
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
	case 3:
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
	case 2:
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
	case 1:
		fold = ut_fold_ulint_pair(fold, (ulint)(*str++));
	}

	return(fold);
}
/* } ut0rnd.ic */

/* buf0checksum.c { */
/********************************************************************//**
Calculates a page CRC32 which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value on
32-bit and 64-bit architectures.
@return	checksum */
UNIV_INTERN
ib_uint32_t
buf_calc_page_crc32(
/*================*/
	const byte*	page)	/*!< in: buffer page */
{
	ib_uint32_t	checksum;

	/* Since the field FIL_PAGE_FILE_FLUSH_LSN, and in versions <= 4.1.x
	FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, are written outside the buffer pool
	to the first pages of data files, we have to skip them in the page
	checksum calculation.
	We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
	checksum is stored, and also the last 8 bytes of page because
	there we store the old formula checksum. */

	checksum = ut_crc32(page + FIL_PAGE_OFFSET,
			    FIL_PAGE_FILE_FLUSH_LSN - FIL_PAGE_OFFSET)
		^ ut_crc32(page + FIL_PAGE_DATA,
			   UNIV_PAGE_SIZE - FIL_PAGE_DATA
			   - FIL_PAGE_END_LSN_OLD_CHKSUM);

	return(checksum);
}

/********************************************************************//**
Calculates a page checksum which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value on
32-bit and 64-bit architectures.
@return	checksum */
UNIV_INTERN
ulint
buf_calc_page_new_checksum(
/*=======================*/
	const byte*	page)	/*!< in: buffer page */
{
	ulint checksum;

	/* Since the field FIL_PAGE_FILE_FLUSH_LSN, and in versions <= 4.1.x
	FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, are written outside the buffer pool
	to the first pages of data files, we have to skip them in the page
	checksum calculation.
	We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
	checksum is stored, and also the last 8 bytes of page because
	there we store the old formula checksum. */

	checksum = ut_fold_binary(page + FIL_PAGE_OFFSET,
				  FIL_PAGE_FILE_FLUSH_LSN - FIL_PAGE_OFFSET)
		+ ut_fold_binary(page + FIL_PAGE_DATA,
				 UNIV_PAGE_SIZE - FIL_PAGE_DATA
				 - FIL_PAGE_END_LSN_OLD_CHKSUM);
	checksum = checksum & 0xFFFFFFFFUL;

	return(checksum);
}

/********************************************************************//**
In versions < 4.0.14 and < 4.1.1 there was a bug that the checksum only
looked at the first few bytes of the page. This calculates that old
checksum.
NOTE: we must first store the new formula checksum to
FIL_PAGE_SPACE_OR_CHKSUM before calculating and storing this old checksum
because this takes that field as an input!
@return	checksum */
UNIV_INTERN
ulint
buf_calc_page_old_checksum(
/*=======================*/
	const byte*	page)	/*!< in: buffer page */
{
	ulint checksum;

	checksum = ut_fold_binary(page, FIL_PAGE_FILE_FLUSH_LSN);

	checksum = checksum & 0xFFFFFFFFUL;

	return(checksum);
}
/* } buf0checksum.c */

#endif /* INNOCHECKSUM_SOLARIS */


#include <my_global.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* command line argument to do page checks (that's it) */
/* another argument to specify page ranges... seek to right spot and go from there */

#ifndef INNOCHECKSUM_SOLARIS
void
ut_dbg_assertion_failed(
/*====================*/
        const char* expr,       /*!< in: the failed assertion (optional) */
        const char* file,       /*!< in: source file containing the assertion */
        ulint line)             /*!< in: line number of the assertion */
{
	fprintf(stderr, "innochecksum(%s:%u): Assertion %s failed.\n",
		file, (unsigned) line, expr ? expr : "");
}

void
ut_print_timestamp(
/*===============*/
	FILE*	file __attribute__((unused)))	/*!< in: file where to print */
{
}
#endif /* INNOCHECKSUM_SOLARIS */

int main(int argc, char **argv)
{
  FILE *f;                     /* our input file */
  byte *p;                     /* storage of pages read */
  int bytes;                   /* bytes read count */
  ulint ct;                    /* current page number (0 based) */
  int now;                     /* current time */
  int lastt;                   /* last time */
  ulint oldcsum, oldcsumfield, csum, csumfield, crc32, logseq, logseqfield; /* ulints for checksum storage */
  struct stat st;              /* for stat, if you couldn't guess */
  unsigned long long int size; /* size of file (has to be 64 bits) */
  ulint pages;                 /* number of pages in file */
  ulint start_page= 0, end_page= 0, use_end_page= 0; /* for starting and ending at certain pages */
  off_t offset= 0;
  int just_count= 0;          /* if true, just print page count */
  int verbose= 0;
  int debug= 0;
  int c;
  int fd;

  ut_crc32_init();

  /* remove arguments */
  while ((c= getopt(argc, argv, "cvds:e:p:")) != -1)
  {
    switch (c)
    {
    case 'v':
      verbose= 1;
      break;
    case 'c':
      just_count= 1;
      break;
    case 's':
      start_page= atoi(optarg);
      break;
    case 'e':
      end_page= atoi(optarg);
      use_end_page= 1;
      break;
    case 'p':
      start_page= atoi(optarg);
      end_page= atoi(optarg);
      use_end_page= 1;
      break;
    case 'd':
      debug= 1;
      break;
    case ':':
      fprintf(stderr, "option -%c requires an argument\n", optopt);
      return 1;
      break;
    case '?':
      fprintf(stderr, "unrecognized option: -%c\n", optopt);
      return 1;
      break;
    }
  }

  /* debug implies verbose... */
  if (debug) verbose= 1;

  /* make sure we have the right arguments */
  if (optind >= argc)
  {
    printf("InnoDB offline file checksum utility.\n");
    printf("usage: %s [-c] [-s <start page>] [-e <end page>] [-p <page>] [-v] [-d] <filename>\n", argv[0]);
    printf("\t-c\tprint the count of pages in the file\n");
    printf("\t-s n\tstart on this page number (0 based)\n");
    printf("\t-e n\tend at this page number (0 based)\n");
    printf("\t-p n\tcheck only this page (0 based)\n");
    printf("\t-v\tverbose (prints progress every 5 seconds)\n");
    printf("\t-d\tdebug mode (prints checksums for each page)\n");
    return 1;
  }

  /* stat the file to get size and page count */
  if (stat(argv[optind], &st))
  {
    perror("error statting file");
    return 1;
  }
  size= st.st_size;
  pages= size / UNIV_PAGE_SIZE;
  if (just_count)
  {
    printf("%lu\n", pages);
    return 0;
  }
  else if (verbose)
  {
    printf("file %s = %llu bytes (%lu pages)...\n", argv[optind], size, pages);
    printf("checking pages in range %lu to %lu\n", start_page, use_end_page ? end_page : (pages - 1));
  }

  /* open the file for reading */
  f= fopen(argv[optind], "r");
  if (!f)
  {
    perror("error opening file");
    return 1;
  }

  /* seek to the necessary position */
  if (start_page)
  {
    fd= fileno(f);
    if (!fd)
    {
      perror("unable to obtain file descriptor number");
      return 1;
    }

    offset= (off_t)start_page * (off_t)UNIV_PAGE_SIZE;

    if (lseek(fd, offset, SEEK_SET) != offset)
    {
      perror("unable to seek to necessary offset");
      return 1;
    }
  }

  /* allocate buffer for reading (so we don't realloc every time) */
  p= (byte*)malloc(UNIV_PAGE_SIZE);

  /* main checksumming loop */
  ct= start_page;
  lastt= 0;
  while (!feof(f))
  {
    bytes= fread(p, 1, UNIV_PAGE_SIZE, f);
    if (!bytes && feof(f)) return 0;
    if (bytes != UNIV_PAGE_SIZE)
    {
      fprintf(stderr, "bytes read (%d) doesn't match universal page size (%d)\n", bytes, UNIV_PAGE_SIZE);
      return 1;
    }

    /* check the "stored log sequence numbers" */
    logseq= mach_read_from_4(p + FIL_PAGE_LSN + 4);
    logseqfield= mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM + 4);
    if (debug)
      printf("page %lu: log sequence number: first = %lu; second = %lu\n", ct, logseq, logseqfield);
    if (logseq != logseqfield)
    {
      fprintf(stderr, "page %lu invalid (fails log sequence number check)\n", ct);
      return 1;
    }

    /* check old method of checksumming */
    oldcsum= buf_calc_page_old_checksum(p);
    oldcsumfield= mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM);
    if (debug)
      printf("page %lu: old style: calculated = %lu; recorded = %lu\n", ct, oldcsum, oldcsumfield);
    if (oldcsumfield != mach_read_from_4(p + FIL_PAGE_LSN) && oldcsumfield != oldcsum)
    {
      fprintf(stderr, "page %lu invalid (fails old style checksum)\n", ct);
      return 1;
    }

    /* now check the new method */
    csum= buf_calc_page_new_checksum(p);
    crc32= buf_calc_page_crc32(p);
    csumfield= mach_read_from_4(p + FIL_PAGE_SPACE_OR_CHKSUM);
    if (debug)
      printf("page %lu: new style: calculated = %lu; crc32 = %lu; recorded = %lu\n",
          ct, csum, crc32, csumfield);
    if (csumfield != 0 && crc32 != csumfield && csum != csumfield)
    {
      fprintf(stderr, "page %lu invalid (fails innodb and crc32 checksum)\n", ct);
      return 1;
    }

    /* end if this was the last page we were supposed to check */
    if (use_end_page && (ct >= end_page))
      return 0;

    /* do counter increase and progress printing */
    ct++;
    if (verbose)
    {
      if (ct % 64 == 0)
      {
        now= time(0);
        if (!lastt) lastt= now;
        if (now - lastt >= 1)
        {
          printf("page %lu okay: %.3f%% done\n", (ct - 1), (float) ct / pages * 100);
          lastt= now;
        }
      }
    }
  }
  return 0;
}

