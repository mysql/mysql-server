/*
 * Copyright (c) 1988 The Regents of the University of California.
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
/*static char *sccsid = "from: @(#)isatty.c	5.6 (Berkeley) 2/23/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#ifdef sunos4
#include <sys/termio.h>
#else
#include <termios.h>
#endif
#include <unistd.h>

/*
 * If TIOCGETA is not defined try TCGETATTR
 * If TCGETATTR is not defined try TCGETA
 * If that doesn't work try getting it from termio.h
 */
#ifndef TIOCGETA
#ifdef TCGETATTR
#define TIOCGETA    TCGETATTR
#else
#ifndef TCGETA
#include <termio.h>
#endif 
#ifndef TIOCGETA
#define TIOCGETA    TCGETA
#endif
#endif 
#endif

/* fd is the real fd to pass to the kernel */
int isatty_basic(int fd)
{
#ifdef sunos4
	struct termio t;
#else /* !sunos4 */
	struct termios t;
#endif /* sunos4 */
	return (machdep_sys_ioctl(fd,
#ifdef sunos4
				  TCGETA,
#else /* !sunos4 */
				  TIOCGETA,
#endif /* sunos4 */
				  &t) ? 0 : 1);
}

int isatty(int fd)
{
	int ret;

	if ((ret = fd_lock(fd, FD_READ, NULL)) == OK) {
		ret = isatty_basic(fd_table[fd]->fd.i);
		fd_unlock(fd, FD_READ);
	} else {
		/* Return 0 or 1 */
		ret = 0;
	}
	return(ret);
}

