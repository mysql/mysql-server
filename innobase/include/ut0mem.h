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

UNIV_INLINE
void*
ut_memcpy(void* dest, void* sour, ulint n);

UNIV_INLINE
void*
ut_memmove(void* dest, void* sour, ulint n);

UNIV_INLINE
int
ut_memcmp(void* str1, void* str2, ulint n);


/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined and set_to_zero is TRUE. */

void*
ut_malloc_low(
/*==========*/
	                     /* out, own: allocated memory */
        ulint   n,           /* in: number of bytes to allocate */
	ibool   set_to_zero); /* in: TRUE if allocated memory should be set
			     to zero if UNIV_SET_MEM_TO_ZERO is defined */
/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined. */

void*
ut_malloc(
/*======*/
	                /* out, own: allocated memory */
        ulint   n);     /* in: number of bytes to allocate */
/**************************************************************************
Frees a memory bloock allocated with ut_malloc. */

void
ut_free(
/*====*/
	void* ptr);  /* in, own: memory block */
/**************************************************************************
Frees all allocated memory not freed yet. */

void
ut_free_all_mem(void);
/*=================*/

UNIV_INLINE
char*
ut_strcpy(char* dest, char* sour);

UNIV_INLINE
ulint
ut_strlen(char* str);

UNIV_INLINE
int
ut_strcmp(void* str1, void* str2);

/**************************************************************************
Catenates two strings into newly allocated memory. The memory must be freed
using mem_free. */

char*
ut_str_catenate(
/*============*/
			/* out, own: catenated null-terminated string */
	char*	str1,	/* in: null-terminated string */
	char*	str2);	/* in: null-terminated string */

#ifndef UNIV_NONINL
#include "ut0mem.ic"
#endif

#endif

