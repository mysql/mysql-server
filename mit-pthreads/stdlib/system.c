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
/*static char *sccsid = "from: @(#)system.c	5.10 (Berkeley) 2/23/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <pthread/paths.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

extern char **environ;

system(command)
	const char *command;
{
    char *argp[] = {"sh", "-c", "*to be filled in*", NULL};
    void (*intsave)(), (*quitsave)(), (*signal())();
    sigset_t tmp_mask, old_mask;
    int pstat;
    pid_t pid;

	if (!command)		/* just checking... */
		return(1);

	argp[2] = (char *) command;
	sigemptyset(&tmp_mask);
	sigaddset(&tmp_mask, SIGCHLD);
	pthread_sigmask(SIG_BLOCK, &tmp_mask, &old_mask);
	switch(pid = fork()) {
	case -1:			/* error */
		(void)pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
		return(-1);
	case 0:				/* child */
		(void)pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
		execve(_PATH_BSHELL, argp, environ);
		_exit(127);
	}

	intsave = pthread_signal(SIGINT, SIG_IGN);
	quitsave = pthread_signal(SIGQUIT, SIG_IGN);
	pid = waitpid(pid, (int *)&pstat, 0);
	(void)pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
	(void)pthread_signal(SIGQUIT, quitsave);
	(void)pthread_signal(SIGINT, intsave);
	return(pid == -1 ? -1 : pstat);
}
