#include <features.h>
#include <linux/signal.h>

#ifndef SIGCLD
#define SIGCLD		SIGCHLD
#endif

typedef int sig_atomic_t;

typedef __sighandler_t	SignalHandler;

#define SignalBad		((SignalHandler)-1)
#define SignalDefault	((SignalHandler)0)
#define SignalIgnore	((SignalHandler)1)

#define	__sigmask(sig)		(1 << ((sig) - 1))
#define	sigmask				__sigmask

#define	__SIGFILLSET		0xffffffff
#define	__SIGEMPTYSET		0
#define	__SIGADDSET(s,n)	((*s) |= (__sigmask(n)))
#define	__SIGDELSET(s,n)	((*s) &= ~(__sigmask(n))) 
#define	__SIGISMEMBER(s,n)	((*s) & (__sigmask(n))) 

