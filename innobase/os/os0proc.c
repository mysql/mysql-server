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


/*
How to get AWE to compile on Windows?
-------------------------------------

In the project settings of the innobase project the Visual C++ source,
__WIN2000__ has to be defined.

The Visual C++ has to be relatively recent and _WIN32_WINNT has to be
defined to a value >= 0x0500 when windows.h is included.

#define _WIN32_WINNT	0x0500

Where does AWE work?
-------------------

See the error message in os_awe_allocate_physical_mem().

How to assign privileges for mysqld to use AWE?
-----------------------------------------------

See the error message in os_awe_enable_lock_pages_in_mem().

Use Windows AWE functions in this order
---------------------------------------

(1) os_awe_enable_lock_pages_in_mem();
(2) os_awe_allocate_physical_mem();
(3) os_awe_allocate_virtual_mem_window();
(4) os_awe_map_physical_mem_to_window().

To test 'AWE' in a computer which does not have the AWE API,
you can compile with UNIV_SIMULATE_AWE defined in this file.
*/

#ifdef UNIV_SIMULATE_AWE
/* If we simulate AWE, we allocate the 'physical memory' here */
byte*		os_awe_simulate_mem;
ulint		os_awe_simulate_mem_size;
os_awe_t*	os_awe_simulate_page_info;
byte*		os_awe_simulate_window;
ulint		os_awe_simulate_window_size;
/* In simulated AWE the following contains a NULL pointer or a pointer
to a mapped 'physical page' for each 4 kB page in the AWE window */
byte**		os_awe_simulate_map;
#endif

#ifdef __WIN2000__
os_awe_t*	os_awe_page_info;
ulint		os_awe_n_pages;
byte*		os_awe_window;
ulint		os_awe_window_size;
#endif

ibool os_use_large_pages;
/* Large page size. This may be a boot-time option on some platforms */
ulint os_large_page_size;

/********************************************************************
Windows AWE support. Tries to enable the "lock pages in memory" privilege for
the current process so that the current process can allocate memory-locked
virtual address space to act as the window where AWE maps physical memory. */

ibool
os_awe_enable_lock_pages_in_mem(void)
/*=================================*/
				/* out: TRUE if success, FALSE if error;
				prints error info to stderr if no success */
{
#ifdef UNIV_SIMULATE_AWE

	return(TRUE);

#elif defined(__WIN2000__)
  	struct {
    	DWORD 			Count;
    	LUID_AND_ATTRIBUTES 	Privilege[1];
  	} 	Info;
	HANDLE	hProcess;
  	HANDLE	Token;
  	BOOL 	Result;

	hProcess = GetCurrentProcess();

  	/* Open the token of the current process */

  	Result = OpenProcessToken(hProcess,
                              TOKEN_ADJUST_PRIVILEGES,
                              &Token);
  	if (Result != TRUE) {
    		fprintf(stderr,
			"InnoDB: AWE: Cannot open process token, error %lu\n",
			(ulint)GetLastError());
    		return(FALSE);
  	}

  	Info.Count = 1;

    	Info.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;

  	/* Get the local unique identifier (LUID) of the SE_LOCK_MEMORY
	privilege */

  	Result = LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME,
                                  &(Info.Privilege[0].Luid));
  	if (Result != TRUE)  {
    		fprintf(stderr,
	"InnoDB: AWE: Cannot get local privilege value for %s, error %lu.\n",
			SE_LOCK_MEMORY_NAME, (ulint)GetLastError());

    		return(FALSE);
  	}

  	/* Try to adjust the privilege */

  	Result = AdjustTokenPrivileges(Token, FALSE,
                                   (PTOKEN_PRIVILEGES)&Info,
                                   0, NULL, NULL);
  	/* Check the result */

  	if (Result != TRUE)  {
    		fprintf(stderr,
		"InnoDB: AWE: Cannot adjust process token privileges, error %u.\n",
			GetLastError());
    		return(FALSE);
  	} else if (GetLastError() != ERROR_SUCCESS) {
      		fprintf(stderr,
"InnoDB: AWE: Cannot enable SE_LOCK_MEMORY privilege, error %lu.\n"
"InnoDB: In Windows XP Home you cannot use AWE. In Windows 2000 and XP\n"
"InnoDB: Professional you must go to the Control Panel, to\n"
"InnoDB: Security Settings, to Local Policies, and enable\n"
"InnoDB: the 'lock pages in memory' privilege for the user who runs\n"
"InnoDB: the MySQL server.\n", GetLastError());

		return(FALSE);
	}

	CloseHandle(Token);

	return(TRUE);
