#include <standards.h>

typedef int sig_atomic_t; /* accessable as an atomic entity (ANSI) */

/*
 * valid signal values: all undefined values are reserved for future use 
 * note: POSIX requires a value of 0 to be used as the null signal in kill()
 */
#define	SIGHUP	   1	/* hangup, generated when terminal disconnects */
#define	SIGINT	   2	/* interrupt, generated from terminal special char */
#define	SIGQUIT	   3	/* (*) quit, generated from terminal special char */
#define	SIGILL	   4	/* (*) illegal instruction (not reset when caught)*/
#define	SIGTRAP	   5	/* (*) trace trap (not reset when caught) */
#define	SIGABRT    6	/* (*) abort process */
#define SIGEMT	   7	/* EMT instruction */
#define	SIGFPE	   8	/* (*) floating point exception */
#define	SIGKILL	   9	/* kill (cannot be caught or ignored) */
#define	SIGBUS	  10	/* (*) bus error (specification exception) */
#define	SIGSEGV	  11	/* (*) segmentation violation */
#define	SIGSYS	  12	/* (*) bad argument to system call */
#define	SIGPIPE	  13	/* write on a pipe with no one to read it */
#define	SIGALRM	  14	/* alarm clock timeout */
#define	SIGTERM	  15	/* software termination signal */
#define	SIGURG 	  16	/* (+) urgent contition on I/O channel */
#define	SIGSTOP	  17	/* (@) stop (cannot be caught or ignored) */
#define	SIGTSTP	  18	/* (@) interactive stop */
#define	SIGCONT	  19	/* (!) continue (cannot be caught or ignored) */
#define SIGCHLD   20	/* (+) sent to parent on child stop or exit */
#define SIGTTIN   21	/* (@) background read attempted from control terminal*/
#define SIGTTOU   22	/* (@) background write attempted to control terminal */
#define SIGIO	  23	/* (+) I/O possible, or completed */
#define SIGXCPU	  24	/* cpu time limit exceeded (see setrlimit()) */
#define SIGXFSZ	  25	/* file size limit exceeded (see setrlimit()) */
#define SIGVTALRM 26	/* virtual time alarm (see setitimer) */
#define SIGPROF   27	/* profiling time alarm (see setitimer) */
#define SIGWINCH  28	/* (+) window size changed */
#define SIGINFO   29    /* information request */
#define SIGUSR1   30	/* user defined signal 1 */
#define SIGUSR2   31	/* user defined signal 2 */
#define SIGMAX    31
#define NSIG      31

/*
 * additional signal names supplied for compatibility, only 
 */
#define SIGIOINT SIGURG	/* printer to backend error signal */
#define SIGAIO	SIGIO	/* base lan i/o */
#define SIGPTY  SIGIO	/* pty i/o */
#define	SIGPOLL	SIGIO	/* STREAMS version of this signal */
#define SIGIOT  SIGABRT /* abort (terminate) process */ 
#define	SIGLOST	SIGIOT	/* old BSD signal ?? */
#define SIGPWR  SIGINFO /* Power Fail/Restart -- SVID3/SVR4 */
#define SIGCLD	SIGCHLD

/*
 * valid signal action values; other values => pointer to handler function 
 */
#define	SIG_DFL		(void (*)())0
#define	SIG_IGN		(void (*)())1

/*
 * values of "how" argument to sigprocmask() call
 */
#define SIG_BLOCK	1
#define SIG_UNBLOCK	2
#define SIG_SETMASK	3

/*
 * sigaction structure used in sigaction() system call 
 * The order of the fields in this structure must match those in
 * the sigvec structure (below).
 */
struct sigaction {
	void	(*sa_handler)();	/* signal handler, or action value */
	sigset_t sa_mask;		/* signals to block while in handler */
	int	sa_flags;		/* signal action flags */
};

#define __SIGEMPTYSET       0
#define __SIGFILLSET        0xffffffff
#define __SIGADDSET(s, n)   ( *(s) |= 1L << ((n) - 1), 0)
#define __SIGDELSET(s, n)   ( *(s) &= ~(1L << ((n) - 1)), 0)
#define __SIGISMEMBER(s, n) ( (*(s) & (1L << ((n) - 1))) != (sigset_t)0)


#define SIGSTKSZ	(16384)
#define MINSIGSTKSZ	(4096)

/*
 * valid flags define for sa_flag field of sigaction structure 
 */
#define	SA_ONSTACK	0x00000001	/* run on special signal stack */
#define SA_RESTART	0x00000002	/* restart system calls on sigs */
#define SA_NOCLDSTOP    0x00000004  	/* do not set SIGCHLD for child stops*/
#define SA_NODEFER	0x00000008	/* don't block while handling */
#define SA_RESETHAND	0x00000010	/* old sys5 style behavior */
#define SA_NOCLDWAIT	0x00000020	/* no zombies */
#define SA_SIGINFO	0x00000040	/* deliver siginfo to handler */

/* This is for sys/time.h */
/* Removed for OSF1 V3.2
typedef union sigval {
    int     sival_int;
    void    *sival_ptr;
} sigval_t;
*/
