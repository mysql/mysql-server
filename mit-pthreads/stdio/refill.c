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
/*static char *sccsid = "from: @(#)refill.c	5.3 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "local.h"
#include "glue.h"

extern pthread_mutex_t __sfp_mutex;
extern pthread_cond_t __sfp_cond;
extern struct glue __sglue;
extern int __sfp_state;

/* This function is very similar to __swalk_sflush */
static void __swalk_lflush()
{
	register FILE *fp, *savefp;
    register int n, saven;
    register struct glue *g, *saveg;

	/* Only allow other threads to read __sglue */
    pthread_mutex_lock(&__sfp_mutex);
    __sfp_state++;
    pthread_mutex_unlock(&__sfp_mutex);

    saven = 0;
    saveg = NULL;
    savefp = NULL;
    for (g = &__sglue; g != NULL; g = g->next) {
        for (fp = g->iobs, n = g->niobs; --n >= 0; fp++) {
            if ((fp->_flags & (__SLBF|__SWR)) == (__SLBF|__SWR)) {
                /* Is there anything to flush? */
                if (fp->_bf._base && (fp->_bf._base - fp->_p)) {
                    if (ftrylockfile(fp)) { /* Can we flush it */
                        if (!saven) {       /* No, save first fp we can't flush */
                            saven = n;
                            saveg = g;
                            savefp = fp;
                            continue;
                        }
					  } else {
                        (void) __sflush(fp);
						funlockfile(fp);
                    }
                }
            }
        }
    }
	if (savefp) {
        for (g = saveg; g != NULL; g = g->next) {
            for (fp = savefp, n = saven + 1; --n >= 0; fp++) {
            	if ((fp->_flags & (__SLBF|__SWR)) == (__SLBF|__SWR)) {
                    /* Anything to flush */
                    while (fp->_bf._base && (fp->_bf._base - fp->_p)) {
                        flockfile(fp);
						(void) __sflush(fp);
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
}

/*
 * Refill a stdio buffer.
 * Return EOF on eof or error, 0 otherwise.
 */
__srefill(fp)
	register FILE *fp;
{

	/* make sure stdio is set up */
	__sinit ();

	fp->_r = 0;		/* largely a convenience for callers */

	/* SysV does not make this test; take it out for compatibility */
	if (fp->_flags & __SEOF)
		return (EOF);

	/* if not already reading, have to be reading and writing */
	if ((fp->_flags & __SRD) == 0) {
		if ((fp->_flags & __SRW) == 0) {
			errno = EBADF;
			return (EOF);
		}
		/* switch to reading */
		if (fp->_flags & __SWR) {
			if (__sflush(fp))
				return (EOF);
			fp->_flags &= ~__SWR;
			fp->_w = 0;
			fp->_lbfsize = 0;
		}
		fp->_flags |= __SRD;
	} else {
		/*
		 * We were reading.  If there is an ungetc buffer,
		 * we must have been reading from that.  Drop it,
		 * restoring the previous buffer (if any).  If there
		 * is anything in that buffer, return.
		 */
		if (HASUB(fp)) {
			FREEUB(fp);
			if ((fp->_r = fp->_ur) != 0) {
				fp->_p = fp->_up;
				return (0);
			}
		}
	}

	if (fp->_file == -1) {
		fp->_flags |= __SEOF;
		return(EOF);
	}

	if (fp->_bf._base == NULL)
		__smakebuf(fp);

	/*
	 * Before reading from a line buffered or unbuffered file,
	 * flush all line buffered output files, per the ANSI C
	 * standard.
	 */
	if (fp->_flags & (__SLBF|__SNBF))
		__swalk_lflush();
	fp->_p = fp->_bf._base;
	fp->_r = __sread(fp, (char *)fp->_p, fp->_bf._size);
	fp->_flags &= ~__SMOD;	/* buffer contents are again pristine */
	if (fp->_r <= 0) {
		if (fp->_r == 0)
			fp->_flags |= __SEOF;
		else {
			fp->_r = 0;
			fp->_flags |= __SERR;
		}
		return (EOF);
	}
	return (0);
}