#else
#ifdef __WIN__
	fprintf(stderr,
"InnoDB: AWE: Error: to use AWE you must use a ...-nt MySQL executable.\n");
#endif	
	return(FALSE);
#endif
}

/********************************************************************
Allocates physical RAM memory up to 64 GB in an Intel 32-bit x86
processor. */

ibool
os_awe_allocate_physical_mem(
/*=========================*/
				/* out: TRUE if success */
	os_awe_t** page_info,	/* out, own: array of opaque data containing
				the info for allocated physical memory pages;
				each allocated 4 kB physical memory page has
				one slot of type os_awe_t in the array */
	ulint	  n_megabytes)	/* in: number of megabytes to allocate */
{
#ifdef UNIV_SIMULATE_AWE
	os_awe_simulate_page_info = ut_malloc(sizeof(os_awe_t) *
		n_megabytes * ((1024 * 1024) / OS_AWE_X86_PAGE_SIZE));

	os_awe_simulate_mem = ut_align(ut_malloc(
					4096 + 1024 * 1024 * n_megabytes),
					4096);
	os_awe_simulate_mem_size = n_megabytes * 1024 * 1024;

	*page_info = os_awe_simulate_page_info;

	return(TRUE);

#elif defined(__WIN2000__)
	BOOL		bResult;
  	os_awe_t 	NumberOfPages;		/* Question: why does Windows
  						use the name ULONG_PTR for
  						a scalar integer type? Maybe
  						because we may also refer to
  						&NumberOfPages? */
  	os_awe_t 	NumberOfPagesInitial;
  	SYSTEM_INFO 	sSysInfo;
  	int 		PFNArraySize;

	if (n_megabytes > 64 * 1024) {

		fprintf(stderr,
"InnoDB: AWE: Error: tried to allocate %lu MB.\n"
"InnoDB: AWE cannot allocate more than 64 GB in any computer.\n", n_megabytes);

		return(FALSE);
	}

  	GetSystemInfo(&sSysInfo);  /* fill the system information structure */

  	if ((ulint)OS_AWE_X86_PAGE_SIZE != (ulint)sSysInfo.dwPageSize) {
		fprintf(stderr,
"InnoDB: AWE: Error: this computer has a page size of %lu.\n"
"InnoDB: Should be 4096 bytes for InnoDB AWE support to work.\n",
			(ulint)sSysInfo.dwPageSize);

		return(FALSE);
	}

  	/* Calculate the number of pages of memory to request */

  	NumberOfPages = n_megabytes * ((1024 * 1024) / OS_AWE_X86_PAGE_SIZE);
 
 	/* Calculate the size of page_info for allocated physical pages */

  	PFNArraySize = NumberOfPages * sizeof(os_awe_t);

   	*page_info = (os_awe_t*)HeapAlloc(GetProcessHeap(), 0, PFNArraySize);

	if (*page_info == NULL) {
    		fprintf(stderr,
"InnoDB: AWE: Failed to allocate page info array from process heap, error %lu\n",
			(ulint)GetLastError());

    		return(FALSE);
  	}

	ut_total_allocated_memory += PFNArraySize;

  	/* Enable this process' privilege to lock pages to physical memory */

	if (!os_awe_enable_lock_pages_in_mem()) {

		return(FALSE);
	}

  	/* Allocate the physical memory */

  	NumberOfPagesInitial = NumberOfPages;

	os_awe_page_info = *page_info;
	os_awe_n_pages = (ulint)NumberOfPages;

	/* Compilation note: if the compiler complains the function is not
	defined, see the note at the start of this file */

 	bResult = AllocateUserPhysicalPages(GetCurrentProcess(),
                                       &NumberOfPages,
                                       *page_info);
  	if (bResult != TRUE) {
    		fprintf(stderr,
"InnoDB: AWE: Cannot allocate physical pages, error %lu.\n",
			(ulint)GetLastError());

    		return(FALSE);
  	}

  	if (NumberOfPagesInitial != NumberOfPages) {
    		fprintf(stderr,
"InnoDB: AWE: Error: allocated only %lu pages of %lu requested.\n"
"InnoDB: Check that you have enough free RAM.\n"
"InnoDB: In Windows XP Professional and 2000 Professional\n"
"InnoDB: Windows PAE size is max 4 GB. In 2000 and .NET\n"
"InnoDB: Advanced Servers and 2000 Datacenter Server it is 32 GB,\n"
"InnoDB: and in .NET Datacenter Server it is 64 GB.\n"
"InnoDB: A Microsoft web page said that the processor must be an Intel\n"
"InnoDB: processor.\n",
			(ulint)NumberOfPages,
			(ulint)NumberOfPagesInitial);

    		return(FALSE);
  	}

	fprintf(stderr,
"InnoDB: Using Address Windowing Extensions (AWE); allocated %lu MB\n",
		n_megabytes);

	return(TRUE);	
#else
	UT_NOT_USED(n_megabytes);
	UT_NOT_USED(page_info);
	
	return(FALSE);
#endif
}

