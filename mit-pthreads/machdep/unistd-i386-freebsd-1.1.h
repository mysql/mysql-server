/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *
 *	@(#)unistd.h	5.13 (Berkeley) 6/17/91
 */

#ifndef _UNISTD_H_
#define	_UNISTD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/unistd.h>

#define	 STDIN_FILENO	0	/* standard input file descriptor */
#define	STDOUT_FILENO	1	/* standard output file descriptor */
#define	STDERR_FILENO	2	/* standard error file descriptor */

#ifndef NULL
#define	NULL		0	/* null pointer constant */
#endif

typedef	int ssize_t;		/* count of bytes or error indication */

__BEGIN_DECLS
void	 _exit __P_((int));
int	 access __P_((const char *, int));
u_int	 alarm __P_((u_int));
int	 chdir __P_((const char *));
int	 chown __P_((const char *, uid_t, gid_t));
int	 close __P_((int));
char	*cuserid __P_((char *));
int	 dup __P_((int));
int	 dup2 __P_((int, int));
int	 execl __P_((const char *, const char *, ...));
int	 execle __P_((const char *, const char *, ...));
int	 execlp __P_((const char *, const char *, ...));
int	 execv __P_((const char *, char * const *));
int	 execve __P_((const char *, char * const *, char * const *));
int	 execvp __P_((const char *, char * const *));
pid_t	 fork __P_((void));
long	 fpathconf __P_((int, int));		/* not yet */
char	*getcwd __P_((char *, size_t));
gid_t	 getegid __P_((void));
uid_t	 geteuid __P_((void));
gid_t	 getgid __P_((void));
int	 getgroups __P_((int, int *));		/* XXX (gid_t *) */
char	*getlogin __P_((void));
pid_t	 getpgrp __P_((void));
pid_t	 getpid __P_((void));
pid_t	 getppid __P_((void));
uid_t	 getuid __P_((void));
int	 isatty __P_((int));
int	 link __P_((const char *, const char *));
off_t	 lseek __P_((int, off_t, int));
long	 pathconf __P_((const char *, int));	/* not yet */
int	 pause __P_((void));
int	 pipe __P_((int *));
ssize_t	 read __P_((int, void *, size_t));
int	 rmdir __P_((const char *));
int	 setgid __P_((gid_t));
int	 setpgid __P_((pid_t, pid_t));
pid_t	 setsid __P_((void));
int	 setuid __P_((uid_t));
u_int	 sleep __P_((u_int));
long	 sysconf __P_((int));			/* not yet */
pid_t	 tcgetpgrp __P_((int));
int	 tcsetpgrp __P_((int, pid_t));
char	*ttyname __P_((int));
int	 unlink __P_((const char *));
ssize_t	 write __P_((int, const void *, size_t));

#ifndef	_POSIX_SOURCE

/* structure timeval required for select() */
#include <sys/time.h>

int	 acct __P_((const char *));
int	 async_daemon __P_((void));
char	*brk __P_((const char *));
int	 chflags __P_((const char *, long));
int	 chroot __P_((const char *));
char	*crypt __P_((const char *, const char *));
int	 des_cipher __P_((const char *, char *, long, int));
int	 des_setkey __P_((const char *key));
int	 encrypt __P_((char *, int));
void	 endusershell __P_((void));
int	 exect __P_((const char *, char * const *, char * const *));
int	 fchdir __P_((int));
int	 fchflags __P_((int, long));
int	 fchown __P_((int, uid_t, gid_t));
int	 fsync __P_((int));
int	 ftruncate __P_((int, off_t));
int	 getdtablesize __P_((void));
long	 gethostid __P_((void));
int	 gethostname __P_((char *, int));
mode_t	 getmode __P_((const void *, mode_t));
int	 getpagesize __P_((void));
char	*getpass __P_((const char *));
char	*getusershell __P_((void));
char	*getwd __P_((char *));			/* obsoleted by getcwd() */
int	 initgroups __P_((const char *, int));
int	 mknod __P_((const char *, mode_t, dev_t));
int	 mkstemp __P_((char *));
char	*mktemp __P_((char *));
int	 nfssvc __P_((int));
int	 nice __P_((int));
void	 psignal __P_((u_int, const char *));
extern char *sys_siglist[];
int	 profil __P_((char *, int, int, int));
int	 rcmd __P_((char **, int, const char *,
		const char *, const char *, int *));
char	*re_comp __P_((const char *));
int	 re_exec __P_((const char *));
int	 readlink __P_((const char *, char *, int));
int	 reboot __P_((int));
int	 revoke __P_((const char *));
int	 rresvport __P_((int *));
int	 ruserok __P_((const char *, int, const char *, const char *));
char	*sbrk __P_((int));
int	 select __P_((int, fd_set *, fd_set *, fd_set *, struct timeval *));
int	 setegid __P_((gid_t));
int	 seteuid __P_((uid_t));
int	 setgroups __P_((int, const int *));
void	 sethostid __P_((long));
int	 sethostname __P_((const char *, int));
int	 setkey __P_((const char *));
int	 setlogin __P_((const char *));
void	*setmode __P_((const char *));
int	 setpgrp __P_((pid_t pid, pid_t pgrp));	/* obsoleted by setpgid() */
int	 setregid __P_((int, int));
int	 setreuid __P_((int, int));
int	 setrgid __P_((gid_t));
int	 setruid __P_((uid_t));
void	 setusershell __P_((void));
int	 swapon __P_((const char *));
int	 symlink __P_((const char *, const char *));
void	 sync __P_((void));
int	 syscall __P_((int, ...));
int	 truncate __P_((const char *, off_t));
int	 ttyslot __P_((void));
u_int	 ualarm __P_((u_int, u_int));
void	 usleep __P_((u_int));
void	*valloc __P_((size_t));			/* obsoleted by malloc() */
int	 vfork __P_((void));

#endif /* !_POSIX_SOURCE */
__END_DECLS

#endif /* !_UNISTD_H_ */
