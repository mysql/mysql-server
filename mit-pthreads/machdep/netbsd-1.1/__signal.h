#include <sys/signal.h>

__BEGIN_DECLS

#if NSIG <= 32
#define	__SIGEMPTYSET		0
#define	__SIGFILLSET		0xffffffff
#define	__SIGADDSET(s, n)	(*(s) |= 1 << ((n) - 1), 0)
#define	__SIGDELSET(s, n)	(*(s) &= ~(1 << ((n) - 1)), 0)
#define	__SIGISMEMBER(s, n)	((*(s) & (1 << ((n) - 1))) != 0)

#else	/* XXX Netbsd >= 1.3H */

int	sigaction __P_((int, const struct sigaction *, struct sigaction *)) __RENAME(__sigaction14);

#define	__SIGEMPTYSET		{ 0, 0, 0, 0}
#define	__SIGFILLSET		{ 0xffffffff, 0xffffffff, \
				  0xffffffff, 0xffffffff }
#define __SIGMASK(n)		(1 << (((n) - 1) & 31))
#define	__SIGWORD(n)		(((n) - 1) >> 5)
#define	__SIGADDSET(s, n)	((s)->__bits[__SIGWORD(n)] |= __SIGMASK(n))
#define	__SIGDELSET(s, n)	((s)->__bits[__SIGWORD(n)] &= ~__SIGMASK(n))
#define	__SIGISMEMBER(s, n)	(((s)->__bits[__SIGWORD(n)] & __SIGMASK(n)) != 0)

#endif

__END_DECLS
