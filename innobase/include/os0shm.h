/******************************************************
The interface to the operating system
shared memory primitives

(c) 1995 Innobase Oy

Created 9/23/1995 Heikki Tuuri
*******************************************************/

#ifndef os0shm_h
#define os0shm_h

#include "univ.i"

typedef void*			os_shm_t;


/********************************************************************
Creates an area of shared memory. It can be named so that
different processes may access it in the same computer.
If an area with the same name already exists, returns
a handle to that area (where the size of the area is
not changed even if this call requests a different size).
To use the area, it first has to be mapped to the process
address space by os_shm_map. */

os_shm_t
os_shm_create(
/*==========*/
			/* out, own: handle to the shared
			memory area, NULL if error */
	ulint	size,	/* in: area size < 4 GB */
	char*	name);	/* in: name of the area as a null-terminated
			string */
/***************************************************************************
Frees a shared memory area. The area can be freed only after it
has been unmapped in all the processes where it was mapped. */

ibool
os_shm_free(
/*========*/
				/* out: TRUE if success */
	os_shm_t	shm);	/* in, own: handle to a shared memory area */
/***************************************************************************
Maps a shared memory area in the address space of a process. */

void*
os_shm_map(
/*=======*/
				/* out: address of the area, NULL if error */
	os_shm_t	shm);	/* in: handle to a shared memory area */
/***************************************************************************
Unmaps a shared memory area from the address space of a process. */

ibool
os_shm_unmap(
/*=========*/
			/* out: TRUE if succeed */
	void*	addr);	/* in: address of the area */


#ifndef UNIV_NONINL
#include "os0shm.ic"
#endif

#endif 
