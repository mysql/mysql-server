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
/*static char *sccsid = "from: @(#)freopen.c	5.6 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "local.h"

extern pthread_mutex_t __sfp_mutex;
extern pthread_cond_t __sfp_cond;
extern int __sfp_state;

/* 
 * Re-direct an existing, open (probably) file to some other file. 
 * ANSI is written such that the original file gets closed if at
 * all possible, no matter what.
 */
FILE *
freopen(file, mode, fp)
	const char *file, *mode;
	register FILE *fp;
{
	int f, flags, oflags;
	FILE *ret;

	if ((flags = __sflags(mode, &oflags)) == 0) {
		(void) fclose(fp);
		return (NULL);
	}

	__sinit ();

	/*
	 * There are actually programs that depend on being able to "freopen"
	 * descriptors that weren't originally open.  Keep this from breaking.
	 * Remember whether the stream was open to begin with, and which file
	 * descriptor (if any) was associated with it.  If it was attached to
	 * a descriptor, defer closing it; freopen("/dev/stdin", "r", stdin)
	 * should work.  This is unnecessary if it was not a Unix file.
	 */
	/* while lock __sfp_mutex, to block out fopen, and other freopen calls */
	while (pthread_mutex_lock(&__sfp_mutex) == OK) {
		if (ftrylockfile(fp) == OK) {
			if (fp->_flags) {
				/* flush the stream; ANSI doesn't require this. */
				if (fp->_flags & __SWR) 
					(void) __sflush(fp);
				__sclose(fp);
				/*
			     * Finish closing fp.  We cannot keep fp->_base:
				 * it may be the wrong size.  This loses the effect
				 * of any setbuffer calls, but stdio has always done
				 * this before.
   	     	 	 * NOTE: We do this even if __ftrylockfilr failed with
				 * an error to avoid memory leaks.
			     */
				if (fp->_flags & __SMBF)
					free((char *)fp->_bf._base);
				fp->_w = 0;
				fp->_r = 0;
				fp->_p = NULL;
				fp->_bf._base = NULL;
				fp->_bf._size = 0;
				fp->_lbfsize = 0;
				if (HASUB(fp))
					FREEUB(fp);
				fp->_ub._size = 0;
				if (HASLB(fp))
					FREELB(fp);
				fp->_lb._size = 0;
			} 
			/* Get a new descriptor to refer to the new file. */
			if ((f = open(file, oflags, 0666)) < OK) 
				ret = NULL;
			/*
			 * If reopening something that was open before on a real file, try
	 		 * to maintain the descriptor.  Various C library routines (perror)
	 		 * assume stderr is always fd STDERR_FILENO, even if being 
			 * freopen'd.
	 		 */
			/* Testing f == fp->_file may no longer be necessary */
			if (fp->_file >= 0 && f != fp->_file) {
				if (dup2(f, fp->_file) >= OK) {
					(void)close(f);
					f = fp->_file;
				}
			}
			fp->_flags = flags;
			fp->_file = f;
			ret = fp;
		} else {
			/* unlock __sfp_mutex, and try again later */
			pthread_mutex_unlock(&__sfp_mutex);
			pthread_yield();
			continue;
		}
		/* @@ Yo, Chris!  Between the "break" and "continue" statements
		   above, the program will never get here.  What gives?  */
		pthread_mutex_unlock(&__sfp_mutex);
		funlockfile(fp);
		return(ret);
	}
	(void)fclose(fp);
	return(NULL);
}
