/******************************************************
The interface to the operating system
shared memory primitives

(c) 1995 Innobase Oy

Created 9/23/1995 Heikki Tuuri
*******************************************************/

#include "os0shm.h"
#ifdef UNIV_NONINL
#include "os0shm.ic"
#endif

#ifdef __WIN__
#include "windows.h"

typedef	HANDLE	os_shm_t;
#endif

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
	char*	name)	/* in: name of the area as a null-terminated
			string */
{
#ifdef __WIN__
	os_shm_t	shm;

	ut_a(name);
	ut_a(size > 0);
	ut_a(size < 0xFFFFFFFF);

	/* In Windows NT shared memory is created as a memory mapped
	file */
	shm = CreateFileMapping((HANDLE)0xFFFFFFFF, /* use operating system
						    swap file as the backing
						    file */
				NULL,		    /* default security
						    descriptor */
				PAGE_READWRITE,	    /* allow reading and
						    writing */
				0,		    /* size must be less
						    than 4 GB */
				(DWORD)size,	
				name);			
	return(shm);
#else
	UT_NOT_USED(size);
	UT_NOT_USED(name);

	return(NULL);
#endif
}

/***************************************************************************
Frees a shared memory area. The area can be freed only after it
has been unmapped in all the processes where it was mapped. */

ibool
os_shm_free(
/*========*/
				/* out: TRUE if success */
	os_shm_t	shm)	/* in, own: handle to a shared memory area */
{
#ifdef __WIN__

	BOOL	ret;

	ut_a(shm);

	ret = CloseHandle(shm);

	if (ret) {
		return(TRUE);
	} else {
		return(FALSE);
	}
#else
	UT_NOT_USED(shm);
#endif
}

/***************************************************************************
Maps a shared memory area in the address space of a process. */

void*
os_shm_map(
/*=======*/
				/* out: address of the area, NULL if error */
	os_shm_t	shm)	/* in: handle to a shared memory area */
{
#ifdef __WIN__
	void*	mem;

	ut_a(shm);

	mem = MapViewOfFile(shm,
			    FILE_MAP_ALL_ACCESS,  /* read and write access
			    			  allowed */
			    0,			  /* map from start of */
			    0,			  /* area */
			    0);			  /* map the whole area */
	return(mem);
#else
	UT_NOT_USED(shm);
#endif
}
		
/***************************************************************************
Unmaps a shared memory area from the address space of a process. */

ibool
os_shm_unmap(
/*=========*/
			/* out: TRUE if succeed */
	void*	addr)	/* in: address of the area */
{
#ifdef __WIN__
	BOOL	ret;

	ut_a(addr);

	ret = UnmapViewOfFile(addr);

	if (ret) {
		return(TRUE);
	} else {
		return(FALSE);
	}
#else
	UT_NOT_USED(addr);
#endif
}
