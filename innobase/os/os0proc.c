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

#ifdef notdefined
/********************************************************************
Creates a new process. */

ibool
os_process_create(
/*==============*/
	char*		name,	/* in: name of the executable to start
				or its full path name */
	char*		cmd,	/* in: command line for the starting
				process, or NULL if no command line
				specified */
	os_process_t*	proc,	/* out: handle to the process */
	os_process_id_t* id)	/* out: process id */

{
	BOOL			ret;
	PROCESS_INFORMATION	pinfo;
	STARTUPINFO		sinfo;

	/* The following assignments are default for the startupinfo
	structure */
	sinfo.cb		= sizeof(STARTUPINFO);
	sinfo.lpReserved	= NULL;
	sinfo.lpDesktop		= NULL;
	sinfo.cbReserved2	= 0;
	sinfo.lpReserved	= NULL;
	
	ret = CreateProcess(name,
			      cmd,
			      NULL,	/* No security attributes */
			      NULL,	/* No thread security attrs */
			      FALSE,	/* Do not inherit handles */
			      0,	/* No creation flags */
			      NULL,	/* No environment */
			      NULL,	/* Same current directory */
			      &sinfo,
			      &pinfo);

	*proc = pinfo.hProcess;
	*id   = pinfo.dwProcessId;

	return(ret);
}

/**************************************************************************
Exits a process. */

void
os_process_exit(
/*============*/
	ulint	code)	/* in: exit code */
{
	ExitProcess((UINT)code);
}

/**************************************************************************
Gets a process exit code. */

ibool
os_process_get_exit_code(
/*=====================*/
				/* out: TRUE if succeed, FALSE if fail */
	os_process_t	proc,	/* in: handle to the process */
	ulint*		code)	/* out: exit code */
{
	DWORD		ex_code;
	BOOL		ret;

	ret = GetExitCodeProcess(proc, &ex_code);

	*code = (ulint)ex_code;
	
	return(ret);
}
#endif /* notdedfined */

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
	printf(
        "Warning: process priority boost setting currently not functional!\n"
	);
#else
	UT_NOT_USED(do_boost);
#endif
}
