/******************************************************
The interface to the operating system
process control primitives

(c) 1995 Innobase Oy

Created 9/30/1995 Heikki Tuuri
*******************************************************/

#include "os0proc.h"
#ifdef UNIV_NONINL
#include "os0proc.ic"
#endif

#ifdef __WIN__
#include <windows.h>
#endif

#include "ut0mem.h"

/********************************************************************
Converts the current process id to a number. It is not guaranteed that the
number is unique. In Linux returns the 'process number' of the current
thread. That number is the same as one sees in 'top', for example. In Linux
the thread id is not the same as one sees in 'top'. */

ulint
os_proc_get_number(void)
/*====================*/
{
#ifdef __WIN__
	return((ulint)GetCurrentProcessId());
#else
	return((ulint)getpid());
#endif
}

/********************************************************************
Allocates non-cacheable memory. */

void*
os_mem_alloc_nocache(
/*=================*/
			/* out: allocated memory */
	ulint	n)	/* in: number of bytes */
{
#ifdef __WIN__
	void*	ptr;

      	ptr = VirtualAlloc(NULL, n, MEM_COMMIT,
					PAGE_READWRITE | PAGE_NOCACHE);
	ut_a(ptr);

	return(ptr);
#else
	return(ut_malloc(n));
#endif
}

/********************************************************************
Sets the priority boost for threads released from waiting within the current
process. */

void
os_process_set_priority_boost(
/*==========================*/
	ibool	do_boost)	/* in: TRUE if priority boost should be done,
				FALSE if not */
{
#ifdef __WIN__
	ibool	no_boost;

	if (do_boost) {
		no_boost = FALSE;
	} else {
		no_boost = TRUE;
	}

	ut_a(TRUE == 1);

/* Does not do anything currently!
	SetProcessPriorityBoost(GetCurrentProcess(), no_boost);
*/
	fputs("Warning: process priority boost setting currently not functional!\n",
		stderr);
#else
	UT_NOT_USED(do_boost);
#endif
}
