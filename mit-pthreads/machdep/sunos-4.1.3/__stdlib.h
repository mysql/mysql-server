/* $Id$ */

#ifndef  __sys_stdtypes_h

#ifndef	_SYS___STDLIB_H_
#define	_SYS___STDLIB_H_

#include <sys/stdtypes.h>   /* to get size_t */

#if (! defined _SIZE_T_ ) && (! defined(_GCC_SIZE_T))
#define _SIZE_T_
#define _GCC_SIZE_T
typedef pthread_size_t    size_t;
#endif

#if (! defined _WCHAR_T_ ) && (! defined(_GCC_WCHAR_T))
#define _WCHAR_T_
#define _GCC_WCHAR_T
typedef unsigned int	wchar_t;
#endif

#ifndef NULL
#define NULL    0
#endif

#endif

#endif
