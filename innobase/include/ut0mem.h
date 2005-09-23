/***********************************************************************
Memory primitives

(c) 1994, 1995 Innobase Oy

Created 5/30/1994 Heikki Tuuri
************************************************************************/

#ifndef ut0mem_h
#define ut0mem_h

#include "univ.i"
#include <string.h>
#include <stdlib.h>

/* The total amount of memory currently allocated from the OS with malloc */
extern ulint	ut_total_allocated_memory;

UNIV_INLINE
void*
ut_memcpy(void* dest, const void* sour, ulint n);

UNIV_INLINE
void*
ut_memmove(void* dest, const void* sour, ulint n);

UNIV_INLINE
int
ut_memcmp(const void* str1, const void* str2, ulint n);


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
	ibool	assert_on_error); /* in: if TRUE, we crash mysqld if the memory
				cannot be allocated */
/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined. */

void*
ut_malloc(
/*======*/
	                /* out, own: allocated memory */
        ulint   n);     /* in: number of bytes to allocate */
/**************************************************************************
Tests if malloc of n bytes would succeed. ut_malloc() asserts if memory runs
out. It cannot be used if we want to return an error message. Prints to
stderr a message if fails. */

ibool
ut_test_malloc(
/*===========*/
			/* out: TRUE if succeeded */
	ulint	n);	/* in: try to allocate this many bytes */
/**************************************************************************
Frees a memory bloock allocated with ut_malloc. */

void
ut_free(
/*====*/
	void* ptr);  /* in, own: memory block */
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
	ulint	size);	/* in: desired size */
/**************************************************************************
Frees in shutdown all allocated memory not freed yet. */

void
ut_free_all_mem(void);
/*=================*/

UNIV_INLINE
char*
ut_strcpy(char* dest, const char* sour);

UNIV_INLINE
ulint
ut_strlen(const char* str);

UNIV_INLINE
int
ut_strcmp(const void* str1, const void* str2);

/**************************************************************************
Copies up to size - 1 characters from the NUL-terminated string src to
dst, NUL-terminating the result. Returns strlen(src), so truncation
occurred if the return value >= size. */
ulint
ut_strlcpy(
/*=======*/
				/* out: strlen(src) */
	char*		dst,	/* in: destination buffer */
	const char*	src,	/* in: source buffer */
	ulint		size);	/* in: size of destination buffer */

/**************************************************************************
Compute strlen(ut_strcpyq(str, q)). */
UNIV_INLINE
ulint
ut_strlenq(
/*=======*/
				/* out: length of the string when quoted */
	const char*	str,	/* in: null-terminated string */
	char		q);	/* in: the quote character */

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
	const char*	src);	/* in: null-terminated string */

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
	ulint		len);	/* in: length of src */

#ifndef UNIV_NONINL
#include "ut0mem.ic"
#endif

#endif
