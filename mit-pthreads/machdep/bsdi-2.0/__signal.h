#include <sys/signal.h>

#define	__SIGEMPTYSET		0
#define	__SIGFILLSET		0xffffffff
#define	__SIGADDSET(s, n)	(*(s) |= 1 << ((n) - 1), 0)
#define	__SIGDELSET(s, n)	(*(s) &= ~(1 << ((n) - 1)), 0)
#define	__SIGISMEMBER(s, n)	((*(s) & (1 << ((n) - 1))) != 0)
