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
/*static char *sccsid = "from: @(#)fflush.c	5.1 (Berkeley) 1/20/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include "local.h"

/* Flush a single file, or (if fp is NULL) all files.  */
fflush(fp)
	register FILE *fp;
{
	int retval;

	if (fp == NULL)
		return (__swalk_sflush());
	flockfile(fp);

	if ((fp->_flags & (__SWR | __SRW)) == 0) {
		errno = EBADF;
		retval = EOF;
	} else {
		retval = __sflush(fp);
	}
	funlockfile(fp);
	return(retval);
}

__sflush(fp)
	register FILE *fp;
{
	register unsigned char *p;
	register int n, t;

	t = fp->_flags;
	if ((t & __SWR) == 0)
		return (0);

	if ((p = fp->_bf._base) == NULL)
		return (0);

	n = fp->_p - p;		/* write this much */

	/*
	 * Set these immediately to avoid problems with longjmp and to allow
	 * exchange buffering (via setvbuf) in user write function.
	 */
	fp->_p = p;
	fp->_w = t & (__SLBF|__SNBF) ? 0 : fp->_bf._size;

	for (; n > 0; n -= t, p += t) {
		t = __swrite(fp, (char *)p, n);
		if (t <= 0) {
			fp->_flags |= __SERR;
			return (EOF);
		}
	}
	return (0);
}
