/* ==== posix.h ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu	
 *
 * Description : Convert a system to a more or less POSIX system.
 *
 *  1.00 93/07/20 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#ifndef O_NONBLOCK
#ifdef FNDELAY
#define	O_NONBLOCK	FNDELAY
#endif
#endif

#ifndef O_ACCMODE
#define O_ACCMODE (O_RDONLY|O_RDWR|O_WRONLY)
#endif

#ifndef	S_ISREG
#define S_ISREG(x)	((x & S_IFMT) == S_IFREG)
#endif

#ifndef ENOSYS
#define	ENOSYS	EINVAL
#endif

/* Make sure we have size_t defined */
#include <pthread/types.h>
