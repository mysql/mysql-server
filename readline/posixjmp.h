/* posixjmp.h -- wrapper for setjmp.h with changes for POSIX systems. */

#ifndef _POSIXJMP_H_
#define _POSIXJMP_H_

#include <setjmp.h>

/* This *must* be included *after* config.h */

#if defined (HAVE_POSIX_SIGSETJMP)
#  define procenv_t	sigjmp_buf
#  if !defined (__OPENNT)
#    undef setjmp
#    define setjmp(x)	sigsetjmp((x), 1)
#    undef longjmp
#    define longjmp(x, n)	siglongjmp((x), (n))
#  endif /* !__OPENNT */
#else
#  define procenv_t	jmp_buf
#endif

#endif /* _POSIXJMP_H_ */
