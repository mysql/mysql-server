/*********************************************************************
Debug utilities for Innobase

(c) 1994, 1995 Innobase Oy

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
# define UT_DBG_STOP	while (0)	/* We do not do this on NetWare */
#else /* __NETWARE__ */
/* Flag for indicating that all threads should stop.  This will be set
by ut_dbg_assertion_failed(). */
extern ibool	ut_dbg_stop_threads;

/* A null pointer that will be dereferenced to trigger a memory trap */
extern ulint*	ut_dbg_null_ptr;

/*****************************************************************
Stop a thread after assertion failure. */

void
ut_dbg_stop_thread(
/*===============*/
	const char*	file,
	ulint		line);

/* Abort the execution. */
# define UT_DBG_PANIC					\
	if (*(ut_dbg_null_ptr)) ut_dbg_null_ptr = NULL
/* Stop threads in ut_a(). */
# define UT_DBG_STOP do						\
	if (UNIV_UNLIKELY(ut_dbg_stop_threads)) {		\
		ut_dbg_stop_thread(__FILE__, (ulint) __LINE__);	\
	} while (0)
#endif /* __NETWARE__ */

/* Abort execution if EXPR does not evaluate to nonzero. */
#define ut_a(EXPR) do {						\
	if (UT_DBG_FAIL(EXPR)) {				\
		ut_dbg_assertion_failed(#EXPR,			\
				__FILE__, (ulint) __LINE__);	\
		UT_DBG_PANIC;					\
	}							\
	UT_DBG_STOP; 						\
} while (0)

/* Abort execution. */
#define ut_error do {						\
	ut_dbg_assertion_failed(0, __FILE__, (ulint) __LINE__);	\
	UT_DBG_PANIC;						\
} while (0)

#ifdef UNIV_DEBUG
#define ut_ad(EXPR)  	ut_a(EXPR)
#define ut_d(EXPR)	do {EXPR;} while (0)
#else
#define ut_ad(EXPR)
#define ut_d(EXPR)
#endif

#define UT_NOT_USED(A)	A = A

#endif
