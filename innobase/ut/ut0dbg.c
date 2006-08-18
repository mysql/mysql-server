/*********************************************************************
Debug utilities for Innobase.

(c) 1994, 1995 Innobase Oy

Created 1/30/1994 Heikki Tuuri
**********************************************************************/

#include "univ.i"

#if defined(__GNUC__) && (__GNUC__ > 2)
#else
/* This is used to eliminate compiler warnings */
ulint	ut_dbg_zero	= 0;
#endif

/* If this is set to TRUE all threads will stop into the next assertion
and assert */
ibool	ut_dbg_stop_threads	= FALSE;
#ifdef __NETWARE__ 
ibool panic_shutdown = FALSE;  	/* This is set to TRUE when on NetWare there
				happens an InnoDB assertion failure or other
				fatal error condition that requires an
				immediate shutdown. */
#else /* __NETWARE__ */
/* Null pointer used to generate memory trap */

ulint*	ut_dbg_null_ptr		= NULL;
#endif /* __NETWARE__ */

/*****************************************************************
Report a failed assertion. */

void
ut_dbg_assertion_failed(
/*====================*/
	const char* expr,	/* in: the failed assertion (optional) */
	const char* file,	/* in: source file containing the assertion */
	ulint line)		/* in: line number of the assertion */
{
	ut_print_timestamp(stderr);
	fprintf(stderr,
		"InnoDB: Assertion failure in thread %lu"
		" in file %s line %lu\n",
		os_thread_pf(os_thread_get_curr_id()), file, line);
	if (expr) {
		fprintf(stderr,
			"InnoDB: Failing assertion: %s\n", expr);
	}

	fputs(
"InnoDB: We intentionally generate a memory trap.\n"
"InnoDB: Submit a detailed bug report to http://bugs.mysql.com.\n"
"InnoDB: If you get repeated assertion failures or crashes, even\n"
"InnoDB: immediately after the mysqld startup, there may be\n"
"InnoDB: corruption in the InnoDB tablespace. Please refer to\n"
"InnoDB: http://dev.mysql.com/doc/refman/5.0/en/forcing-recovery.html\n"
"InnoDB: about forcing recovery.\n", stderr);
	ut_dbg_stop_threads = TRUE;
}

#ifdef __NETWARE__
/*****************************************************************
Shut down MySQL/InnoDB after assertion failure. */

void
ut_dbg_panic(void)
/*==============*/
{
	if (!panic_shutdown) {
		panic_shutdown = TRUE;
		innobase_shutdown_for_mysql();
	}
	exit(1);
}
#else /* __NETWARE__ */
/*****************************************************************
Stop a thread after assertion failure. */

void
ut_dbg_stop_thread(
/*===============*/
	const char*	file,
	ulint		line)
{
	fprintf(stderr, "InnoDB: Thread %lu stopped in file %s line %lu\n",
		os_thread_pf(os_thread_get_curr_id()), file, line);
	os_thread_sleep(1000000000);
}
#endif /* __NETWARE__ */
