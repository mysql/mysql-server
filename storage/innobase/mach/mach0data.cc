/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

/******************************************************************//**
@file mach/mach0data.cc
Utilities for converting data from the database file
to the machine format.

Created 11/28/1995 Heikki Tuuri
***********************************************************************/

#include "mach0data.h"

#ifdef UNIV_NONINL
#include "mach0data.ic"
#endif

/** Read a 32-bit integer in a compressed form.
@param[in,out]	ptr	pointer to memory where to read;
advanced by the number of bytes consumed, or set NULL if out of space
@param[in]	end_ptr	end of the buffer
@return unsigned value */
ib_uint32_t
mach_parse_compressed(
	const byte**	ptr,
	const byte*	end_ptr)
{
	ulint	val;

	if (*ptr >= end_ptr) {
		*ptr = NULL;
		return(0);
	}

	val = mach_read_from_1(*ptr);

	if (val < 0x80) {
		/* 0nnnnnnn (7 bits) */
		++*ptr;
		return(static_cast<ib_uint32_t>(val));
	}

	/* Workaround GCC bug
	https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77673:
	the compiler moves mach_read_from_4 right to the beginning of the
	function, causing and out-of-bounds read if we are reading a short
	integer close to the end of buffer. */
#if defined(__GNUC__) && (__GNUC__ >= 5) && !defined(__clang__)
#define DEPLOY_FENCE
#endif

#ifdef DEPLOY_FENCE
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif

	if (val < 0xC0) {
		/* 10nnnnnn nnnnnnnn (14 bits) */
		if (end_ptr >= *ptr + 2) {
			val = mach_read_from_2(*ptr) & 0x3FFF;
			ut_ad(val > 0x7F);
			*ptr += 2;
			return(static_cast<ib_uint32_t>(val));
		}
		*ptr = NULL;
		return(0);
	}

#ifdef DEPLOY_FENCE
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif

	if (val < 0xE0) {
		/* 110nnnnn nnnnnnnn nnnnnnnn (21 bits) */
		if (end_ptr >= *ptr + 3) {
			val = mach_read_from_3(*ptr) & 0x1FFFFF;
			ut_ad(val > 0x3FFF);
			*ptr += 3;
			return(static_cast<ib_uint32_t>(val));
		}
		*ptr = NULL;
		return(0);
	}

#ifdef DEPLOY_FENCE
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif

	if (val < 0xF0) {
		/* 1110nnnn nnnnnnnn nnnnnnnn nnnnnnnn (28 bits) */
		if (end_ptr >= *ptr + 4) {
			val = mach_read_from_4(*ptr) & 0xFFFFFFF;
			ut_ad(val > 0x1FFFFF);
			*ptr += 4;
			return(static_cast<ib_uint32_t>(val));
		}
		*ptr = NULL;
		return(0);
	}

#ifdef DEPLOY_FENCE
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif

#undef DEPLOY_FENCE

	ut_ad(val == 0xF0);

	/* 11110000 nnnnnnnn nnnnnnnn nnnnnnnn nnnnnnnn (32 bits) */
	if (end_ptr >= *ptr + 5) {
		val = mach_read_from_4(*ptr + 1);
		ut_ad(val > 0xFFFFFFF);
		*ptr += 5;
		return(static_cast<ib_uint32_t>(val));
	}

	*ptr = NULL;
	return(0);
}
