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

void*
ut_malloc(ulint n)
{
	void*	ret;
	/*
	ret = VirtualAlloc(NULL, n, MEM_COMMIT, PAGE_READWRITE);
	*/

	ret = malloc(n);

	if (ret == NULL) {
		fprintf(stderr,
		"Innobase: Fatal error: cannot allocate memory!\n");
		fprintf(stderr,
		"Innobase: Cannot continue operation!\n");
		fprintf(stderr,
		"Innobase: Check if you can increase the swap file of your\n");
		fprintf(stderr,
		"Innobase: operating system.\n");

		exit(1);
	}				

	return(ret);
}

/**************************************************************************
Catenates two strings into newly allocated memory. The memory must be freed
using mem_free. */

char*
ut_str_catenate(
/*============*/
			/* out, own: catenated null-terminated string */
	char*	str1,	/* in: null-terminated string */
	char*	str2)	/* in: null-terminated string */
{
	ulint	len1;
	ulint	len2;
	char*	str;

	len1 = ut_strlen(str1);
	len2 = ut_strlen(str2);

	str = mem_alloc(len1 + len2 + 1);

	ut_memcpy(str, str1, len1);
	ut_memcpy(str + len1, str2, len2);

	str[len1 + len2] = '\0';

	return(str);
}
