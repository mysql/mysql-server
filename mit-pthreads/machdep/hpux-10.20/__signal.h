#include <sys/signal.h>

#ifndef SIGCLD
#define SIGCLD         SIGCHLD
#endif

/* #define sigmask(n)      	((unsigned int)1 << (((n) - 1) & (32 - 1))) */
#define sigword(n)      	(((unsigned int)((n) - 1))>>5)

#define __SIGEMPTYSET   	{ 0, 0, 0, 0, 0, 0, 0, 0 }
#define __SIGFILLSET   		{ 0xffffffff,0xffffffff,0xffffffff,0xffffffff,\
				  0xffffffff,0xffffffff,0xffffffff,0xffffffff}
#define __SIGADDSET(s, n)     	((s)->sigset[sigword(n)] |= sigmask(n))
#define __SIGDELSET(s, n)     	((s)->sigset[sigword(n)] &= ~sigmask(n))
#define __SIGISMEMBER(s, n)   	((s)->sigset[sigword(n)] & sigmask(n))

#define SIGSET_SIZE sizeof(sigset_t)/sizeof(long)

#define SIG_ANY(sig) sig_any(&sig)

static inline int sig_any(sigset_t *sig) {
  int i;
  for (i=0; i < SIGSET_SIZE; i++)
    if (sig->sigset[i] != 0)
      return 1;
  return 0;
}

