/* ==== file.c ============================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
 * All rights reserved.
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
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : The locking functions for stdio.
 *
 *  1.00 93/09/04 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <stdio.h>

/* ==========================================================================
 * flockfile()
 */
void flockfile(FILE *fp)
{
	pthread_mutex_t *mutex;
	int fd, flags;

	if ((fd = fileno(fp)) >= 0) {
		pthread_mutex_lock(mutex = &(fd_table[fd]->mutex));

		if (fp->_flags & __SRW) {
			flags = FD_READ | FD_WRITE;
		} else {
			if (fp->_flags & __SWR) {
				flags = FD_WRITE;
			} else {
				flags = FD_READ;
			}
		}

		/* This might fail but POSIX doesn't give a damn. */
		fd_basic_lock(fd, flags, mutex, NULL);
		pthread_mutex_unlock(mutex);
	}
}

/* ==========================================================================
 * ftrylockfile()
 */
int ftrylockfile(FILE *fp)
{
	pthread_mutex_t *mutex;
	int fd, flags;

	if ((fd = fileno(fp)) >= 0) {
		pthread_mutex_lock(mutex = &(fd_table[fd]->mutex));

		if (fp->_flags & __SRW) {
			flags = FD_READ | FD_WRITE;
		} else {
			if (fp->_flags & __SWR) {
				flags = FD_WRITE;
			} else {
				flags = FD_READ;
			}
		}
		if (!(fd_table[fd]->r_owner && fd_table[fd]->w_owner)) {
			fd_basic_lock(fd, flags, mutex, NULL);
			fd = OK;
		} else {
			fd = NOTOK;
		}
		pthread_mutex_unlock(mutex);
	} else {
		fd = OK;
	}
	return(fd);
}

/* ==========================================================================
 * funlockfile()
 */
void funlockfile(FILE *fp)
{
	pthread_mutex_t *mutex;
	int fd, flags;

	if ((fd = fileno(fp)) >= 0) {
		pthread_mutex_lock(mutex = &(fd_table[fd]->mutex));

		if (fp->_flags & __SRW) {
			flags = FD_READ | FD_WRITE;
		} else {
			if (fp->_flags & __SWR) {
				flags = FD_WRITE;
			} else {
				flags = FD_READ;
			}
		}
		fd_basic_unlock(fd, flags);
		pthread_mutex_unlock(mutex);
	}
}

