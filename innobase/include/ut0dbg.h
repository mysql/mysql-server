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

				
#define ut_a(EXPR)\
{\
	ulint	dbg_i;\
\
	if (!((ulint)(EXPR) + ut_dbg_zero)) {\
	   	fprintf(stderr,\
       "InnoDB: Assertion failure in thread %lu in file %s line %lu\n",\
			os_thread_get_curr_id(), IB__FILE__, (ulint)__LINE__);\
	   	fprintf(stderr,\
       "InnoDB: We intentionally generate a memory trap.\n");\
                fprintf(stderr,\
       "InnoDB: Send a detailed bug report to mysql@lists.mysql.com\n");\
		ut_dbg_stop_threads = TRUE;\
		dbg_i = *(ut_dbg_null_ptr);\
	   	if (dbg_i) {\
			ut_dbg_null_ptr = NULL;\
		}\
	}\
	if (ut_dbg_stop_threads) {\
	        fprintf(stderr,\
                     "InnoDB: Thread %lu stopped in file %s line %lu\n",\
			os_thread_get_curr_id(), IB__FILE__, (ulint)__LINE__);\
		os_thread_sleep(1000000000);\
	}\
}

#define ut_error {\
	ulint	dbg_i;\
	   fprintf(stderr,\
	  "InnoDB: Assertion failure in thread %lu in file %s line %lu\n",\
			os_thread_get_curr_id(), IB__FILE__, (ulint)__LINE__);\
	   fprintf(stderr,\
		   "InnoDB: We intentionally generate a memory trap.\n");\
           fprintf(stderr,\
            "InnoDB: Send a detailed bug report to mysql@lists.mysql.com\n");\
	   ut_dbg_stop_threads = TRUE;\
	   dbg_i = *(ut_dbg_null_ptr);\
	   printf("%lu", dbg_i);\
}

#ifdef UNIV_DEBUG
#define ut_ad(EXPR)  	ut_a(EXPR)
#define ut_d(EXPR)	{EXPR;}
#else
#define ut_ad(EXPR)
#define ut_d(EXPR)
#endif


#define UT_NOT_USED(A)	A = A







#endif

