/*
 * Copyright (c) 1988 Regents of the University of California.
 * Copyright (c) 1993, 1994 Chris Provenzano proven@mit.edu
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
static char sccsid[] = "@(#)perror.c	5.11 (Berkeley) 2/24/91";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

char *strerror(int); 	/* For systems that don't prototype it in string.h */

void
perror(s)
	const char *s;
{
	char * e;

    if (fd_lock(STDERR_FILENO, FD_WRITE, NULL) == OK) {
		if (s && *s) {
			fd_table[STDERR_FILENO]->ops->write(fd_table[STDERR_FILENO]->fd, 
			  fd_table[STDERR_FILENO]->flags, s, strlen(s), NULL);
			fd_table[STDERR_FILENO]->ops->write(fd_table[STDERR_FILENO]->fd, 
			  fd_table[STDERR_FILENO]->flags, ": ", 2, NULL);
		}
		e = strerror(errno);
		fd_table[STDERR_FILENO]->ops->write(fd_table[STDERR_FILENO]->fd, 
		  fd_table[STDERR_FILENO]->flags, e, strlen(e), NULL);
		fd_table[STDERR_FILENO]->ops->write(fd_table[STDERR_FILENO]->fd, 
		  fd_table[STDERR_FILENO]->flags, "\n", 1, NULL);
		fd_unlock(STDERR_FILENO, FD_WRITE);
	}
}
