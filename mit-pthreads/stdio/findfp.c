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
/*static char *sccsid = "from: @(#)findfp.c	5.10 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "local.h"
#include "glue.h"


#define NSTATIC	20	/* stdin + stdout + stderr + the usual */
#define	NDYNAMIC 10	/* add ten more whenever necessary */

#define	std(flags, file) \
	{0,0,0,flags,file,{0},0 }
/*	 p r w flags file _bf z  */

static FILE usual[NSTATIC - 3];	/* the usual */
static struct glue uglue = { 0, NSTATIC - 3, usual };

FILE __sF[3] = {
	std(__SRD, 0),		/* stdin */
	std(__SWR, 1),		/* stdout */
	std(__SWR|__SNBF, 2)	/* stderr */
};
struct glue __sglue = { &uglue, 3, __sF };
FILE *__iob = __sF;
FILE *_iob = __sF;

pthread_mutex_t __sfp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t 	__sfp_cond	= PTHREAD_COND_INITIALIZER;
/*
 * __sfp_state = 0, when free, > 0 when in _fwalk 
 * This allows multiple readers in _fwalk, but only one writer __sfp,
 * or freopen() at a time.
 */
int	__sfp_state = 0;

static struct glue *moreglue(register int n)
{
	register struct glue *g;
	register FILE *p;
	static FILE empty;

	g = (struct glue *)malloc(sizeof(struct glue) + n * sizeof(FILE));
	if (g == NULL)
		return (NULL);
	p = (FILE *)(g + 1);
	g->next = NULL;
	g->niobs = n;
	g->iobs = p;
	while (--n >= 0)
		*p++ = empty;
	return (g);
}

/*
 * Find a free FILE for fopen et al.
 */
FILE *__sfp()
{
	register FILE *fp;
	register int n;
	register struct glue *g;

	for (g = &__sglue;; g = g->next) {
		for (fp = g->iobs, n = g->niobs; --n >= 0; fp++)
			if (fp->_flags == 0) {
				fp->_flags = 1;		/* reserve this slot; caller sets real flags */
				fp->_p = NULL;		/* no current pointer */
				fp->_w = 0;		/* nothing to read or write */
				fp->_r = 0;
				fp->_bf._base = NULL;	/* no buffer */
				fp->_bf._size = 0;
				fp->_lbfsize = 0;	/* not line buffered */
				fp->_file = -1;		/* no file */
				fp->_ub._base = NULL;	/* no ungetc buffer */
				fp->_ub._size = 0;
				fp->_lb._base = NULL;	/* no line buffer */
				fp->_lb._size = 0;
				goto __sfp_done;
			}
		if (g->next == NULL && (g->next = moreglue(NDYNAMIC)) == NULL) {
			fp = NULL;
			break;
		}
	}
__sfp_done:;
	return (fp);
}

/*
 * exit() calls _cleanup() through *__cleanup, set whenever we
 * open or buffer a file.  This chicanery is done so that programs
 * that do not use stdio need not link it all in.
 *
 * The name `_cleanup' is, alas, fairly well known outside stdio.
 */
void _cleanup()
{
	(void) __swalk_sflush();
}

/*
 * __sinit() is called whenever stdio's internal variables must be set up.
 * Do the pthread_once stuff here to keep pthread_once_t out of the
 * header files.  No reason sprintf.c &c should need to include pthread.h...
 */
static void __s_real_init ()
{
    /* make sure we clean up on exit */
    __cleanup = _cleanup;
}

static pthread_once_t sdidinit = PTHREAD_ONCE_INIT;

void __sinit ()
{
    pthread_once (&sdidinit, __s_real_init);
}
