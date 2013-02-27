/*****************************************************************************

Copyright (c) 1994, 2009, Oracle and/or its affiliates. All Rights Reserved.

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

#include "univ.i"
#include <stdlib.h>
#include "os0thread.h"

#if defined(__GNUC__) && (__GNUC__ > 2)
/** Test if an assertion fails.
@param EXPR	assertion expression
@return		nonzero if EXPR holds, zero if not */
# define UT_DBG_FAIL(EXPR) UNIV_UNLIKELY(!((ulint)(EXPR)))
#else
/** This is used to eliminate compiler warnings */
extern ulint	ut_dbg_zero;
/** Test if an assertion fails.
@param EXPR	assertion expression
@return		nonzero if EXPR holds, zero if not */
# define UT_DBG_FAIL(EXPR) !((ulint)(EXPR) + ut_dbg_zero)
#endif

/*************************************************************//**
Report a failed assertion. */
UNIV_INTERN
void
ut_dbg_assertion_failed(
/*====================*/
	const char*	expr,	/*!< in: the failed assertion */
	const char*	file,	/*!< in: source file containing the assertion */
	ulint		line)	/*!< in: line number of the assertion */
	UNIV_COLD __attribute__((nonnull(2)));

/** Abort the execution. */
# define UT_DBG_PANIC abort()

/** Abort execution if EXPR does not evaluate to nonzero.
@param EXPR	assertion expression that should hold */
#define ut_a(EXPR) do {						\
	if (UT_DBG_FAIL(EXPR)) {				\
		ut_dbg_assertion_failed(#EXPR,			\
				__FILE__, (ulint) __LINE__);	\
		UT_DBG_PANIC;					\
	}							\
} while (0)

/** Abort execution. */
#define ut_error do {						\
	ut_dbg_assertion_failed(0, __FILE__, (ulint) __LINE__);	\
	UT_DBG_PANIC;						\
} while (0)

#ifdef UNIV_DEBUG
/** Debug assertion. Does nothing unless UNIV_DEBUG is defined. */
#define ut_ad(EXPR)	ut_a(EXPR)
/** Debug statement. Does nothing unless UNIV_DEBUG is defined. */
#define ut_d(EXPR)	do {EXPR;} while (0)
#else
/** Debug assertion. Does nothing unless UNIV_DEBUG is defined. */
#define ut_ad(EXPR)
/** Debug statement. Does nothing unless UNIV_DEBUG is defined. */
#define ut_d(EXPR)
#endif

/** Silence warnings about an unused variable by doing a null assignment.
@param A	the unused variable */
#define UT_NOT_USED(A)	A = A

#ifdef UNIV_COMPILE_TEST_FUNCS

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

/** structure used for recording usage statistics */
struct speedo_t {
	struct rusage	ru;	/*!< getrusage() result */
	struct timeval	tv;	/*!< gettimeofday() result */
};

/*******************************************************************//**
Resets a speedo (records the current time in it). */
UNIV_INTERN
void
speedo_reset(
/*=========*/
	speedo_t*	speedo);	/*!< out: speedo */

/*******************************************************************//**
Shows the time elapsed and usage statistics since the last reset of a
speedo. */
UNIV_INTERN
void
speedo_show(
/*========*/
	const speedo_t*	speedo);	/*!< in: speedo */

#endif /* UNIV_COMPILE_TEST_FUNCS */

#endif /* !UNIV_INNOCHECKSUM */

#endif