/********************************************************************
Allocates a window in the virtual address space where we can map then
pages of physical memory. */

byte*
os_awe_allocate_virtual_mem_window(
/*===============================*/
			/* out, own: allocated memory, or NULL if did not
			succeed */
	ulint	size)	/* in: virtual memory allocation size in bytes, must
			be < 2 GB */
{
#ifdef UNIV_SIMULATE_AWE
	ulint	i;

	os_awe_simulate_window = ut_align(ut_malloc(4096 + size), 4096);
	os_awe_simulate_window_size = size;

	os_awe_simulate_map = ut_malloc(sizeof(byte*) * (size / 4096));

	for (i = 0; i < (size / 4096); i++) {
		*(os_awe_simulate_map + i) = NULL;
	}

	return(os_awe_simulate_window);
	
#elif defined(__WIN2000__)
	byte*	ptr;

	if (size > (ulint)0x7FFFFFFFUL) {
		fprintf(stderr,
"InnoDB: AWE: Cannot allocate %lu bytes of virtual memory\n", size);

		return(NULL);
	}
	
	ptr = VirtualAlloc(NULL, (SIZE_T)size, MEM_RESERVE | MEM_PHYSICAL,
							PAGE_READWRITE);
	if (ptr == NULL) {
		fprintf(stderr,
"InnoDB: AWE: Cannot allocate %lu bytes of virtual memory, error %lu\n",
		size, (ulint)GetLastError());

		return(NULL);
	}

	os_awe_window = ptr;
	os_awe_window_size = size;

	ut_total_allocated_memory += size;

	return(ptr);
#else
	UT_NOT_USED(size);
	
	return(NULL);
#endif
}

/********************************************************************
With this function you can map parts of physical memory allocated with
the ..._allocate_physical_mem to the virtual address space allocated with
the previous function. Intel implements this so that the process page
tables are updated accordingly. A test on a 1.5 GHz AMD processor and XP
showed that this takes < 1 microsecond, much better than the estimated 80 us
for copying a 16 kB page memory to memory. But, the operation will at least
partially invalidate the translation lookaside buffer (TLB) of all
processors. Under a real-world load the performance hit may be bigger. */

