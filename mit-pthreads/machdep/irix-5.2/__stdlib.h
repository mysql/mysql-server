#include <sgidefs.h>

#if !defined(_SIZE_T) && !defined(_SIZE_T_)
#define _SIZE_T
#if (_MIPS_SZLONG == 32)
typedef unsigned int    size_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef unsigned long   size_t;
#endif
#endif

#ifndef _WCHAR_T
#define _WCHAR_T
#if (_MIPS_SZLONG == 32)
typedef long wchar_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef __int32_t wchar_t;
#endif
#endif

#ifndef __NORETURN
#define __NORETURN
#endif

#ifndef NULL
#define NULL    0
#endif

