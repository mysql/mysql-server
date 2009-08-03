/*****************************************************************************

Copyright (c) 1994, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/*********************************************************************
Debug utilities for Innobase

Created 1/30/1994 Heikki Tuuri
**********************************************************************/

#ifndef ut0dbg_h
#define ut0dbg_h

#include "univ.i"
#include <stdlib.h>
#include "os0thread.h"

#if defined(__GNUC__) && (__GNUC__ > 2)
# define UT_DBG_FAIL(EXPR) UNIV_UNLIKELY(!((ulint)(EXPR)))
#else
extern ulint	ut_dbg_zero; /* This is used to eliminate
				compiler warnings */
# define UT_DBG_FAIL(EXPR) !((ulint)(EXPR) + ut_dbg_zero)
#endif

/*****************************************************************
Report a failed assertion. */
UNIV_INTERN
void
ut_dbg_assertion_failed(
/*====================*/
	const char* expr,	/* in: the failed assertion */
	const char* file,	/* in: source file containing the assertion */
	ulint line);		/* in: line number of the assertion */

#ifdef __NETWARE__
/* Flag for ignoring further assertion failures.
On NetWare, have a graceful exit rather than a segfault to avoid abends. */
extern ibool	panic_shutdown;
/* Abort the execution. */
void ut_dbg_panic(void);
# define UT_DBG_PANIC ut_dbg_panic()
/* Stop threads in ut_a(). */
# define UT_DBG_STOP	do {} while (0)	/* We do not do this on NetWare */
#else /* __NETWARE__ */
# if defined(__WIN__) || defined(__INTEL_COMPILER)
#  undef UT_DBG_USE_ABORT
# elif defined(__GNUC__) && (__GNUC__ > 2)
#  define UT_DBG_USE_ABORT
# endif

# ifndef UT_DBG_USE_ABORT
/* A null pointer that will be dereferenced to trigger a memory trap */
extern ulint*	ut_dbg_null_ptr;
# endif

# if defined(UNIV_SYNC_DEBUG) || !defined(UT_DBG_USE_ABORT)
/* Flag for indicating that all threads should stop.  This will be set
by ut_dbg_assertion_failed(). */
extern ibool	ut_dbg_stop_threads;

/*****************************************************************
Stop a thread after assertion failure. */
UNIV_INTERN
void
ut_dbg_stop_thread(
/*===============*/
	const char*	file,
	ulint		line);
# endif

# ifdef UT_DBG_USE_ABORT
/* Abort the execution. */
#  define UT_DBG_PANIC abort()
/* Stop threads (null operation) */
#  define UT_DBG_STOP do {} while (0)
# else /* UT_DBG_USE_ABORT */
/* Abort the execution. */
#  define UT_DBG_PANIC					\
	if (*(ut_dbg_null_ptr)) ut_dbg_null_ptr = NULL
/* Stop threads in ut_a(). */
#  define UT_DBG_STOP do						\
	if (UNIV_UNLIKELY(ut_dbg_stop_threads)) {		\
		ut_dbg_stop_thread(__FILE__, (ulint) __LINE__);	\
	} while (0)
# endif /* UT_DBG_USE_ABORT */
#endif /* __NETWARE__ */

/* Abort execution if EXPR does not evaluate to nonzero. */
#define ut_a(EXPR) do {						\
	if (UT_DBG_FAIL(EXPR)) {				\
		ut_dbg_assertion_failed(#EXPR,			\
				__FILE__, (ulint) __LINE__);	\
		UT_DBG_PANIC;					\
	}							\
	UT_DBG_STOP;						\
} while (0)

/* Abort execution. */
#define ut_error do {						\
	ut_dbg_assertion_failed(0, __FILE__, (ulint) __LINE__);	\
	UT_DBG_PANIC;						\
} while (0)

#ifdef UNIV_DEBUG
#define ut_ad(EXPR)	ut_a(EXPR)
#define ut_d(EXPR)	do {EXPR;} while (0)
#else
#define ut_ad(EXPR)
#define ut_d(EXPR)
#endif

#define UT_NOT_USED(A)	A = A

#ifdef UNIV_COMPILE_TEST_FUNCS

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

/* structure used for recording usage statistics */
typedef struct speedo_struct {
	struct rusage	ru;
	struct timeval	tv;
} speedo_t;

/***********************************************************************
Resets a speedo (records the current time in it). */
UNIV_INTERN
void
speedo_reset(
/*=========*/
	speedo_t*	speedo);	/* out: speedo */

/***********************************************************************
Shows the time elapsed and usage statistics since the last reset of a
speedo. */
UNIV_INTERN
void
speedo_show(
/*========*/
	const speedo_t*	speedo);	/* in: speedo */

#endif /* UNIV_COMPILE_TEST_FUNCS */

#endif
