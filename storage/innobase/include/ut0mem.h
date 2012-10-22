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
Allocates memory. */

void*
ut_malloc_low(
/*==========*/
					/* out, own: allocated memory */
	ulint	n,			/* in: number of bytes to allocate */
	ibool	assert_on_error);	/* in: if TRUE, we crash mysqld if
					the memory cannot be allocated */
/**************************************************************************
Allocates memory. */
#define ut_malloc(n) ut_malloc_low(n, TRUE)
/**************************************************************************
Frees a memory block allocated with ut_malloc. */

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
       ory will be uninitialized.  If ptr is NULL,  the	 call  is
       equivalent  to malloc(size); if size is equal to zero, the
       call is equivalent to free(ptr).	 Unless ptr is	NULL,  it
       must  have  been	 returned by an earlier call to malloc(),
       calloc() or realloc().

RETURN VALUE
       realloc() returns a pointer to the newly allocated memory,
       which is suitably aligned for any kind of variable and may
       be different from ptr, or NULL if the  request  fails.  If
       size  was equal to 0, either NULL or a pointer suitable to
       be passed to free() is returned.	 If realloc()  fails  the
       original	 block	is  left  untouched  - it is not freed or
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
Like ut_strlcpy, but if src doesn't fit in dst completely, copies the last
(size - 1) bytes of src, not the first. */

ulint
ut_strlcpy_rev(
/*===========*/
				/* out: strlen(src) */
	char*		dst,	/* in: destination buffer */
	const char*	src,	/* in: source buffer */
	ulint		size);	/* in: size of destination buffer */

/**************************************************************************
Return the number of times s2 occurs in s1. Overlapping instances of s2
are only counted once. */

ulint
ut_strcount(
/*========*/
				/* out: the number of times s2 occurs in s1 */
	const char*	s1,	/* in: string to search in */
	const char*	s2);	/* in: string to search for */

/**************************************************************************
Replace every occurrence of s1 in str with s2. Overlapping instances of s1
are only replaced once. */

char *
ut_strreplace(
/*==========*/
				/* out, own: modified string, must be
				freed with mem_free() */
	const char*	str,	/* in: string to operate on */
	const char*	s1,	/* in: string to replace */
	const char*	s2);	/* in: string to replace s1 with */

#ifndef UNIV_NONINL
#include "ut0mem.ic"
#endif

#endif
