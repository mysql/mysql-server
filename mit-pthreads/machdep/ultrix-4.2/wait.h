
#ifndef _SYS_WAIT_H_
#define _SYS_WAIT_H_

#include <ansi_compat.h>
#include <sys/cdefs.h>

#if !defined(_POSIX_SOURCE)
union wait {
#else
union __wait	{
#endif /* !defined(_POSIX_SOURCE) */
#ifdef __vax
	int	w_status;		/* used in syscall */
#endif /* __vax */
#ifdef __mips__
	unsigned int	w_status;		/* used in syscall */
#endif /* __mips */
	/*
	 * Terminated process status.
	 */
	struct {
#ifdef __vax
		unsigned short	w_Termsig:7;	/* termination signal */
		unsigned short	w_Coredump:1;	/* core dump indicator */
		unsigned short	w_Retcode:8;	/* exit code if w_termsig==0 */
#endif /* __vax */
#ifdef __mips__
#ifdef __MIPSEL__
		unsigned int	w_Termsig:7;	/* termination signal */
		unsigned int	w_Coredump:1;	/* core dump indicator */
		unsigned int	w_Retcode:8;	/* exit code if w_termsig==0 */
		unsigned int	w_Filler:16;	/* pad to word boundary */
#endif /* __MIPSEL */
#ifdef __MIPSEB__
		unsigned int	w_Filler:16;	/* pad to word boundary */
		unsigned int	w_Retcode:8;	/* exit code if w_termsig==0 */
		unsigned int	w_Coredump:1;	/* core dump indicator */
		unsigned int	w_Termsig:7;	/* termination signal */
#endif /* __MIPSEB */
#endif /* __mips */
	} w_T;
	/*
	 * Stopped process status.  Returned
	 * only for traced children unless requested
	 * with the WUNTRACED option bit.
	 */
	struct {
#ifdef __vax
		unsigned short	w_Stopval:8;	/* == W_STOPPED if stopped */
		unsigned short	w_Stopsig:8;	/* signal that stopped us */
#endif /* __vax */
#ifdef __mips__
#ifdef __MIPSEL__
		unsigned int	w_Stopval:8;	/* == W_STOPPED if stopped */
		unsigned int	w_Stopsig:8;	/* signal that stopped us */
		unsigned int	w_Filler:16;	/* pad to word boundary */
#endif /* __MIPSEL */
#ifdef __MIPSEB__
		unsigned int	w_Filler:16;	/* pad to word boundary */
		unsigned int	w_Stopsig:8;	/* signal that stopped us */
		unsigned int	w_Stopval:8;	/* == W_STOPPED if stopped */
#endif /* __MIPSEB */
#endif /* __mips */
	} w_S;
};

#if !defined(_POSIX_SOURCE)
#define	w_termsig	w_T.w_Termsig
#define w_coredump	w_T.w_Coredump
#define w_retcode	w_T.w_Retcode
#define w_stopval	w_S.w_Stopval
#define w_stopsig	w_S.w_Stopsig
#define	WSTOPPED	0177	/* value of s.stopval if process is stopped */
#endif /* !defined(_POSIX_SOURCE) */

#ifdef  WSTOPPED
#define _WSTOPPED	WSTOPPED
#else
#define _WSTOPPED	0177
#endif

/*
 * Option bits for the second argument of wait3.  WNOHANG causes the
 * wait to not hang if there are no stopped or terminated processes, rather
 * returning an error indication in this case (pid==0).  WUNTRACED
 * indicates that the caller should receive status about untraced children
 * which stop due to signals.  If children are stopped and a wait without
 * this option is done, it is as though they were still running... nothing
 * about them is returned.
 */
#define WNOHANG		1	/* dont hang in wait */
#define WUNTRACED	2	/* tell about stopped, untraced children */

/*
 * Must cast as union wait * because POSIX defines the input to these macros
 * as int.
 */

#ifdef _POSIX_SOURCE
#define WIFSTOPPED(x)	(((union __wait *)&(x))->w_S.w_Stopval == _WSTOPPED)
#define WIFSIGNALED(x)	(((union __wait *)&(x))->w_S.w_Stopval != _WSTOPPED && ((union __wait *)&(x))->w_T.w_Termsig != 0)
#define WIFEXITED(x)	(((union __wait *)&(x))->w_S.w_Stopval != _WSTOPPED && ((union __wait *)&(x))->w_T.w_Termsig == 0)
#define	WEXITSTATUS(x)	(((union __wait *)&(x))->w_T.w_Retcode)
#define	WTERMSIG(x)	(((union __wait *)&(x))->w_T.w_Termsig)
#define	WSTOPSIG(x)	(((union __wait *)&(x))->w_S.w_Stopsig)
#endif /* _POSIX_SOURCE */

#if !defined(_POSIX_SOURCE)
#define WIFSTOPPED(x)	(((union wait *)&(x))->w_stopval == WSTOPPED)
#define WIFSIGNALED(x)	(((union wait *)&(x))->w_stopval != WSTOPPED && ((union wait *)&(x))->w_termsig != 0)
#define WIFEXITED(x)	(((union wait *)&(x))->w_stopval != WSTOPPED && ((union wait *)&(x))->w_termsig == 0)
#define	WEXITSTATUS(x)	(((union wait *)&(x))->w_retcode)
#define	WTERMSIG(x)	(((union wait *)&(x))->w_termsig)
#define	WSTOPSIG(x)	(((union wait *)&(x))->w_stopsig)
#endif /* !defined(_POSIX_SOURCE) */

pid_t wait 					__P_((int *));
pid_t waitpid 				__P_((pid_t, int *, int));

#endif /* _SYS_WAIT_H_ */
