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
/*static char *sccsid = "from: @(#)fopen.c	5.5 (Berkeley) 2/5/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "local.h"

extern pthread_mutex_t __sfp_mutex;
extern pthread_cond_t __sfp_cond;
extern int __sfp_state;

FILE *fopen(const char *file, const char *mode)
{
	register FILE *fp;
	register int f;
	int flags, oflags;

	if ((flags = __sflags(mode, &oflags)) == 0)
		return (NULL);
	if ((f = open(file, oflags, 0666)) < 0) {
		return (NULL);
	}

	__sinit ();
    pthread_mutex_lock(&__sfp_mutex);
    while (__sfp_state) {
        pthread_cond_wait(&__sfp_cond, &__sfp_mutex);
    }

	if (fp = __sfp()) {
		fp->_file = f;
		fp->_flags = flags;
	
		/*
		 * When opening in append mode, even though we use O_APPEND,
		 * we need to seek to the end so that ftell() gets the right
		 * answer.  If the user then alters the seek pointer, or
		 * the file extends, this will fail, but there is not much
		 * we can do about this.  (We could set __SAPP and check in
		 * fseek and ftell.)
		 */
		if (oflags & O_APPEND)
			(void) __sseek((void *)fp, (off_t)0, SEEK_END);
	}
	pthread_mutex_unlock(&__sfp_mutex);
	return (fp);
}
