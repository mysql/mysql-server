/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 1993, 1994 Chris Provenzano. 
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)fwalk.c	5.2 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include "local.h"
#include "glue.h"

extern pthread_mutex_t __sfp_mutex;
extern pthread_cond_t __sfp_cond;
extern int __sfp_state;

/*
 * fwalk now can only be used for flushing the buffers.
 * This is all it was originally used for.
 * The function has also become much more complicated.
 * The first time through we flush everything we can.
 * If this fails to flush everything because we couldn't get a lock
 * we wait on the locksfor the second pass. Why this works ...
 * 
 * This function must allow for multiple threads to flush everything.
 * This function cannot flush buffers locked by another thread.
 * So we flush everything we can the first pass. This includes all
 * buffers locked by this thread, and wait on buffers that are locked.
 * Eventually other threads willl unlock there buffers or flush them themselves
 * at which point this thread will notice that it's empty or be able to
 * flush the buffer. This is fine so long as no other thread tries to flush
 * all buffers. Here is the possible deadlock condition, but since this thread
 * has flushed all buffers it can, there are NO buffers locked by this thread
 * that need flushing so any other thread flushing won't block waiting on this
 * thread thereby eliminating the deadlock condition.
 */

int __swalk_sflush()
{
	register FILE *fp, *savefp;
	register int n, ret, saven;
	register struct glue *g, *saveg;

	/* Only allow other threads to read __sglue */
	pthread_mutex_lock(&__sfp_mutex);
	__sfp_state++;
	pthread_mutex_unlock(&__sfp_mutex);

	ret = 0;
	saven = 0;
	saveg = NULL;
	savefp = NULL;
	for (g = &__sglue; g != NULL; g = g->next) {
		for (fp = g->iobs, n = g->niobs; --n >= 0; fp++) {
			if (fp->_flags != 0) {			
				/* Is there anything to flush? */
				if (fp->_bf._base && (fp->_bf._base - fp->_p)) {
					if (ftrylockfile(fp)) {	/* Can we flush it */
						if (!saven) {	/* No, save first fp we can't flush */
							saven = n;
							saveg = g;
							savefp = fp;
							continue;
						}
					} else {
						ret |= __sflush(fp);
						funlockfile(fp);
					}
				}
			}
		}
	}
	if (savefp) {
		for (g = saveg; g != NULL; g = g->next) {
			for (fp = savefp, n = saven + 1; --n >= 0; fp++) {
				if (fp->_flags != 0) {		
 					/* Anything to flush */
					while (fp->_bf._base && (fp->_bf._base - fp->_p)) {
						flockfile(fp);
						ret |= __sflush(fp);
						funlockfile(fp);
					}
				}
			}
		}
	}

	/* If no other readers wakeup a thread waiting to do __sfp */
	pthread_mutex_lock(&__sfp_mutex);
	if (! (--__sfp_state)) {
		pthread_cond_signal(&__sfp_cond);
	}
	pthread_mutex_unlock(&__sfp_mutex);
	return (ret);
}

