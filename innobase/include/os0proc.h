/******************************************************
The interface to the operating system
process control primitives

(c) 1995 Innobase Oy

Created 9/30/1995 Heikki Tuuri
*******************************************************/

#ifndef os0proc_h
#define os0proc_h

#include "univ.i"

typedef void*			os_process_t;
typedef unsigned long int	os_process_id_t;

/********************************************************************
Allocates non-cacheable memory. */

void*
os_mem_alloc_nocache(
/*=================*/
			/* out: allocated memory */
	ulint	n);	/* in: number of bytes */
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
	os_process_id_t* id);	/* out: process id */
/**************************************************************************
Exits a process. */

void
os_process_exit(
/*============*/
	ulint	code);	/* in: exit code */
/**************************************************************************
Gets process exit code. */

ibool
os_process_get_exit_code(
/*=====================*/
				/* out: TRUE if succeed, FALSE if fail */
	os_process_t	proc,	/* in: handle to the process */
	ulint*		code);	/* out: exit code */
#endif
/********************************************************************
Sets the priority boost for threads released from waiting within the current
process. */

void
os_process_set_priority_boost(
/*==========================*/
	ibool	do_boost);	/* in: TRUE if priority boost should be done,
				FALSE if not */

#ifndef UNIV_NONINL
#include "os0proc.ic"
#endif

#endif 
