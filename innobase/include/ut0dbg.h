/*********************************************************************
Debug utilities for Innobase

(c) 1994, 1995 Innobase Oy

Created 1/30/1994 Heikki Tuuri
**********************************************************************/

#ifndef ut0dbg_h
#define ut0dbg_h

#include "univ.i"
#include <assert.h>
#include <stdlib.h>
#include "os0thread.h"

extern ulint	ut_dbg_zero; /* This is used to eliminate
				compiler warnings */
extern ibool	ut_dbg_stop_threads;

extern ulint*	ut_dbg_null_ptr;

extern const char*	ut_dbg_msg_assert_fail;
extern const char*	ut_dbg_msg_trap;
extern const char*	ut_dbg_msg_stop;

#define ut_a(EXPR)\
	if (!((ulint)(EXPR) + ut_dbg_zero)) {\
                ut_print_timestamp(stderr);\
	   	fprintf(stderr, ut_dbg_msg_assert_fail,\
		os_thread_pf(os_thread_get_curr_id()), IB__FILE__,\
                (ulint)__LINE__);\
		fputs("InnoDB: Failing assertion: " #EXPR "\n", stderr);\
		fputs(ut_dbg_msg_trap, stderr);\
		ut_dbg_stop_threads = TRUE;\
		(*ut_dbg_null_ptr)++;\
	}\
	if (ut_dbg_stop_threads) {\
	        fprintf(stderr, ut_dbg_msg_stop,\
     os_thread_pf(os_thread_get_curr_id()), IB__FILE__, (ulint)__LINE__);\
		os_thread_sleep(1000000000);\
	}

#define ut_error\
        ut_print_timestamp(stderr);\
	fprintf(stderr, ut_dbg_msg_assert_fail,\
	os_thread_pf(os_thread_get_curr_id()), IB__FILE__, (ulint)__LINE__);\
	fprintf(stderr, ut_dbg_msg_trap);\
	ut_dbg_stop_threads = TRUE;\
	(*ut_dbg_null_ptr)++;

#ifdef UNIV_DEBUG
#define ut_ad(EXPR)  	ut_a(EXPR)
#define ut_d(EXPR)	{EXPR;}
#else
#define ut_ad(EXPR)
#define ut_d(EXPR)
#endif


#define UT_NOT_USED(A)	A = A







#endif

