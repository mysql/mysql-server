/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)wait.h	7.4 (Berkeley) 1/27/88
 */
#ifndef __SYS_WAIT_H__
#define __SYS_WAIT_H__

#ifdef _POSIX_SOURCE
#define _W_INT(i)   (i)
#else
#define _W_INT(w)   (*(int *)&(w))  /* convert union wait to int */
#define WCOREFLAG   0200
#endif

#define WSTOPFLG                0177
#define WIFSTOPPED(stat)        ((_W_INT(stat)&0377)==_WSTOPPED&&((_W_INT(stat)>>8)&0377)!=0)
#define WSTOPSIG(stat)			((_W_INT(stat)>>8)&0377)
#define WIFSIGNALED(stat)       ((_W_INT(stat)&0377)>0&&((_W_INT(stat)>>8)&0377)==0)
#define WTERMSIG(stat)			(_W_INT(stat)&0177)
#define WIFEXITED(stat)         ((_W_INT(stat)&0377)==0)
#define WEXITSTATUS(stat)		((_W_INT(stat)>>8)&0377)
#define WCOREDUMP(stat)			(_W_INT(stat) & WCOREFLAG)

/*
 * Option bits for the second argument of wait3.  WNOHANG causes the
 * wait to not hang if there are no stopped or terminated processes, rather
 * returning an error indication in this case (pid==0).  WUNTRACED
 * indicates that the caller should receive status about untraced children
 * which stop due to signals.  If children are stopped and a wait without
 * this option is done, it is as though they were still running... nothing
 * about them is returned.
 */
#define WNOHANG		0100	
#define	WUNTRACED	0004	 /* for POSIX */

#if !defined(_POSIX_SOURCE)

/*
 * Structure of the information in the first word returned by both
 * wait and wait3.  If w_stopval==_WSTOPPED, then the second structure
 * describes the information returned, else the first.  See WUNTRACED below.
 */
typedef union wait	{
	int	w_status;		/* used in syscall */
	/*
	 * Terminated process status.
	 */
	struct {
#ifdef _MIPSEL
		unsigned int	w_Termsig:7,	/* termination signal */
				w_Coredump:1,	/* core dump indicator */
				w_Retcode:8,	/* exit code if w_termsig==0 */
				w_Filler:16;	/* upper bits filler */
#endif
#ifdef _MIPSEB
		unsigned int	w_Filler:16,	/* upper bits filler */
				w_Retcode:8,	/* exit code if w_termsig==0 */
				w_Coredump:1,	/* core dump indicator */
				w_Termsig:7;	/* termination signal */
#endif
	} w_T;
	/*
	 * Stopped process status.  Returned
	 * only for traced children unless requested
	 * with the WUNTRACED option bit.
	 */
	struct {
#ifdef _MIPSEL
		unsigned int	w_Stopval:8,	/* == W_STOPPED if stopped */
				w_Stopsig:8,	/* signal that stopped us */
				w_Filler:16;	/* upper bits filler */
#endif
#ifdef _MIPSEB
		unsigned int	w_Filler:16,	/* upper bits filler */
				w_Stopsig:8,	/* signal that stopped us */
				w_Stopval:8;	/* == W_STOPPED if stopped */
#endif
	} w_S;
} wait_t;
#define	w_termsig	w_T.w_Termsig
#define w_coredump	w_T.w_Coredump
#define w_retcode	w_T.w_Retcode
#define w_stopval	w_S.w_Stopval
#define w_stopsig	w_S.w_Stopsig



#define WSTOPPED        0004    /* wait for processes stopped by signals */
#endif /* !defined(_POSIX_SOURCE) */

#include <sys/types.h>
#include <sys/cdefs.h>
__BEGIN_DECLS
pid_t   wait __P_((int *));
pid_t   waitpid __P_((pid_t, int *, int));
#ifndef _POSIX_SOURCE
pid_t   wait3 __P_((int *, int, void *));
pid_t   wait4 __P_((pid_t, int *, int, void *));
#endif

#endif /* __SYS_WAIT_H__ */
