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
/*static char *sccsid = "from: @(#)setvbuf.c	5.5 (Berkeley) 5/6/93";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "local.h"

/*
 * Set one of the three kinds of buffering, optionally including
 * a buffer.
 */
setvbuf(fp, buf, mode, size)
	register FILE *fp;
	char *buf;
	register int mode;
	register size_t size;
{
	register int ret, flags;
	size_t iosize;
	int ttyflag;

	/*
	 * Verify arguments.  The `int' limit on `size' is due to this
	 * particular implementation.  Note, buf and size are ignored
	 * when setting _IONBF.
	 */
	if (mode != _IONBF)
		if ((mode != _IOFBF && mode != _IOLBF) || (int)size < 0)
			return (EOF);

	flockfile(fp);
	/*
	 * Write current buffer, if any.  Discard unread input, cancel
	 * line buffering, and free old buffer if malloc()ed.
	 */
	ret = 0;
	(void)__sflush(fp);
	fp->_r = fp->_lbfsize = 0;
	flags = fp->_flags;
	if (flags & __SMBF)
		free((void *)fp->_bf._base);
	flags &= ~(__SLBF | __SNBF | __SMBF | __SOPT | __SNPT);

	/* If setting unbuffered mode, skip all the hard work. */
	if (mode == _IONBF)
		goto nbf;

	/*
	 * Find optimal I/O size for seek optimization.  This also returns
	 * a `tty flag' to suggest that we check isatty(fd), but we do not
	 * care since our caller told us how to buffer.
	 */
	flags |= __swhatbuf(fp, &iosize, &ttyflag);
	if (size == 0) {
		buf = NULL;	/* force local allocation */
		size = iosize;
	}

	/* Allocate buffer if needed. */
	if (buf == NULL) {
		if ((buf = malloc(size)) == NULL) {
			/*
			 * Unable to honor user's request.  We will return
			 * failure, but try again with file system size.
			 */
			ret = EOF;
			if (size != iosize) {
				size = iosize;
				buf = malloc(size);
			}
		}
		if (buf == NULL) {
			/* No luck; switch to unbuffered I/O. */
nbf:
			fp->_flags = flags | __SNBF;
			fp->_w = 0;
			fp->_bf._base = fp->_p = fp->_nbuf;
			fp->_bf._size = 1;
			funlockfile(fp);
			return (ret);
		}
		flags |= __SMBF;
	}

	/*
	 * Kill any seek optimization if the buffer is not the
	 * right size.
	 *
	 * SHOULD WE ALLOW MULTIPLES HERE (i.e., ok iff (size % iosize) == 0)?
	 */
	if (size != iosize)
		flags |= __SNPT;

	/*
	 * Fix up the FILE fields, and set __cleanup for output flush on
	 * exit (since we are buffered in some way).  If in r/w mode, go
	 * to the intermediate state, so that everyone has to call
	 * __srefill or __swsetup on the first operation -- it is more
	 * trouble than it is worth to set things up correctly here.
	 */
	if (mode == _IOLBF)
		flags |= __SLBF;
	if (flags & __SRW)
		flags &= ~(__SRD | __SWR);
	fp->_w = size; /* Was 0 (mevans) */
	fp->_flags = flags;
	fp->_bf._base = fp->_p = (unsigned char *)buf;
	fp->_bf._size = size;
	fp->_lbfsize = 0;
	__cleanup = _cleanup;

	funlockfile(fp);
	return (ret);
}
