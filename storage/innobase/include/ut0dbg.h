/*****************************************************************************

Copyright (c) 1994, 2013, Oracle and/or its affiliates. All Rights Reserved.

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

/*****************************************************************//**
@file include/ut0dbg.h
Debug utilities for Innobase

Created 1/30/1994 Heikki Tuuri
**********************************************************************/

#ifndef ut0dbg_h
#define ut0dbg_h

#ifdef UNIV_INNOCHECKSUM
#define ut_a		assert
#define ut_ad		assert
#define ut_error	assert(0)
#else /* !UNIV_INNOCHECKSUM */

/* Do not include univ.i because univ.i includes this. */

#include "os0thread.h"

/*************************************************************//**
Report a failed assertion. */

void
ut_dbg_assertion_failed(
/*====================*/
	const char*	expr,	/*!< in: the failed assertion */
	const char*	file,	/*!< in: source file containing the assertion */
	ulint		line)	/*!< in: line number of the assertion */
	UNIV_COLD __attribute__((nonnull(2), noreturn));

/** Abort execution if EXPR does not evaluate to nonzero.
@param EXPR assertion expression that should hold */
#define ut_a(EXPR) do {						\
	if (UNIV_UNLIKELY(!(ulint) (EXPR))) {			\
		ut_dbg_assertion_failed(#EXPR,			\
				__FILE__, (ulint) __LINE__);	\
	}							\
} while (0)

/** Abort execution. */
#define ut_error						\
	ut_dbg_assertion_failed(0, __FILE__, (ulint) __LINE__)

#ifdef UNIV_DEBUG
/** Debug assertion. Does nothing unless UNIV_DEBUG is defined. */
#define ut_ad(EXPR)	ut_a(EXPR)
/** Debug statement. Does nothing unless UNIV_DEBUG is defined. */
#define ut_d(EXPR)	EXPR
#else
/** Debug assertion. Does nothing unless UNIV_DEBUG is defined. */
#define ut_ad(EXPR)
/** Debug statement. Does nothing unless UNIV_DEBUG is defined. */
#define ut_d(EXPR)
#endif

/** Silence warnings about an unused variable by doing a null assignment.
@param A the unused variable */
#define UT_NOT_USED(A)	A = A

#ifdef UNIV_COMPILE_TEST_FUNCS
#include "univ.i"
#if defined(HAVE_SYS_TYPES_H) && defined(HAVE_SYS_TIME_H) && defined(HAVE_RESOURCE_H)

/** structure used for recording usage statistics */
struct speedo_t {
	struct rusage	ru;	/*!< getrusage() result */
	struct timeval	tv;	/*!< gettimeofday() result */
};

/*******************************************************************//**
Resets a speedo (records the current time in it). */

void
speedo_reset(
/*=========*/
	speedo_t*	speedo);	/*!< out: speedo */

/*******************************************************************//**
Shows the time elapsed and usage statistics since the last reset of a
speedo. */

void
speedo_show(
/*========*/
	const speedo_t*	speedo);	/*!< in: speedo */

#endif /* HAVE_SYS_TYPES_H && HAVE_SYS_TIME_H && HAVE_RESOURCE_H */

#endif /* UNIV_COMPILE_TEST_FUNCS */

#endif /* !UNIV_INNOCHECKSUM */

#endif
