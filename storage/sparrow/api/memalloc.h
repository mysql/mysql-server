/* This is the include file that should be included 'first' in every C file. */
#ifndef _spw_api_memalloc_h
#define _spw_api_memalloc_h

// Reuse type and constant definitions from MySQL
//#include <my_global.h>
#include "my_sys.h"
//#include <string.h>
//#include "m_string.h"

//#define bzero(A,B)             memset((A),0,(B))

namespace Sparrow
{
	void my_free(void *ptr);
	void *my_malloc(size_t size, myf my_flags);
	void *my_realloc(void *oldpoint, size_t size, myf my_flags);
	void *my_memdup(const void *from, size_t length, myf my_flags);
	char *my_strdup(const char *from, myf my_flags);
	char *my_strndup(const char *from, size_t length, myf my_flags);
}

#endif /* _spw_api_memalloc_h */
