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
/*static char *sccsid = "from: @(#)fgetline.c	5.2 (Berkeley) 5/4/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "local.h"

/*
 * Expand the line buffer.  Return -1 on error.
 * The `new size' does not account for a terminating '\0',
 * so we add 1 here.
 */
__slbexpand(fp, newsize)
	FILE *fp;
	size_t newsize;
{
	void *p;

	if (fp->_lb._size >= ++newsize)
		return (0);
	if ((p = realloc(fp->_lb._base, newsize)) == NULL)
		return (-1);
	fp->_lb._base = p;
	fp->_lb._size = newsize;
	return (0);
}

/*
 * Get an input line.  The returned pointer often (but not always)
 * points into a stdio buffer.  Fgetline smashes the newline (if any)
 * in the stdio buffer; callers must not use it on streams that
 * have `magic' setvbuf() games happening.
 */
char *
fgetline(fp, lenp)
	register FILE *fp;
	size_t *lenp;
{
	register unsigned char *p;
	register size_t len;
	size_t off;

	flockfile(fp);

	/* make sure there is input */
	if (fp->_r <= 0 && __srefill(fp)) {
		if (lenp != NULL)
			*lenp = 0;
		funlockfile(fp);
		return (NULL);
	}

	/* look for a newline in the input */
	if ((p = memchr((void *)fp->_p, '\n', fp->_r)) != NULL) {
		register char *ret;

		/*
		 * Found one.  Flag buffer as modified to keep
		 * fseek from `optimising' a backward seek, since
		 * the newline is about to be trashed.  (We should
		 * be able to get away with doing this only if
		 * p is not pointing into an ungetc buffer, since
		 * fseek discards ungetc data, but this is the
		 * usual case anyway.)
		 */
		ret = (char *)fp->_p;
		len = p - fp->_p;
		fp->_flags |= __SMOD;
		*p = 0;
		fp->_r -= len + 1;
		fp->_p = p + 1;
		if (lenp != NULL)
			*lenp = len;
		funlockfile(fp);
		return (ret);
	}

	/*
	 * We have to copy the current buffered data to the line buffer.
	 *
	 * OPTIMISTIC is length that we (optimistically)
	 * expect will accomodate the `rest' of the string,
	 * on each trip through the loop below.
	 */
#define OPTIMISTIC 80

	for (len = fp->_r, off = 0;; len += fp->_r) {
		register size_t diff;

		/*
		 * Make sure there is room for more bytes.
		 * Copy data from file buffer to line buffer,
		 * refill file and look for newline.  The
		 * loop stops only when we find a newline.
		 */
		if (__slbexpand(fp, len + OPTIMISTIC))
			goto error;
		(void) memcpy((void *)(fp->_lb._base + off), (void *)fp->_p, 
		    len - off);
		off = len;
		if (__srefill(fp))
			break;	/* EOF or error: return partial line */
		if ((p = memchr((void *)fp->_p, '\n', fp->_r)) == NULL)
			continue;

		/* got it: finish up the line (like code above) */
		fp->_flags |= __SMOD;	/* soon */
		diff = p - fp->_p;
		len += diff;
		if (__slbexpand(fp, len))
			goto error;
		(void) memcpy((void *)(fp->_lb._base + off), (void *)fp->_p, diff);
		fp->_r -= diff + 1;
		fp->_p = p + 1;
		break;
	}
	if (lenp != NULL)
		*lenp = len;
	fp->_lb._base[len] = 0;

	funlockfile(fp);
	return ((char *)fp->_lb._base);

error:
	if (lenp != NULL)
		*lenp = 0;	/* ??? */
	funlockfile(fp);
	return (NULL);		/* ??? */
}
