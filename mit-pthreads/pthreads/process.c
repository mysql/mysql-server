/* ==== process.c ============================================================
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
 * Description : Process functions (fork, exec, ...).
 *
 *  1.23 94/04/18 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdarg.h>
#include <unistd.h>
#ifdef HAVE_ALLOC_H
#include <alloc.h>
#endif

extern void *alloca();

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

/* ==========================================================================
 * fork()
 *
 * This function requires a sig_prevent()/sig_check_and_resume() for the
 * parent. The child never unlocks.
 */
pid_t fork()
{
	pid_t ret;

	pthread_sched_prevent();

	fd_kern_fork();
	if (ret = machdep_sys_fork()) { /* Parent or error */
		pthread_sched_resume();
	} else { /* Child */
		machdep_unset_thread_timer(NULL);
		machdep_stop_timer(NULL);
		fork_lock++;
                pthread_kernel_lock--;
	}
	return(ret);
}

#ifdef HAVE_VFORK
/* The semantics of vfork probably won't mix well with the pthread
   library code.  Don't even try.  */
pid_t vfork ()
{
	return fork ();
}
#endif

/* ==========================================================================
 * execve()
 *
 * This function requires a sig_prevent()/sig_check_and_resume() if one
 * hasn't been done in the fork routine. Normally machdep_sys_execve()
 * should never return.
 */
int execve(const char *name, char * const *argv, char * const *envp) 
{
	int ret;

	if (!fork_lock) {
		pthread_sched_prevent();
		fd_kern_exec(0);
		ret = machdep_sys_execve(name, argv, envp);
		pthread_sched_resume();
	} else {
		fd_kern_exec(1);
		ret = machdep_sys_execve(name, argv, envp);
	}
	return(ret);
}

/* Variants of execve.  Define them here so that the system versions
   don't get used and drag in the system version of execve.  */
#include <sys/stat.h>
#include <string.h>
#include <sys/param.h>
extern char **environ;

static const char *find (const char *name, char *buf)
{
	char *p1, *p2;
	extern char *getenv ();
	struct stat sb;

	if (strchr (name, '/'))
		return name;
	p1 = getenv ("PATH");
	if (p1 == 0)
		p1 = "/bin:/usr/bin:";
	while (*p1) {
		memset (buf, 0, MAXPATHLEN);
		p2 = strchr (p1, ':');
		if (p2 == 0)
			p2 = p1 + strlen (p1);
		strncpy (buf, p1, p2 - p1);
		buf[p2 - p1] = 0;
		strcat (buf, "/");
		strcat (buf, name);
		if (lstat (buf, &sb) == 0)
			return buf;

		if (*p2 == ':')
			p2++;
		p1 = p2;
	}
	return name;
}

int execl (const char *path, const char *arg, ...)
{
#ifdef SCO_3_5
  return execve (path, (char *const *) &arg, environ);
#else
  char ** argv;
  va_list ap;
  int i;

  va_start(ap, arg);
  for (i = 1; va_arg(ap, char *) != NULL; i++);
  va_end(ap);

  argv = alloca (i * sizeof (char *));

  va_start(ap, arg);
  argv[0] = (char *) arg;
  for (i = 1; (argv[i] = (char *) va_arg(ap, char *)) != NULL; i++);
  va_end(ap);

  return execve (path, argv, environ);
#endif
}

int execlp (const char *name, const char *arg, ...)
{
#ifdef SCO_3_5
  char buf[MAXPATHLEN];
  return execve (find (name, buf), (char *const *) &arg, environ);
#else
  char buf[MAXPATHLEN];
  char ** argv;
  va_list ap;
  int i;
 
  va_start(ap, arg);
  for (i = 1; va_arg(ap, char *) != NULL; i++);
  va_end(ap);
 
  argv = alloca (i * sizeof (char *));

  va_start(ap, arg);
  argv[0] = (char *) arg;
  for (i = 1; (argv[i] = (char *) va_arg(ap, char *)) != NULL; i++);
  va_end(ap);

  return execve (find (name, buf), argv, environ);
#endif
}

int execle (const char *name, const char *arg, ... /* , char *const envp[] */);

/* This one turns on ptrace-style tracing?  */
int exect (const char *path, char *const argv[], char *const envp[]);

int execv (const char *path, char *const argv[]) {
	return execve (path, argv, environ);
}

int execvp (const char *name, char *const argv[]) {
	char buf[MAXPATHLEN];
	return execve (find (name, buf), argv, environ);
}