ibool
os_awe_map_physical_mem_to_window(
/*==============================*/
					/* out: TRUE if success; the function
					calls exit(1) in case of an error */
	byte*		ptr,		/* in: a page-aligned pointer to
					somewhere in the virtual address
					space window; we map the physical mem
					pages here */
	ulint		n_mem_pages,	/* in: number of 4 kB mem pages to
					map */
	os_awe_t*	page_info)	/* in: array of page infos for those
					pages; each page has one slot in the
					array */
{
#ifdef UNIV_SIMULATE_AWE
	ulint	i;
	byte**	map;
	byte*	page;
	byte*	phys_page;

	ut_a(ptr >= os_awe_simulate_window);
	ut_a(ptr < os_awe_simulate_window + os_awe_simulate_window_size);
	ut_a(page_info >= os_awe_simulate_page_info);
	ut_a(page_info < os_awe_simulate_page_info +
			 		(os_awe_simulate_mem_size / 4096));

	/* First look if some other 'physical pages' are mapped at ptr,
	and copy them back to where they were if yes */

	map = os_awe_simulate_map
			+ ((ulint)(ptr - os_awe_simulate_window)) / 4096;
	page = ptr;
		
	for (i = 0; i < n_mem_pages; i++) {
		if (*map != NULL) {
			ut_memcpy(*map, page, 4096);
		}
		map++;
		page += 4096;
	}

	/* Then copy to ptr the 'physical pages' determined by page_info; we
	assume page_info is a segment of the array we created at the start */

	phys_page = os_awe_simulate_mem
			+ (ulint)(page_info - os_awe_simulate_page_info)
			  * 4096;

	ut_memcpy(ptr, phys_page, n_mem_pages * 4096);

	/* Update the map */

	map = os_awe_simulate_map
			+ ((ulint)(ptr - os_awe_simulate_window)) / 4096;

	for (i = 0; i < n_mem_pages; i++) {
		*map = phys_page;

		map++;
		phys_page += 4096;
	}

	return(TRUE);
	
#elif defined(__WIN2000__)
	BOOL		bResult;
	os_awe_t	n_pages;

	n_pages = (os_awe_t)n_mem_pages;
	
	if (!(ptr >= os_awe_window)) {
		fprintf(stderr,
"InnoDB: AWE: Error: trying to map to address %lx but AWE window start %lx\n",
		(ulint)ptr, (ulint)os_awe_window);
		ut_a(0);
	}

	if (!(ptr <= os_awe_window + os_awe_window_size - UNIV_PAGE_SIZE)) {
		fprintf(stderr,
"InnoDB: AWE: Error: trying to map to address %lx but AWE window end %lx\n",
		(ulint)ptr, (ulint)os_awe_window + os_awe_window_size);
		ut_a(0);
	}

	if (!(page_info >= os_awe_page_info)) {
		fprintf(stderr,
"InnoDB: AWE: Error: trying to map page info at %lx but array start %lx\n",
		(ulint)page_info, (ulint)os_awe_page_info);
		ut_a(0);
	}

	if (!(page_info <= os_awe_page_info + (os_awe_n_pages - 4))) {
		fprintf(stderr,
"InnoDB: AWE: Error: trying to map page info at %lx but array end %lx\n",
		(ulint)page_info, (ulint)(os_awe_page_info + os_awe_n_pages));
		ut_a(0);
	}

	bResult = MapUserPhysicalPages((PVOID)ptr, n_pages, page_info);

	if (bResult != TRUE) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: AWE: Mapping of %lu physical pages to address %lx failed,\n"
"InnoDB: error %lu.\n"
"InnoDB: Cannot continue operation.\n",
			n_mem_pages, (ulint)ptr, (ulint)GetLastError());
		exit(1);
	}

	return(TRUE);
#else
	UT_NOT_USED(ptr);
	UT_NOT_USED(n_mem_pages);
	UT_NOT_USED(page_info);

	return(FALSE);
#endif
}	

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
/*=================*/
      /* out: allocated memory */
  ulint	n, /* in: number of bytes */
	ibool set_to_zero, /* in: TRUE if allocated memory should be set
        to zero if UNIV_SET_MEM_TO_ZERO is defined */
	ibool	assert_on_error) /* in: if TRUE, we crash mysqld if the memory
				cannot be allocated */
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
    fprintf(stderr, "InnoDB: HugeTLB: Warning: Failed to allocate %lu bytes. "
            "errno %d\n", n, errno);
  } else {
    ptr = shmat(shmid, NULL, 0);
    if (ptr == (void *)-1) {
      fprintf(stderr, "InnoDB: HugeTLB: Warning: Failed to attach shared memory "
              "segment, errno %d\n", errno);
    }
    /*
      Remove the shared memory segment so that it will be automatically freed
      after memory is detached or process exits
    */
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

  fprintf(stderr, "InnoDB HugeTLB: Warning: Using conventional memory pool\n");
skip:
#endif /* HAVE_LARGE_PAGES */
  
	return(ut_malloc_low(n, set_to_zero, assert_on_error));
}

/********************************************************************
Frees large pages memory. */

void
os_mem_free_large(
/*=================*/
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
