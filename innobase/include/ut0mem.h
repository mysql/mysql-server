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


void*
ut_malloc(ulint n);

UNIV_INLINE
void
ut_free(void* ptr);

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

