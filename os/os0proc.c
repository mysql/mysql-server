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

#include "ut0mem.h"
#include "ut0byte.h"

ibool os_use_large_pages;
/* Large page size. This may be a boot-time option on some platforms */
ulint os_large_page_size;

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
Allocates large pages memory. */

void*
os_mem_alloc_large(
/*===============*/
					/* out: allocated memory */
	ulint		n,		/* in: number of bytes */
	ibool		set_to_zero,	/* in: TRUE if allocated memory
					should be set to zero if
					UNIV_SET_MEM_TO_ZERO is defined */
	ibool		assert_on_error)/* in: if TRUE, we crash mysqld if
					 the memory cannot be allocated */
{
#ifdef HAVE_LARGE_PAGES
	ulint size;
	int shmid;
	void *ptr = NULL;
	struct shmid_ds buf;

	if (!os_use_large_pages || !os_large_page_size) {
		goto skip;
	}

#ifdef UNIV_LINUX
	/* Align block size to os_large_page_size */
	size = ((n - 1) & ~(os_large_page_size - 1)) + os_large_page_size;

	shmid = shmget(IPC_PRIVATE, (size_t)size, SHM_HUGETLB | SHM_R | SHM_W);
	if (shmid < 0) {
		fprintf(stderr, "InnoDB: HugeTLB: Warning: Failed to allocate"
			" %lu bytes. errno %d\n", n, errno);
	} else {
		ptr = shmat(shmid, NULL, 0);
		if (ptr == (void *)-1) {
			fprintf(stderr, "InnoDB: HugeTLB: Warning: Failed to"
				" attach shared memory segment, errno %d\n",
				errno);
		}

		/* Remove the shared memory segment so that it will be
		automatically freed after memory is detached or
		process exits */
		shmctl(shmid, IPC_RMID, &buf);
	}
#endif

	if (ptr) {
		if (set_to_zero) {
#ifdef UNIV_SET_MEM_TO_ZERO
			memset(ptr, '\0', size);
#endif
		}

		return(ptr);
	}

	fprintf(stderr, "InnoDB HugeTLB: Warning: Using conventional"
		" memory pool\n");
skip:
#endif /* HAVE_LARGE_PAGES */

	return(ut_malloc_low(n, set_to_zero, assert_on_error));
}

/********************************************************************
Frees large pages memory. */

void
os_mem_free_large(
/*==============*/
	void	*ptr)	/* in: number of bytes */
{
#ifdef HAVE_LARGE_PAGES
	if (os_use_large_pages && os_large_page_size
#ifdef UNIV_LINUX
	    && !shmdt(ptr)
#endif
	    ) {
		return;
	}
#endif

	ut_free(ptr);
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

#if TRUE != 1
# error "TRUE != 1"
#endif

	/* Does not do anything currently!
	SetProcessPriorityBoost(GetCurrentProcess(), no_boost);
	*/
	fputs("Warning: process priority boost setting"
	      " currently not functional!\n",
	      stderr);
#else
	UT_NOT_USED(do_boost);
#endif
}
