
#include <sys/signal.h>
#include <sys/stdtypes.h>

typedef int sig_atomic_t;

#define __SIGFILLSET        0xffffffff
#define __SIGEMPTYSET       0
#define __SIGADDSET(s,n)    ((*s) |= (1 << ((n) - 1)))
#define __SIGDELSET(s,n)    ((*s) &= ~(1 << ((n) - 1)))
#define __SIGISMEMBER(s,n)  ((*s) & (1 << ((n) - 1)))
