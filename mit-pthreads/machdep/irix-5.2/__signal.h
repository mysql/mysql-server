#include <sys/signal.h>

typedef int 	sig_atomic_t;

#ifndef sigmask
#define sigmask(n)      ((unsigned int)1 << (((n) - 1) & (32 - 1)))
#endif
#define sigword(n)      (((unsigned int)((n) - 1))>>5)

#define __SIGEMPTYSET 		 { 0, 0, 0, 0 };
#define __SIGFILLSET 		 { 0xffffffff,0xffffffff,0xffffffff,0xffffffff };
#define __SIGADDSET(s, n)     ((s)->sigbits[sigword(n)] |= sigmask(n))
#define __SIGDELSET(s, n)     ((s)->sigbits[sigword(n)] &= ~sigmask(n))
#define __SIGISMEMBER(s, n)   (sigmask(n) & (s)->sigbits[sigword(n)])

