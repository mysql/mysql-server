#ifndef	__sys_signal_h
#define	__sys_signal_h

#define	NSIG	32

/*
 * If any signal defines (SIG*) are added, deleted, or changed, the same
 * changes must be made in /usr/include/signal.h as well.
 */
#define	SIGHUP	1	/* hangup */
#define	SIGINT	2	/* interrupt */
#define	SIGQUIT	3	/* quit */
#define	SIGILL	4	/* illegal instruction (not reset when caught) */
#define	SIGTRAP	5	/* trace trap (not reset when caught) */
#define	SIGIOT	6	/* IOT instruction */
#define	SIGABRT 6	/* used by abort, replace SIGIOT in the future */
#define	SIGEMT	7	/* EMT instruction */
#define	SIGFPE	8	/* floating point exception */
#define	SIGKILL	9	/* kill (cannot be caught or ignored) */
#define	SIGBUS	10	/* bus error */
#define	SIGSEGV	11	/* segmentation violation */
#define	SIGSYS	12	/* bad argument to system call */
#define	SIGPIPE	13	/* write on a pipe with no one to read it */
#define	SIGALRM	14	/* alarm clock */
#define	SIGTERM	15	/* software termination signal from kill */
#define	SIGURG	16	/* urgent condition on IO channel */
#define	SIGSTOP	17	/* sendable stop signal not from tty */
#define	SIGTSTP	18	/* stop signal from tty */
#define	SIGCONT	19	/* continue a stopped process */
#define	SIGCHLD	20	/* to parent on child stop or exit */
#define	SIGCLD	20	/* System V name for SIGCHLD */
#define	SIGTTIN	21	/* to readers pgrp upon background tty read */
#define	SIGTTOU	22	/* like TTIN for output if (tp->t_local&LTOSTOP) */
#define	SIGIO	23	/* input/output possible signal */
#define	SIGPOLL	SIGIO	/* System V name for SIGIO */
#define	SIGXCPU	24	/* exceeded CPU time limit */
#define	SIGXFSZ	25	/* exceeded file size limit */
#define	SIGVTALRM 26	/* virtual time alarm */
#define	SIGPROF	27	/* profiling time alarm */
#define	SIGWINCH 28	/* window changed */
#define	SIGLOST 29	/* resource lost (eg, record-lock lost) */
#define	SIGUSR1 30	/* user defined signal 1 */
#define	SIGUSR2 31	/* user defined signal 2 */

struct	sigvec {
	void	(*sv_handler)();	/* signal handler */
	int	sv_mask;		/* signal mask to apply */
	int	sv_flags;		/* see signal options below */
};
#define	SV_ONSTACK	0x0001	/* take signal on signal stack */
#define	SV_INTERRUPT	0x0002	/* do not restart system on signal return */
#define	SV_RESETHAND	0x0004	/* reset signal handler to SIG_DFL when signal taken */
/*
 * If any SA_NOCLDSTOP or SV_NOCLDSTOP is change, the same
 * changes must be made in /usr/include/signal.h as well.
 */
#define	SV_NOCLDSTOP	0x0008	/* don't send a SIGCHLD on child stop */
#define	SA_ONSTACK	SV_ONSTACK
#define	SA_INTERRUPT	SV_INTERRUPT
#define	SA_RESETHAND	SV_RESETHAND

#define	SA_NOCLDSTOP	SV_NOCLDSTOP
#define	sv_onstack sv_flags	/* isn't compatibility wonderful! */

/*
 * If SIG_ERR, SIG_DFL, SIG_IGN, or SIG_HOLD are changed, the same changes
 * must be made in /usr/include/signal.h as well.
 */
#define	SIG_ERR		(void (*)())-1
#define	SIG_DFL		(void (*)())0
#define	SIG_IGN		(void (*)())1

/*
 * Macro for converting signal number to a mask suitable for sigblock().
 */
#define	sigmask(m)	(1 << ((m)-1))

/*
 * If SIG_BLOCK, SIG_UNBLOCK, or SIG_SETMASK are changed, the same changes
 * must be made in /usr/include/signal.h as well.
 */
#define	SIG_BLOCK		0x0001
#define	SIG_UNBLOCK		0x0002
#define	SIG_SETMASK		0x0004

/*
 * If changes are made to sigset_t or struct sigaction, the same changes
 * must be made in /usr/include/signal.h as well.
 */
#include <sys/stdtypes.h>

struct	sigaction {
	void 		(*sa_handler)();
	sigset_t	sa_mask;
	int		sa_flags;
};

#endif	/* !__sys_signal_h */
