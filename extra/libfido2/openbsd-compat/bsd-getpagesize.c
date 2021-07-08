/* Placed in the public domain */

#include "openbsd-compat.h"

#if !defined(HAVE_GETPAGESIZE)

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <limits.h>

int
getpagesize(void)
{
#if defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE)
	long r = sysconf(_SC_PAGESIZE);
	if (r > 0 && r < INT_MAX)
		return (int)r;
#endif
	/*
	 * This is at the lower end of common values and appropriate for
	 * our current use of getpagesize() in recallocarray().
	 */
	return 4096;
}

#endif /* !defined(HAVE_GETPAGESIZE) */
