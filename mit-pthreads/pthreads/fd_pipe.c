/* ==== fd_pipe.c ============================================================
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
 * Description : The new fast ITC pipe routines.
 *
 *  1.00 93/08/14 proven
 *      -Started coding this file.
 *
 *	1.01 93/11/13 proven
 *		-The functions readv() and writev() added.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <pthread/fd_pipe.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread/posix.h>
#include <string.h>
#include <stdlib.h>

#ifndef MIN
#define MIN(a,b)	((a)<(b)?(a):(b))
#endif

/* ==========================================================================
 * The pipe lock is never unlocked until all pthreads waiting are done with it
 * read()
 */
pthread_ssize_t __pipe_read(union fd_data fd_data, int flags, void *buf,
 size_t nbytes, struct timespec * timeout)
{
	struct __pipe *fd = (struct __pipe *)fd_data.ptr;
	struct pthread * pthread;
	int ret = 0;

	if (flags & O_ACCMODE) { return(NOTOK); }

	/* If there is nothing to read, go to sleep */
	if (fd->count == 0) {
		if (flags == WR_CLOSED) {
			return(0);
		}

		pthread_sched_prevent();

		/* queue pthread for a FDR_WAIT */
		pthread_run->next = NULL;
		fd->wait = pthread_run;
		
		pthread_resched_resume(PS_FDR_WAIT);
		ret = fd->size;
	} else {
		ret = MIN(nbytes, fd->count);
		memcpy(buf, fd->buf + fd->offset, ret);
		if (!(fd->count -= ret)) {
			fd->offset = 0;
		}

		if (pthread = fd->wait) {
			fd->wait = NULL;
			pthread_sched_prevent();
			pthread_sched_other_resume(pthread);
		}
	}
	return(ret);
}

/* ==========================================================================
 * __pipe_write()
 *
 * First check to see if the read side is still open, then
 * check to see if there is a thread in a read wait for this pipe, if so
 * copy as much data as possible directly into the read waiting threads
 * buffer. The write thread(whether or not there was a read thread)
 * copies as much data as it can into the pipe buffer and it there
 * is still data it goes to sleep.
 */
pthread_ssize_t __pipe_write(union fd_data fd_data, int flags, const void *buf,
 size_t nbytes, struct timespec * timeout) {
	struct __pipe *fd = (struct __pipe *)fd_data.ptr;
	struct pthread * pthread;
	int ret, count;

	if (!(flags & O_ACCMODE)) { return(NOTOK); }

	while (fd->flags != RD_CLOSED) {
		if (pthread = fd->wait) {

			pthread_sched_prevent();
	
			/* Copy data directly into waiting pthreads buf */
			fd->wait_size = MIN(nbytes, fd->wait_size);
			memcpy(fd->wait_buf, buf, fd->wait_size);
			buf = (const char *)buf + fd->wait_size;
			nbytes -= fd->wait_size;
			ret = fd->wait_size;
			fd->wait = NULL;

			/* Wake up waiting pthread */	
			pthread_sched_other_resume(pthread);
		}

		if (count = MIN(nbytes, fd->size - (fd->offset + fd->count))) {
			memcpy(fd->buf + (fd->offset + fd->count), buf, count);
			buf = (const char *)buf + count;
			nbytes -= count;
			ret += count;
		}
		if (nbytes) {
			pthread_sched_prevent();
			fd->wait = pthread_run;
			pthread_resched_resume(PS_FDW_WAIT);
		} else {		
	   	    return(ret);
		}
	}
	return(NOTOK);
}

/* ==========================================================================
 * __pipe_close()
 *
 * The whole close procedure is a bit odd and needs a bit of a rethink.
 * For now close() locks the fd, calls fd_free() which checks to see if
 * there are any other fd values poinging to the same real fd. If so
 * It breaks the wait queue into two sections those that are waiting on fd
 * and those waiting on other fd's. Those that are waiting on fd are connected
 * to the fd_table[fd] queue, and the count is set to zero, (BUT THE LOCK IS NOT
 * RELEASED). close() then calls fd_unlock which give the fd to the next queued
 * element which determins that the fd is closed and then calls fd_unlock etc...
 */
int __pipe_close(struct __pipe *fd, int flags)
{
	struct pthread * pthread;

	if (!(fd->flags)) {
		if (pthread = fd->wait) {
			if (flags & O_ACCMODE) {
				fd->count = 0;
				fd->wait = NULL;
				fd->flags |= WR_CLOSED;
				pthread_sched_prevent();
				pthread_resched_resume(pthread);
			} else {
				/* Should send a signal */
				fd->flags |= RD_CLOSED;
			}
		}
	} else {
		free(fd);
		return(OK);
	}
}

/* ==========================================================================
 * For fcntl() which isn't implemented yet
 * __pipe_enosys()
 */
static int __pipe_enosys()
{
	SET_ERRNO(ENOSYS);
	return(NOTOK);
}

/* ==========================================================================
 * For writev() and readv() which aren't implemented yet
 * __pipe_enosys_v()
 */
static int __pipe_enosys_v(union fd_data fd, int flags,
			   const struct iovec *vec, int nvec,
			   struct timespec *timeout)
{
	SET_ERRNO(ENOSYS);
	return(NOTOK);
}

/* ==========================================================================
 * For lseek() which isn't implemented yet
 * __pipe_enosys_o()
 */
static off_t __pipe_enosys_o()
{
	SET_ERRNO(ENOSYS);
	return(NOTOK);
}

/*
 * File descriptor operations
 */
struct fd_ops fd_ops[] = {
{	__pipe_write, __pipe_read, __pipe_close, __pipe_enosys,
	__pipe_enosys_v, __pipe_enosys_v, __pipe_enosys_o, 0 },
};

/* ==========================================================================
 * open()
 */
/* int __pipe_open(const char *path, int flags, ...) */
int newpipe(int fd[2])
{
	struct __pipe *fd_data;

	if ((!((fd[0] = fd_allocate()) < OK)) && (!((fd[1] = fd_allocate()) < OK))) {
		fd_data = malloc(sizeof(struct __pipe));
		fd_data->buf = malloc(4096);
		fd_data->size = 4096;
		fd_data->count = 0;
		fd_data->offset = 0;

		fd_data->wait = NULL;
		fd_data->flags = 0;

		fd_table[fd[0]]->fd.ptr = fd_data;
		fd_table[fd[0]]->flags = O_RDONLY;
		fd_table[fd[1]]->fd.ptr = fd_data;
		fd_table[fd[1]]->flags = O_WRONLY;

		return(OK);
	}
	return(NOTOK);
}

