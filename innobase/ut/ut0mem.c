/************************************************************************
Memory primitives

(c) 1994, 1995 Innobase Oy

Created 5/11/1994 Heikki Tuuri
*************************************************************************/

#include "ut0mem.h"

#ifdef UNIV_NONINL
#include "ut0mem.ic"
#endif

#include "mem0mem.h"
#include "os0sync.h"

/* This struct is placed first in every allocated memory block */
typedef struct ut_mem_block_struct ut_mem_block_t;

/* The total amount of memory currently allocated from the OS with malloc */
ulint	ut_total_allocated_memory	= 0;

struct ut_mem_block_struct{
        UT_LIST_NODE_T(ut_mem_block_t) mem_block_list;
			/* mem block list node */
	ulint	size;	/* size of allocated memory */
	ulint	magic_n;
};

#define UT_MEM_MAGIC_N	1601650166

/* List of all memory blocks allocated from the operating system
with malloc */
UT_LIST_BASE_NODE_T(ut_mem_block_t)   ut_mem_block_list;

os_fast_mutex_t ut_list_mutex;  /* this protects the list */

ibool  ut_mem_block_list_inited = FALSE;

ulint*	ut_mem_null_ptr	= NULL;

/**************************************************************************
Initializes the mem block list at database startup. */
static
void
ut_mem_block_list_init(void)
/*========================*/
{
        os_fast_mutex_init(&ut_list_mutex);
        UT_LIST_INIT(ut_mem_block_list);
	ut_mem_block_list_inited = TRUE;
}

/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined and set_to_zero is TRUE. */

void*
ut_malloc_low(
/*==========*/
	                     /* out, own: allocated memory */
        ulint   n,           /* in: number of bytes to allocate */
	ibool   set_to_zero, /* in: TRUE if allocated memory should be set
			     to zero if UNIV_SET_MEM_TO_ZERO is defined */
	ibool	assert_on_error) /* in: if TRUE, we crash mysqld if the memory
				cannot be allocated */
{
	void*	ret;

	ut_ad((sizeof(ut_mem_block_t) % 8) == 0); /* check alignment ok */

	if (!ut_mem_block_list_inited) {
	        ut_mem_block_list_init();
	}

	os_fast_mutex_lock(&ut_list_mutex);

	ret = malloc(n + sizeof(ut_mem_block_t));

	if (ret == NULL) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
		"  InnoDB: Fatal error: cannot allocate %lu bytes of\n"
		"InnoDB: memory with malloc! Total allocated memory\n"
		"InnoDB: by InnoDB %lu bytes. Operating system errno: %lu\n"
		"InnoDB: Cannot continue operation!\n"
		"InnoDB: Check if you should increase the swap file or\n"
		"InnoDB: ulimits of your operating system.\n"
		"InnoDB: On FreeBSD check you have compiled the OS with\n"
		"InnoDB: a big enough maximum process size.\n",
		                  (ulong) n, (ulong) ut_total_allocated_memory,
#ifdef __WIN__
			(ulong) GetLastError()
#else
			(ulong) errno
#endif
			);

		/* Flush stderr to make more probable that the error
		message gets in the error file before we generate a seg
		fault */

		fflush(stderr);

	        os_fast_mutex_unlock(&ut_list_mutex);

		/* Make an intentional seg fault so that we get a stack
		trace */
		/* Intentional segfault on NetWare causes an abend. Avoid this 
		by graceful exit handling in ut_a(). */
#if (!defined __NETWARE__) 
		if (assert_on_error) {
			fprintf(stderr,
		"InnoDB: We now intentionally generate a seg fault so that\n"
		"InnoDB: on Linux we get a stack trace.\n");

			if (*ut_mem_null_ptr) ut_mem_null_ptr = 0;
		} else {
			return(NULL);
		}
#else
		ut_a(0);
#endif
	}		

	if (set_to_zero) {
#ifdef UNIV_SET_MEM_TO_ZERO
	        memset(ret, '\0', n + sizeof(ut_mem_block_t));
#endif
	}

	((ut_mem_block_t*)ret)->size = n + sizeof(ut_mem_block_t);
	((ut_mem_block_t*)ret)->magic_n = UT_MEM_MAGIC_N;

	ut_total_allocated_memory += n + sizeof(ut_mem_block_t);
	
	UT_LIST_ADD_FIRST(mem_block_list, ut_mem_block_list,
			                         ((ut_mem_block_t*)ret));
	os_fast_mutex_unlock(&ut_list_mutex);

	return((void*)((byte*)ret + sizeof(ut_mem_block_t)));
}

/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined. */

void*
ut_malloc(
/*======*/
	                /* out, own: allocated memory */
        ulint   n)      /* in: number of bytes to allocate */
{
        return(ut_malloc_low(n, TRUE, TRUE));
}

/**************************************************************************
Tests if malloc of n bytes would succeed. ut_malloc() asserts if memory runs
out. It cannot be used if we want to return an error message. Prints to
stderr a message if fails. */

