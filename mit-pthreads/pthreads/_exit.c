/* ==== _exit.c ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@mit.edu
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
 *  1.00 94/09/04 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <fcntl.h>

/* ==========================================================================
 * _exit()
 *
 * Change all file descriptors back to their original state,
 * before exiting for good.
 */
void _exit(int status)
{
	int fd;

	pthread_sched_prevent();

	for (fd = 0; fd < dtablesize; fd++) {
		if (fd_table[fd] == NULL) {
			continue;
		}
		/* Is it a kernel fd ? */
		if ((!fd_table[fd]->ops) || (fd_table[fd]->ops->use_kfds != 1)) {
			continue;
		}
		switch (fd_table[fd]->type) {
		case FD_HALF_DUPLEX:
			machdep_sys_fcntl(fd_table[fd]->fd.i, F_SETFL, fd_table[fd]->flags);
			fd_table[fd]->type = FD_TEST_HALF_DUPLEX;
			break;
		case FD_FULL_DUPLEX:
			machdep_sys_fcntl(fd_table[fd]->fd.i, F_SETFL, fd_table[fd]->flags);
			fd_table[fd]->type = FD_TEST_FULL_DUPLEX;
			break;
		default:
			break;
		}
	}
	machdep_sys_exit(status);
}

