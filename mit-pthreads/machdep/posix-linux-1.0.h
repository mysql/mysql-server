/* ==== posix.h ============================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@athena.mit.edu	
 *
 * $Id$
 *
 * Description : Convert a Linux-1.0 system to a more or less POSIX system.
 *				 Mostly POSIX already
 */
 
#ifndef _PTHREAD_POSIX_H_
#define _PTHREAD_POSIX_H_

#include <sys/cdefs.h>

/* Make sure we have size_t defined */
#include <pthread/types.h>

#define __INLINE	extern inline
/*
 * OK now do stuff to make the code compile. 
 * Every OS has its own prototypes for each function
 */
#ifdef malloc
#undef malloc
#endif

#ifdef free
#undef free
#endif

#endif