ibool
ut_test_malloc(
/*===========*/
			/* out: TRUE if succeeded */
	ulint	n)	/* in: try to allocate this many bytes */
{
	void*	ret;

	ret = malloc(n);

	if (ret == NULL) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
		"  InnoDB: Error: cannot allocate %lu bytes of memory for\n"
		"InnoDB: a BLOB with malloc! Total allocated memory\n"
		"InnoDB: by InnoDB %lu bytes. Operating system errno: %d\n"
		"InnoDB: Check if you should increase the swap file or\n"
		"InnoDB: ulimits of your operating system.\n"
		"InnoDB: On FreeBSD check you have compiled the OS with\n"
		"InnoDB: a big enough maximum process size.\n",
		                  (ulong) n,
			          (ulong) ut_total_allocated_memory,
				  (int) errno);
		return(FALSE);
	}

	free(ret);

	return(TRUE);
}	

/**************************************************************************
Frees a memory block allocated with ut_malloc. */

void
ut_free(
/*====*/
	void* ptr)  /* in, own: memory block */
{
        ut_mem_block_t* block;

	block = (ut_mem_block_t*)((byte*)ptr - sizeof(ut_mem_block_t));

	os_fast_mutex_lock(&ut_list_mutex);

	ut_a(block->magic_n == UT_MEM_MAGIC_N);
	ut_a(ut_total_allocated_memory >= block->size);

	ut_total_allocated_memory -= block->size;
	
	UT_LIST_REMOVE(mem_block_list, ut_mem_block_list, block);
	free(block);
	
	os_fast_mutex_unlock(&ut_list_mutex);
}

/**************************************************************************
Implements realloc. This is needed by /pars/lexyy.c. Otherwise, you should not
use this function because the allocation functions in mem0mem.h are the
recommended ones in InnoDB.

man realloc in Linux, 2004:

       realloc()  changes the size of the memory block pointed to
       by ptr to size bytes.  The contents will be  unchanged  to
       the minimum of the old and new sizes; newly allocated mem­
       ory will be uninitialized.  If ptr is NULL,  the  call  is
       equivalent  to malloc(size); if size is equal to zero, the
       call is equivalent to free(ptr).  Unless ptr is  NULL,  it
       must  have  been  returned by an earlier call to malloc(),
       calloc() or realloc().

RETURN VALUE
       realloc() returns a pointer to the newly allocated memory,
       which is suitably aligned for any kind of variable and may
       be different from ptr, or NULL if the  request  fails.  If
       size  was equal to 0, either NULL or a pointer suitable to
       be passed to free() is returned.  If realloc()  fails  the
       original  block  is  left  untouched  - it is not freed or
       moved. */

void*
ut_realloc(
/*=======*/
			/* out, own: pointer to new mem block or NULL */
	void*	ptr,	/* in: pointer to old block or NULL */
	ulint	size)	/* in: desired size */
{
        ut_mem_block_t* block;
	ulint		old_size;
	ulint		min_size;
	void*		new_ptr;

	if (ptr == NULL) {

		return(ut_malloc(size));
	}

	if (size == 0) {
		ut_free(ptr);

		return(NULL);
	}

	block = (ut_mem_block_t*)((byte*)ptr - sizeof(ut_mem_block_t));

	ut_a(block->magic_n == UT_MEM_MAGIC_N);

	old_size = block->size - sizeof(ut_mem_block_t);

	if (size < old_size) {
		min_size = size;
	} else {
		min_size = old_size;
	}
		
	new_ptr = ut_malloc(size);

	if (new_ptr == NULL) {

		return(NULL);
	}				

	/* Copy the old data from ptr */
	ut_memcpy(new_ptr, ptr, min_size);

	ut_free(ptr);

	return(new_ptr);		
}

/**************************************************************************
Frees in shutdown all allocated memory not freed yet. */

void
ut_free_all_mem(void)
/*=================*/
{
        ut_mem_block_t* block;

        os_fast_mutex_free(&ut_list_mutex);

	while ((block = UT_LIST_GET_FIRST(ut_mem_block_list))) {

		ut_a(block->magic_n == UT_MEM_MAGIC_N);
		ut_a(ut_total_allocated_memory >= block->size);

		ut_total_allocated_memory -= block->size;
	
	        UT_LIST_REMOVE(mem_block_list, ut_mem_block_list, block);
	        free(block);
	}
		                      
	if (ut_total_allocated_memory != 0) {
		fprintf(stderr,
"InnoDB: Warning: after shutdown total allocated memory is %lu\n",
		  (ulong) ut_total_allocated_memory);
	}
}

/**************************************************************************
Make a quoted copy of a NUL-terminated string.  Leading and trailing
quotes will not be included; only embedded quotes will be escaped.
See also ut_strlenq() and ut_memcpyq(). */

char*
ut_strcpyq(
/*=======*/
				/* out: pointer to end of dest */
	char*		dest,	/* in: output buffer */
	char		q,	/* in: the quote character */
	const char*	src)	/* in: null-terminated string */
{
	while (*src) {
		if ((*dest++ = *src++) == q) {
			*dest++ = q;
		}
	}

	return(dest);
}

/**************************************************************************
Make a quoted copy of a fixed-length string.  Leading and trailing
quotes will not be included; only embedded quotes will be escaped.
See also ut_strlenq() and ut_strcpyq(). */

char*
ut_memcpyq(
/*=======*/
				/* out: pointer to end of dest */
	char*		dest,	/* in: output buffer */
	char		q,	/* in: the quote character */
	const char*	src,	/* in: string to be quoted */
	ulint		len)	/* in: length of src */
{
	const char*	srcend = src + len;

	while (src < srcend) {
		if ((*dest++ = *src++) == q) {
			*dest++ = q;
		}
	}

	return(dest);
}
