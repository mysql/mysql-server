
#ifndef	_SYS___STDLIB_H_
#define	_SYS___STDLIB_H_

#include <sys/feature_tests.h>

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int    size_t;
#endif

#ifndef _UID_T
#define _UID_T
typedef long    uid_t;
#endif

#ifndef NULL
#define NULL    0
#endif

#ifndef _WCHAR_T
#define _WCHAR_T
typedef long wchar_t;
#endif


#endif
