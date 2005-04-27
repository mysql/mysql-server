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

extern ulint	ut_dbg_zero; /* This is used to eliminate
				compiler warnings */
extern ibool	ut_dbg_stop_threads;

extern ulint*	ut_dbg_null_ptr;

extern const char*	ut_dbg_msg_assert_fail;
extern const char*	ut_dbg_msg_trap;
extern const char*	ut_dbg_msg_stop;
/* Have a graceful exit on NetWare rather than a segfault to avoid abends */
#ifdef __NETWARE__
extern ibool 	panic_shutdown;
#define ut_a(EXPR) do {\
	if (!((ulint)(EXPR) + ut_dbg_zero)) {\
                ut_print_timestamp(stderr);\
	   	fprintf(stderr, ut_dbg_msg_assert_fail,\
		os_thread_pf(os_thread_get_curr_id()), __FILE__,\
                (ulint)__LINE__);\
		fputs("InnoDB: Failing assertion: " #EXPR "\n", stderr);\
		fputs(ut_dbg_msg_trap, stderr);\
		ut_dbg_stop_threads = TRUE;\
		if (ut_dbg_stop_threads) {\
        		fprintf(stderr, ut_dbg_msg_stop,\
	    os_thread_pf(os_thread_get_curr_id()), __FILE__, (ulint)__LINE__);\
	    	}\
		if(!panic_shutdown){\
		panic_shutdown = TRUE;\
		innobase_shutdown_for_mysql();}\
		exit(1);\
		}\
} while (0)
#define ut_error do {\
		ut_print_timestamp(stderr);\
		fprintf(stderr, ut_dbg_msg_assert_fail,\
		os_thread_pf(os_thread_get_curr_id()), __FILE__, (ulint)__LINE__);\
		fprintf(stderr, ut_dbg_msg_trap);\
		ut_dbg_stop_threads = TRUE;\
		if(!panic_shutdown){panic_shutdown = TRUE;\
		innobase_shutdown_for_mysql();}\
} while (0)
#else
#define ut_a(EXPR) do {\
	if (!((ulint)(EXPR) + ut_dbg_zero)) {\
                ut_print_timestamp(stderr);\
	   	fprintf(stderr, ut_dbg_msg_assert_fail,\
		os_thread_pf(os_thread_get_curr_id()), __FILE__,\
                (ulint)__LINE__);\
		fputs("InnoDB: Failing assertion: " #EXPR "\n", stderr);\
		fputs(ut_dbg_msg_trap, stderr);\
		ut_dbg_stop_threads = TRUE;\
		if (*(ut_dbg_null_ptr)) ut_dbg_null_ptr = NULL;\
	}\
	if (ut_dbg_stop_threads) {\
	        fprintf(stderr, ut_dbg_msg_stop,\
     os_thread_pf(os_thread_get_curr_id()), __FILE__, (ulint)__LINE__);\
		os_thread_sleep(1000000000);\
	}\
} while (0)

#define ut_error do {\
        ut_print_timestamp(stderr);\
	fprintf(stderr, ut_dbg_msg_assert_fail,\
	os_thread_pf(os_thread_get_curr_id()), __FILE__, (ulint)__LINE__);\
	fprintf(stderr, ut_dbg_msg_trap);\
	ut_dbg_stop_threads = TRUE;\
	if (*(ut_dbg_null_ptr)) ut_dbg_null_ptr = NULL;\
} while (0)
#endif

#ifdef UNIV_DEBUG
#define ut_ad(EXPR)  	ut_a(EXPR)
#define ut_d(EXPR)	do {EXPR;} while (0)
#else
#define ut_ad(EXPR)
#define ut_d(EXPR)
#endif

#define UT_NOT_USED(A)	A = A

#endif
