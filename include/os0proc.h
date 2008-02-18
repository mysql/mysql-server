/******************************************************
The interface to the operating system
process control primitives

(c) 1995 Innobase Oy

Created 9/30/1995 Heikki Tuuri
*******************************************************/

#ifndef os0proc_h
#define os0proc_h

#include "univ.i"

#ifdef UNIV_LINUX
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

typedef void*			os_process_t;
typedef unsigned long int	os_process_id_t;

extern ibool os_use_large_pages;
/* Large page size. This may be a boot-time option on some platforms */
extern ulint os_large_page_size;

/********************************************************************
Converts the current process id to a number. It is not guaranteed that the
number is unique. In Linux returns the 'process number' of the current
thread. That number is the same as one sees in 'top', for example. In Linux
the thread id is not the same as one sees in 'top'. */
UNIV_INTERN
ulint
os_proc_get_number(void);
/*====================*/
/********************************************************************
Allocates non-cacheable memory. */
UNIV_INTERN
void*
os_mem_alloc_nocache(
/*=================*/
			/* out: allocated memory */
	ulint	n);	/* in: number of bytes */
/********************************************************************
Allocates large pages memory. */
UNIV_INTERN
void*
os_mem_alloc_large(
/*===============*/
					/* out: allocated memory */
	ulint*	n);			/* in/out: number of bytes */
/********************************************************************
Frees large pages memory. */
UNIV_INTERN
void
os_mem_free_large(
/*==============*/
	void	*ptr,			/* in: pointer returned by
					os_mem_alloc_large() */
	ulint	size);			/* in: size returned by
					os_mem_alloc_large() */
/********************************************************************
Sets the priority boost for threads released from waiting within the current
process. */
UNIV_INTERN
void
os_process_set_priority_boost(
/*==========================*/
	ibool	do_boost);	/* in: TRUE if priority boost should be done,
				FALSE if not */

#ifndef UNIV_NONINL
#include "os0proc.ic"
#endif

#endif
