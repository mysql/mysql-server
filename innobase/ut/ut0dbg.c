/*********************************************************************
Debug utilities for Innobase.

(c) 1994, 1995 Innobase Oy

Created 1/30/1994 Heikki Tuuri
**********************************************************************/

#include "univ.i"

/* This is used to eliminate compiler warnings */
ulint	ut_dbg_zero	= 0;

/* If this is set to TRUE all threads will stop into the next assertion
and assert */
ibool	ut_dbg_stop_threads	= FALSE;
#ifdef __NETWARE__ 
ibool panic_shutdown = FALSE;  	/* This is set to TRUE when on NetWare there
				happens an InnoDB assertion failure or other
				fatal error condition that requires an
				immediate shutdown. */
#endif
/* Null pointer used to generate memory trap */

ulint*	ut_dbg_null_ptr		= NULL;

const char*	ut_dbg_msg_assert_fail =
"InnoDB: Assertion failure in thread %lu in file %s line %lu\n";
const char*	ut_dbg_msg_trap =
"InnoDB: We intentionally generate a memory trap.\n"
"InnoDB: Submit a detailed bug report to http://bugs.mysql.com.\n"
"InnoDB: If you get repeated assertion failures or crashes, even\n"
"InnoDB: immediately after the mysqld startup, there may be\n"
"InnoDB: corruption in the InnoDB tablespace. See section 6.1 of\n"
"InnoDB: http://www.innodb.com/ibman.php about forcing recovery.\n";

const char*	ut_dbg_msg_stop =
"InnoDB: Thread %lu stopped in file %s line %lu\n";
