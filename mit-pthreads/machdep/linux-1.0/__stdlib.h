
#ifndef	_SYS___STDLIB_H_
#define	_SYS___STDLIB_H_

#include <features.h>

/* Get size_t, wchar_t and NULL from <stddef.h>.  */
#define __need_size_t
#define __need_wchar_t
#define __need_NULL
#include <stddef.h>

#define __need_Emath
#include <errno.h>

/* Get HUGE_VAL (returned by strtod on overflow) from <float.h>.  */
#define __need_HUGE_VAL
#include <float.h>

#endif
